
#ifndef CHARON_AVALANCHE_OKUTO_IMPL_HPP
#define CHARON_AVALANCHE_OKUTO_IMPL_HPP

#include <cmath>
#include "Panzer_Workset.hpp"
#include "Panzer_Workset_Utilities.hpp"
#include "Panzer_IntegrationRule.hpp"

#include "Charon_Material_Properties.hpp"
#include "Charon_Physical_Constants.hpp"
#include "Charon_Names.hpp"


/*
The Okuto-Crowell avalanche model is by Y. Okuto and C. R. Crowell, "Threshold
Energy Effect on Avalanche Breakdown Voltage in Semiconductor Junctions,"
Solid-State Electronics, vol.18, no.2, pp.161-168, 1975.

The ionization coefficient takes the following form:

alpha(F) = a*[1+c*(T-Tref)] * F^gamma * exp[-(b*[1+d*(T-Tref)]/F)^delta],

where a, b, c, d, gamma, and delta are model parameters whose default values are
given in charon::Material_Properties. The default values are good in Silicon for
the range of electric field from 1e5 V/cm to 1e6 V/cm. F is the driving field,
i.e., GradQuasiFermi, EParallelJ, EParallelJtot, EeffParallelJ, or EeffParallelJtot.
Eeff is computed by charon::FEM_ElectricField.

For numerical reasons, the field F is computed as n*Fn/(n+n0) for electrons,
and p*Fp/(p+p0) for holes. n0 and p0 are damping parameters and default to 0 and
can be set in the Avalanche Generation section through eDrForceRefDens and hDrForceRefDens 
respectively.
Using positive values for n0 and p0 can improve convergence for problems where
strong generation–recombination occurs in regions with small density. 
*/


namespace charon {

///////////////////////////////////////////////////////////////////////////////
//
//  Constructor
//
///////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
Avalanche_Okuto<EvalT, Traits>::
Avalanche_Okuto(
  const Teuchos::ParameterList& p)
{
  using std::string;
  using Teuchos::RCP;
  using PHX::DataLayout;
  using PHX::MDField;
  using panzer::IntegrationRule;
  using Teuchos::ParameterList;

  RCP<ParameterList> valid_params = this->getValidParameters();
  p.validateParameters(*valid_params);

  const charon::Names& n = *(p.get< RCP<const charon::Names> >("Names"));

  // Retrieve data layouts
  RCP<DataLayout> scalar = p.get< RCP<DataLayout> >("Scalar Data Layout");
  RCP<DataLayout> vector = p.get< RCP<DataLayout> >("Vector Data Layout");
  num_points = vector->dimension(1);
  num_dims = vector->dimension(2);

  // Material name
  const string& materialName = p.get<string>("Material Name");

  // Obtain the instance of charon::Material_Properties.
  charon::Material_Properties& matProperty = charon::Material_Properties::getInstance();

  // Retrieve Okuto model parameters
  a_e = matProperty.getPropertyValue(materialName, "Okuto Electron a");
  b_e = matProperty.getPropertyValue(materialName, "Okuto Electron b");
  c_e = matProperty.getPropertyValue(materialName, "Okuto Electorn c");
  d_e = matProperty.getPropertyValue(materialName, "Okuto Electorn d");
  gamma_e = matProperty.getPropertyValue(materialName, "Okuto Electorn gamma");
  delta_e = matProperty.getPropertyValue(materialName, "Okuto Electorn delta");

  a_h = matProperty.getPropertyValue(materialName, "Okuto Hole a");
  b_h = matProperty.getPropertyValue(materialName, "Okuto Hole b");
  c_h = matProperty.getPropertyValue(materialName, "Okuto Hole c");
  d_h = matProperty.getPropertyValue(materialName, "Okuto Hole d");
  gamma_h = matProperty.getPropertyValue(materialName, "Okuto Hole gamma");
  delta_h = matProperty.getPropertyValue(materialName, "Okuto Hole delta");

  // Avalanche ParameterList
  const ParameterList& avaParamList = p.sublist("Avalanche ParameterList");

  // Overwrite parameters when specified by users
  if (avaParamList.isParameter("a_e"))
    a_e = avaParamList.get<double>("a_e");
  if (avaParamList.isParameter("b_e"))
    b_e = avaParamList.get<double>("b_e");
  if (avaParamList.isParameter("c_e"))
    c_e = avaParamList.get<double>("c_e");
  if (avaParamList.isParameter("d_e"))
    d_e = avaParamList.get<double>("d_e");
  if (avaParamList.isParameter("gamma_e"))
    gamma_e = avaParamList.get<double>("gamma_e");
  if (avaParamList.isParameter("delta_e"))
    delta_e = avaParamList.get<double>("delta_e");

  if (avaParamList.isParameter("a_h"))
    a_h = avaParamList.get<double>("a_h");
  if (avaParamList.isParameter("b_h"))
    b_h = avaParamList.get<double>("b_h");
  if (avaParamList.isParameter("c_h"))
    c_h = avaParamList.get<double>("c_h");
  if (avaParamList.isParameter("d_h"))
    d_h = avaParamList.get<double>("d_h");
  if (avaParamList.isParameter("gamma_h"))
    gamma_h = avaParamList.get<double>("gamma_h");
  if (avaParamList.isParameter("delta_h"))
    delta_h = avaParamList.get<double>("delta_h");

  // Set minimum field below which avalanche generation = 0
  minField = 5e4;  // default, [V/cm]
  if (avaParamList.isParameter("Minimum Field"))
    minField = avaParamList.get<double>("Minimum Field");

  // Driving Force = GradQuasiFermi, GradPotentialParallelJ, GradPotentialParallelJtot,
  // EffectiveFieldParallelJ, or EffectiveFieldParallelJtot.
  driveForce = "GradQuasiFermi" ;   // default
  if (avaParamList.isParameter("Driving Force"))
    driveForce = avaParamList.get<string>("Driving Force");

  // Set driving force damping parameters
  eDrForceRefDens = 0.0;  // default, using positive value might improve convergence
  hDrForceRefDens = 0.0;
  if (avaParamList.isParameter("eDrForceRefDens"))
    eDrForceRefDens = avaParamList.get<double>("eDrForceRefDens");
  if (avaParamList.isParameter("hDrForceRefDens"))
    hDrForceRefDens = avaParamList.get<double>("hDrForceRefDens");

  // Define and add dependent fields according to driveForce
  if (driveForce == "GradQuasiFermi")
  {
    grad_qfp_e = MDField<const ScalarT,Cell,Point,Dim>(n.field.elec_grad_qfp,vector);
    grad_qfp_h = MDField<const ScalarT,Cell,Point,Dim>(n.field.hole_grad_qfp,vector);
    this->addDependentField(grad_qfp_e);
    this->addDependentField(grad_qfp_h);
  }
  else if ( (driveForce == "GradPotentialParallelJ") || (driveForce == "GradPotentialParallelJtot") )
  {
    grad_pot = MDField<const ScalarT,Cell,Point,Dim>(n.grad_dof.phi,vector);
    this->addDependentField(grad_pot);
  }
  else if ( (driveForce == "EffectiveFieldParallelJ") || (driveForce == "EffectiveFieldParallelJtot") )
  {
    // these fields are always used (see below) because the derivates need to
    // be computed

//    eff_field_e = MDField<const ScalarT,Cell,Point,Dim>(n.field.elec_efield,vector);
//    eff_field_h = MDField<const ScalarT,Cell,Point,Dim>(n.field.hole_efield,vector);
//    this->addDependentField(eff_field_e);
//    this->addDependentField(eff_field_h);
  }
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Error: Invalid Driving Force !");

  // Always need current densities and carrier densities
  curr_dens_e = MDField<const ScalarT,Cell,Point,Dim>(n.field.elec_curr_density,vector);
  curr_dens_h = MDField<const ScalarT,Cell,Point,Dim>(n.field.hole_curr_density,vector);
  dens_e = MDField<const ScalarT,Cell,Point>(n.dof.edensity,scalar);
  dens_h = MDField<const ScalarT,Cell,Point>(n.dof.hdensity,scalar);

  this->addDependentField(curr_dens_e);
  this->addDependentField(curr_dens_h);
  this->addDependentField(dens_e);
  this->addDependentField(dens_h);

  // Scaling parameters
  scaleParams = p.get< RCP<charon::Scaling_Parameters> >("Scaling Parameters");
  J0 = scaleParams->scale_params.J0;
  R0 = scaleParams->scale_params.R0;
  E0 = scaleParams->scale_params.E0;
  C0 = scaleParams->scale_params.C0;
  T0 = scaleParams->scale_params.T0;

  // Add other dependent fields
  latt_temp = MDField<const ScalarT,Cell,Point>(n.field.latt_temp,scalar);
  this->addDependentField(latt_temp);

  // Always need the following additional fields for computing derivatives
  eff_field_e = MDField<const ScalarT,Cell,Point,Dim>(n.field.elec_efield,vector);
  eff_field_h = MDField<const ScalarT,Cell,Point,Dim>(n.field.hole_efield,vector);
  grad_dens_e = MDField<const ScalarT,Cell,Point,Dim>(n.grad_dof.edensity,vector);
  grad_dens_h = MDField<const ScalarT,Cell,Point,Dim>(n.grad_dof.hdensity,vector);

  diff_coeff_e = MDField<const ScalarT,Cell,Point>(n.field.elec_diff_coeff,scalar);
  diff_coeff_h = MDField<const ScalarT,Cell,Point>(n.field.hole_diff_coeff,scalar);
  mobility_e = MDField<const ScalarT,Cell,Point>(n.field.elec_mobility,scalar);
  mobility_h = MDField<const ScalarT,Cell,Point>(n.field.hole_mobility,scalar);

  this->addDependentField(eff_field_e);
  this->addDependentField(eff_field_h);
  this->addDependentField(grad_dens_e);
  this->addDependentField(grad_dens_h);
  this->addDependentField(diff_coeff_e);
  this->addDependentField(diff_coeff_h);
  this->addDependentField(mobility_e);
  this->addDependentField(mobility_h);

  // Add evaluated field
  avalanche_rate = MDField<ScalarT,Cell,Point>(n.field.avalanche_rate,scalar);
  ava_deriv_e = MDField<ScalarT,Cell,Point>(n.field.ava_deriv_e,scalar);
  ava_deriv_h = MDField<ScalarT,Cell,Point>(n.field.ava_deriv_h,scalar);

  this->addEvaluatedField(avalanche_rate);
  this->addEvaluatedField(ava_deriv_e);
  this->addEvaluatedField(ava_deriv_h);

  std::string name = "Okuto_Avalanche";
  this->setName(name);
}


///////////////////////////////////////////////////////////////////////////////
//
//  evaluateFields()
//
///////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
void
Avalanche_Okuto<EvalT, Traits>::
evaluateFields(
  typename Traits::EvalData workset)
{
  using panzer::index_t;

  // Scale the damping parameters
  ScalarT scaled_eRefDens = eDrForceRefDens / C0;
  ScalarT scaled_hRefDens = hDrForceRefDens / C0;

  // Reference temperature [K]
  double refT = 300.0;

  // Retrieve electron charge in [C]
  charon::PhysicalConstants const& cpc = charon::PhysicalConstants::Instance();
  double eleQ = cpc.q;

  // Avalanche generate rate scaling
  ScalarT scaling = J0 / (eleQ * R0);

  // Loop over cells
  for (index_t cell = 0; cell < workset.num_cells; ++cell)
  {
    for (int point = 0; point < num_points; ++point)
    {
      // Obtain temperature
      ScalarT lattT = latt_temp(cell,point)*T0;  // lattice temperature [K]

      // Obtain carrier density
      const ScalarT& eden = dens_e(cell,point);
      const ScalarT& hden = dens_h(cell,point);

      ScalarT Fe = 0.0;        // driving field
      ScalarT Fh = 0.0;

      // ----------------------------------------------------------------------
      // --- Driving field calculation is the same for all avalanche models ---
      // ----------------------------------------------------------------------

      // Compute absolute values of Electron and Hole GradQuasiFermiPotential
      if (driveForce == "GradQuasiFermi")
      {
        ScalarT egradqfp = 0.0;
        ScalarT hgradqfp = 0.0;
        for (int dim = 0; dim < num_dims; ++dim)
        {
          const ScalarT& gqfpe = grad_qfp_e(cell,point,dim);
          const ScalarT& gqfph = grad_qfp_h(cell,point,dim);

          egradqfp += gqfpe * gqfpe;
          hgradqfp += gqfph * gqfph;
        }
        // The density term is for damping to obtain better convergence in some cases
        Fe = std::sqrt(egradqfp) * E0 * eden/(eden + scaled_eRefDens);  // in [V/cm]
        Fh = std::sqrt(hgradqfp) * E0 * hden/(hden + scaled_hRefDens);
        // std::cout << "Fe = " << Fe << ", Fh = " << Fh << std::endl;
      }

      // Compute projection of negative gradient of potential along carrier current density
      else if (driveForce == "GradPotentialParallelJ")
      {
        ScalarT normJe = 0.0;    // absolute value of Je
        ScalarT normJh = 0.0;
        ScalarT ngpdotJe = 0.0;  // -grad(phi) \dot Je
        ScalarT ngpdotJh = 0.0;
        for (int dim = 0; dim < num_dims; ++dim)
        {
          const ScalarT& Je = curr_dens_e(cell,point,dim);
          const ScalarT& Jh = curr_dens_h(cell,point,dim);
          const ScalarT& gp = grad_pot(cell,point,dim);

          normJe += Je * Je;
          normJh += Jh * Jh;
          ngpdotJe += -gp * Je;
          ngpdotJh += -gp * Jh;
        }
        normJe = std::sqrt(normJe);      // note: Je/normJe has no unit.
        normJh = std::sqrt(normJh);
        Fe = ngpdotJe / normJe * E0 * eden/(eden + scaled_eRefDens);
        Fh = ngpdotJh / normJh * E0 * hden/(hden + scaled_hRefDens);
      }

      // Compute projection of negative gradient of potential along total current density
      else if (driveForce == "GradPotentialParallelJtot")
      {
        ScalarT normJtot = 0.0;
        ScalarT ngpdotJtot = 0.0;
        for (int dim = 0; dim < num_dims; ++dim)
        {
          const ScalarT& Je = curr_dens_e(cell,point,dim);
          const ScalarT& Jh = curr_dens_h(cell,point,dim);
          const ScalarT& gp = grad_pot(cell,point,dim);

          ScalarT Jtot = Je + Jh;
          normJtot += Jtot * Jtot;
          ngpdotJtot += -gp * Jtot;
        }
        normJtot = std::sqrt(normJtot);
        Fe = ngpdotJtot / normJtot * E0 * eden/(eden + scaled_eRefDens);
        Fh = ngpdotJtot / normJtot * E0 * hden/(hden + scaled_hRefDens);
      }

      // Compute projection of effective electric field along carrier current density
      else if (driveForce == "EffectiveFieldParallelJ")
      {
        ScalarT normJe = 0.0;      // absolute value of Je
        ScalarT normJh = 0.0;
        ScalarT eEeffdotJe = 0.0;  // eEeff \dot Je
        ScalarT hEeffdotJh = 0.0;
        for (int dim = 0; dim < num_dims; ++dim)
        {
          const ScalarT& Je = curr_dens_e(cell,point,dim);
          const ScalarT& Jh = curr_dens_h(cell,point,dim);
          const ScalarT& eEeff = eff_field_e(cell,point,dim);
          const ScalarT& hEeff = eff_field_h(cell,point,dim);

          normJe += Je * Je;
          normJh += Jh * Jh;
          eEeffdotJe += eEeff * Je;
          hEeffdotJh += hEeff * Jh;
        }
        normJe = std::sqrt(normJe);
        normJh = std::sqrt(normJh);
        Fe = eEeffdotJe / normJe * E0 * eden/(eden + scaled_eRefDens);
        Fh = hEeffdotJh / normJh * E0 * hden/(hden + scaled_hRefDens);
      }

      // Compute projection of effective electric field along total current density
      else if (driveForce == "EffectiveFieldParallelJtot")
      {
        ScalarT normJtot = 0.0;
        ScalarT eEeffdotJtot = 0.0;  // eEeff \dot Jtot
        ScalarT hEeffdotJtot = 0.0;
        for (int dim = 0; dim < num_dims; ++dim)
        {
          const ScalarT& Je = curr_dens_e(cell,point,dim);
          const ScalarT& Jh = curr_dens_h(cell,point,dim);
          const ScalarT& eEeff = eff_field_e(cell,point,dim);
          const ScalarT& hEeff = eff_field_h(cell,point,dim);

          ScalarT Jtot = Je + Jh;
          normJtot += Jtot * Jtot;
          eEeffdotJtot += eEeff * Jtot;
          hEeffdotJtot += hEeff * Jtot;
        }
        normJtot = std::sqrt(normJtot);
        Fe = eEeffdotJtot / normJtot * E0 * eden/(eden + scaled_eRefDens);
        Fh = hEeffdotJtot / normJtot * E0 * hden/(hden + scaled_hRefDens);
      }

      // Take the absolute values
      Fe = std::abs(Fe);
      Fh = std::abs(Fh);

      // end of driving field calculation -------------------------------------

      // Compute absolute values of current densities
      ScalarT normJe = 0.0;
      ScalarT normJh = 0.0;
      for (int dim = 0; dim < num_dims; ++dim)
      {
        const ScalarT& Je = curr_dens_e(cell,point,dim);
        const ScalarT& Jh = curr_dens_h(cell,point,dim);
        normJe += Je * Je;
        normJh += Jh * Jh;
      }
      normJe = std::sqrt(normJe);
      normJh = std::sqrt(normJh);

      // -----------------------------------------------------------------------
      // --- Ionization coefficients calculation depends on avalanche models ---
      // -----------------------------------------------------------------------

      // Compute impace ionization coefficients [1/cm]
      ScalarT iicoeffe = 0.0;
      ScalarT iicoeffh = 0.0;
      if (Fe > minField)
      {
        ScalarT exe = exp(-pow(b_e*(1.+d_e*(lattT-refT))/Fe, delta_e));
        iicoeffe = a_e * (1.+c_e*(lattT-refT)) * pow(Fe,gamma_e) * exe;
      }
      if (Fh > minField)
      {
        ScalarT exh = exp(-pow(b_h*(1.+d_h*(lattT-refT))/Fh, delta_h));
        iicoeffh = a_h * (1.+c_h*(lattT-refT)) * pow(Fh,gamma_h) * exh;
      }

      // Compute avalanche generate rate (scaled)
      ScalarT eAvaRate = 0.0;
      ScalarT hAvaRate = 0.0;
      if (normJe > 0.)  eAvaRate = iicoeffe*normJe;
      if (normJh > 0.)  hAvaRate = iicoeffh*normJh;
      avalanche_rate(cell,point) = (eAvaRate + hAvaRate)*scaling;

      // -----------------------------------------------------------------------
      // --- Avalanche gen. derivative w.r.t. eden and hden respectively -------
      // -----------------------------------------------------------------------

      // Calculate grad(n) \dot En, |En|^2, grad(p) \dot Ep, and |Ep|^2
      ScalarT gcdotEeffe = 0.0;
      ScalarT gcdotEeffh = 0.0;
      ScalarT normEeffe2 = 0.0;
      ScalarT normEeffh2 = 0.0;
      for (int dim = 0; dim < num_dims; ++dim)
      {
        const ScalarT& grad_eden = grad_dens_e(cell,point,dim);
        const ScalarT& grad_hden = grad_dens_h(cell,point,dim);
        const ScalarT& eEeff = eff_field_e(cell,point,dim);
        const ScalarT& hEeff = eff_field_h(cell,point,dim);
        gcdotEeffe += grad_eden * eEeff;
        gcdotEeffh += grad_hden * hEeff;
        normEeffe2 += eEeff * eEeff;
        normEeffh2 += hEeff * hEeff;
      }

      const ScalarT& ediff = diff_coeff_e(cell,point);
      const ScalarT& hdiff = diff_coeff_h(cell,point);
      const ScalarT& emob = mobility_e(cell,point);
      const ScalarT& hmob = mobility_h(cell,point);

      // Compute avalanche derivative
      if (normJe > 0.)
        ava_deriv_e(cell,point) = iicoeffe / normJe * (ediff*emob*gcdotEeffe
                                + eden*emob*emob*normEeffe2) * scaling;
      else
        ava_deriv_e(cell,point) = 0.0;
      if (normJh > 0.)
        ava_deriv_h(cell,point) = iicoeffh / normJh * (-hdiff*hmob*gcdotEeffh
                                + hden*hmob*hmob*normEeffh2) * scaling;
      else
        ava_deriv_h(cell,point) = 0.0;

    }  // end of loop over point
  }  // end of loop over cell

}


///////////////////////////////////////////////////////////////////////////////
//
//  getValidParameters()
//
///////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
Teuchos::RCP<Teuchos::ParameterList>
Avalanche_Okuto<EvalT, Traits>::getValidParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> p = Teuchos::rcp(new Teuchos::ParameterList);
  p->set<std::string>("Material Name", "?");

  Teuchos::RCP<const charon::Names> n;
  p->set("Names", n);

  Teuchos::RCP<PHX::DataLayout> scalar_dl;
  p->set("Scalar Data Layout", scalar_dl);

  Teuchos::RCP<PHX::DataLayout> vector_dl;
  p->set("Vector Data Layout", vector_dl);

  p->sublist("Avalanche ParameterList", false, "");
  p->sublist("Avalanche ParameterList").set<std::string>("Value", "Okuto", "Use the Okuto-Crowell avalanche model");
  p->sublist("Avalanche ParameterList").set<std::string>("Driving Force", "GradQuasiFermi", "Specify the driving force");
  p->sublist("Avalanche ParameterList").set<double>("eDrForceRefDens", 0., "[cm^-3]");
  p->sublist("Avalanche ParameterList").set<double>("hDrForceRefDens", 0., "[cm^-3]");
  p->sublist("Avalanche ParameterList").set<double>("Minimum Field", 0., "[V/cm]");

  p->sublist("Avalanche ParameterList").set<double>("a_e", 0., "[1/V]");
  p->sublist("Avalanche ParameterList").set<double>("b_e", 0., "[V/cm]");
  p->sublist("Avalanche ParameterList").set<double>("c_e", 0., "[1/K]");
  p->sublist("Avalanche ParameterList").set<double>("d_e", 0., "[1/K]");
  p->sublist("Avalanche ParameterList").set<double>("gamma_e", 0., "[1]");
  p->sublist("Avalanche ParameterList").set<double>("delta_e", 0., "[1]");

  p->sublist("Avalanche ParameterList").set<double>("a_h", 0., "[1/V]");
  p->sublist("Avalanche ParameterList").set<double>("b_h", 0., "[V/cm]");
  p->sublist("Avalanche ParameterList").set<double>("c_h", 0., "[1/K]");
  p->sublist("Avalanche ParameterList").set<double>("d_h", 0., "[1/K]");
  p->sublist("Avalanche ParameterList").set<double>("gamma_h", 0., "[1]");
  p->sublist("Avalanche ParameterList").set<double>("delta_h", 0., "[1]");

  Teuchos::RCP<charon::Scaling_Parameters> sp;
  p->set("Scaling Parameters", sp);

  return p;
}

}

#endif
