
#ifndef CHARON_EQUATIONSET_DDIONLATTICE_IMPL_HPP
#define CHARON_EQUATIONSET_DDIONLATTICE_IMPL_HPP

#include "Teuchos_ParameterList.hpp"
#include "Teuchos_StandardParameterEntryValidators.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_Assert.hpp"
#include "Teuchos_TestForException.hpp"

#include "Phalanx_DataLayout_MDALayout.hpp"
#include "Phalanx_FieldManager.hpp"

#include "Panzer_IntegrationRule.hpp"
#include "Panzer_BasisIRLayout.hpp"

// include evaluators here
#include "Panzer_DOF.hpp"
#include "Panzer_DOFGradient.hpp"
#include "Panzer_Integrator_BasisTimesScalar.hpp"
#include "Panzer_Integrator_GradBasisDotVector.hpp"
#include "Panzer_Sum.hpp"

#include "Charon_Material_Properties.hpp"
#include "Charon_Scaling_Parameters.hpp"
#include "Charon_PotentialFlux.hpp"
#include "Charon_PoissonSource.hpp"

// FEM SUPG-related evaluators
#include "Charon_SUPG_Peclet.hpp"
#include "Charon_PDE_Residual_DD.hpp"
#include "Charon_SUPG_Tau_Linear.hpp"
#include "Charon_SUPG_Tau_Tanh.hpp"

// FEM vector field evaluators
#include "Charon_DDLattice_ElectricField.hpp"
#include "Charon_DDLattice_CurrentDensity.hpp"
#include "Charon_DDLattice_HeatGeneration.hpp"
#include "Charon_DDLattice_VacuumPotential.hpp"
#include "Charon_FEM_Velocity.hpp"
#include "Charon_Effective_Velocity.hpp"

// Symmetrized EFFPG stabilization residual
#include "Charon_SymEFFPG_Stab_Residual.hpp"
#include "Charon_EFFPG_DDIonLattice_CurrentDensity.hpp"


// SUPG-FEM and sym-EFFPG-FEM formulation of the Poisson+DD+Ion+Lattice equations

// ***********************************************************************
template <typename EvalT>
charon::EquationSet_DDIonLattice<EvalT>::
EquationSet_DDIonLattice(const Teuchos::RCP<Teuchos::ParameterList>& params,
                         const int& default_integration_order,
                         const panzer::CellData& cell_data,
                         const Teuchos::RCP<panzer::GlobalData>& global_data,
                         const bool build_transient_support, const std::string& dummy) :
  charon::EquationSet_DefaultImpl<EvalT>(params, default_integration_order, cell_data, global_data, build_transient_support)
{
}

// ***********************************************************************
template <typename EvalT>
charon::EquationSet_DDIonLattice<EvalT>::
EquationSet_DDIonLattice(const Teuchos::RCP<Teuchos::ParameterList>& params,
                         const int& default_integration_order,
                         const panzer::CellData& cell_data,
                         const Teuchos::RCP<panzer::GlobalData>& global_data,
                         const bool build_transient_support) :
  charon::EquationSet_DefaultImpl<EvalT>(params, default_integration_order, cell_data, global_data, build_transient_support)
{
  // ********************
  // Options
  // ********************
  {
    Teuchos::ParameterList valid_parameters;

    this->setDefaultValidParameters(valid_parameters);

    valid_parameters.set("Model ID","","Closure model id associated with this equation set");
    valid_parameters.set("Prefix","","Prefix for using multiple instantiations of the equation set");
    valid_parameters.set("Discontinuous Fields","","List of fields which are discontinuous");
    valid_parameters.set("Discontinuous Suffix","","Suffix for enabling discontinuous fields");
    valid_parameters.set("Basis Type","HGrad","Type of Basis to use");
    valid_parameters.set("Basis Order",1,"Order of the basis");
    valid_parameters.set("Integration Order",default_integration_order,"Order of the integration rule");

    Teuchos::ParameterList& opt = valid_parameters.sublist("Options");
    opt.set<int>("Ion Charge", 1, "Integer number of ion charge");

    Teuchos::setStringToIntegralParameter<int>(
      "Solve Electron",
      "True",
      "Determine if users want to solve the Electron Continuity equation",
      Teuchos::tuple<std::string>("True", "False"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Solve Hole",
      "True",
      "Determine if users want to solve the Hole Continuity equation",
      Teuchos::tuple<std::string>("True", "False"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Solve Ion",
      "True",
      "Determine if users want to solve the Ion Continuity equation",
      Teuchos::tuple<std::string>("True", "False"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Fermi Dirac",
      "False",
      "Determine if users want to use the Fermi-Dirac statistics",
      Teuchos::tuple<std::string>("True","False"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "SRH",
      "Off",
      "Determine if users want to include SRH recombination in the continuity equation(s)",
      Teuchos::tuple<std::string>("On","Off"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Radiative",
      "Off",
      "Determine if users want to include Radiative recombination in the continuity equation(s)",
      Teuchos::tuple<std::string>("On","Off"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Auger",
      "Off",
      "Determine if users want to include Auger recombination in the continuity equation(s)",
      Teuchos::tuple<std::string>("On","Off"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Avalanche",
      "Off",
      "Determine if users want to include Avalanche generation in the continuity equation(s)",
      Teuchos::tuple<std::string>("On","Off"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Discretization Method",
      "FEM-SUPG",
      "Determine the discretization method",
      Teuchos::tuple<std::string>("FEM-SUPG", "FEM-EFFPG", "FEM-SymEFFPG"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Tau_E",
      "Tanh",
      "Determine the Tau_E model in the Electron equation",
      Teuchos::tuple<std::string>("Linear","Tanh"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Tau_H",
      "Tanh",
      "Determines the Tau_H model in the Hole equation",
      Teuchos::tuple<std::string>("Linear","Tanh"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Tau_Ion",
      "Tanh",
      "Determines the Tau_Ion model in the Ion equation",
      Teuchos::tuple<std::string>("Linear","Tanh"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Length Scale",
      "Stream",
      "Determine the Length Scale model in the SUPG stabilization",
      Teuchos::tuple<std::string>("Stream","Shakib"),
      &opt
      );

    Teuchos::setStringToIntegralParameter<int>(
      "Add Source Term",
      "Off",
      "Determine if users want to add the source term in the SUPG stabilization",
      Teuchos::tuple<std::string>("On","Off"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Band Gap Narrowing",
      "Off",
      "Determine if users want to turn on the BGN effect due to heavy doping",
      Teuchos::tuple<std::string>("On","Off"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Electric Field Model",
      "Potential Gradient",
      "Determine the model for the electric field calculation",
      Teuchos::tuple<std::string>("Potential Gradient"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Acceptor Incomplete Ionization",
      "Off",
      "Determine if users want to include Acceptor Incomplete Ionization",
      Teuchos::tuple<std::string>("On","Off"),
      &opt
      );
    Teuchos::setStringToIntegralParameter<int>(
      "Donor Incomplete Ionization",
      "Off",
      "Determine if users want to include Donor Incomplete Ionization",
      Teuchos::tuple<std::string>("On","Off"),
      &opt
      );

    params->validateParametersAndSetDefaults(valid_parameters);

    solveElectron = params->sublist("Options").get<std::string>("Solve Electron");
    solveHole = params->sublist("Options").get<std::string>("Solve Hole");
    solveIon = params->sublist("Options").get<std::string>("Solve Ion");

    const std::string& srh_recomb = params->sublist("Options").get<std::string>("SRH");
    const std::string& rad_recomb = params->sublist("Options").get<std::string>("Radiative");
    const std::string& auger_recomb = params->sublist("Options").get<std::string>("Auger");
    const std::string& ava_gen = params->sublist("Options").get<std::string>("Avalanche");

    // Must solve both continuity equations when any recombination model is on.
    if ((srh_recomb == "On") || (rad_recomb == "On") || (auger_recomb == "On") || (ava_gen == "On"))
    {
      TEUCHOS_ASSERT((solveElectron == "True") && (solveHole == "True"));
      haveSource = true;
    }
    else
      haveSource = false;  // NO recomb. model

    discMethod = params->sublist("Options").get<std::string>("Discretization Method");
    tau_e_type = params->sublist("Options").get<std::string>("Tau_E");
    tau_h_type = params->sublist("Options").get<std::string>("Tau_H");
    tau_ion_type = params->sublist("Options").get<std::string>("Tau_Ion");
    ls_type = params->sublist("Options").get<std::string>("Length Scale");

    std::string source_stab = params->sublist("Options").get<std::string>("Add Source Term");
    if (source_stab == "On")
    {
      if ((srh_recomb == "On") || (rad_recomb == "On") || (auger_recomb == "On") || (ava_gen == "On"))
        add_source_stab = true;
      else
        add_source_stab = false;
    }
    else
      add_source_stab = false;

    // Electric Field Model = Potential Gradient by default, and can be expanded as needed
    field_model = params->sublist("Options").get<std::string>("Electric Field Model");

    // Get the value of Ion Charge
    ion_charge = params->sublist("Options").get<int>("Ion Charge");

    // Set the flag for BGN
    const std::string& bgn = params->sublist("Options").get<std::string>("Band Gap Narrowing");
    if (bgn == "On")
      haveBGN = true;
    else
      haveBGN = false;

    // error check for Incomplete Ionization models
    const std::string& acc_ioniz =
      params->sublist("Options").get<std::string>("Acceptor Incomplete Ionization");
    const std::string& don_ioniz =
      params->sublist("Options").get<std::string>("Donor Incomplete Ionization");
    if(acc_ioniz == "On" && solveHole == "False")
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error,
        "Error: must solve hole continuity equation when " <<
        "Acceptor Incomplete Ionization is On!" << "\n");
    if(don_ioniz == "On" && solveElectron == "False")
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error,
        "Error: must solve electron continuity equation when " <<
        "Donor Incomplete Ionization is On!" << "\n");
  }

  std::string prefix = params->get<std::string>("Prefix");
  std::string discfields = params->get<std::string>("Discontinuous Fields");
  std::string discsuffix = params->get<std::string>("Discontinuous Suffix");
  std::string basis_type = params->get<std::string>("Basis Type");
  int basis_order = params->get<int>("Basis Order");
  std::string model_id = params->get<std::string>("Model ID");
  int integration_order = params->get<int>("Integration Order");

  this->getEvaluatorParameterList()->sublist("Options") = params->sublist("Options");
  this->getEvaluatorParameterList()->set("Type",params->get<std::string>("Type"));

  // *************************************
  // Assemble DOF names and Residual names
  // *************************************

  m_names = Teuchos::rcp(new charon::Names(cell_data.baseCellDimension(),prefix,discfields,discsuffix));

  this->getEvaluatorParameterList()->set("Names",Teuchos::RCP<const charon::Names>(m_names));

  // Always solve the Poisson equation (no transients required)
  this->addDOF(m_names->dof.phi,basis_type,basis_order,integration_order,m_names->res.phi);
  this->addDOFGrad(m_names->dof.phi,m_names->grad_dof.phi);

  // Solve the electron continuity equation when solveElectron = "True"
  if (solveElectron == "True")
  {
    this->addDOF(m_names->dof.edensity,basis_type,basis_order,integration_order,m_names->res.edensity);
    this->addDOFGrad(m_names->dof.edensity,m_names->grad_dof.edensity);
    if (this->buildTransientSupport())
      this->addDOFTimeDerivative(m_names->dof.edensity,m_names->dxdt.edensity);
  }

  // Solve the hole continuity equation when solveHole = "True"
  if (solveHole == "True")
  {
    this->addDOF(m_names->dof.hdensity,basis_type,basis_order,integration_order,m_names->res.hdensity);
    this->addDOFGrad(m_names->dof.hdensity,m_names->grad_dof.hdensity);
    if (this->buildTransientSupport())
      this->addDOFTimeDerivative(m_names->dof.hdensity,m_names->dxdt.hdensity);
  }

  // Solve the ion continuity equation when solveIon = "True"
  if (solveIon == "True")
  {
    this->addDOF(m_names->dof.iondensity,basis_type,basis_order,integration_order,m_names->res.iondensity);
    this->addDOFGrad(m_names->dof.iondensity,m_names->grad_dof.iondensity);
    if (this->buildTransientSupport())
      this->addDOFTimeDerivative(m_names->dof.iondensity,m_names->dxdt.iondensity);
  }

  // Always solve the lattice temperature equation
  this->addDOF(m_names->dof.latt_temp,basis_type,basis_order,integration_order,m_names->res.latt_temp);
  this->addDOFGrad(m_names->dof.latt_temp,m_names->grad_dof.latt_temp);
  if (this->buildTransientSupport())
    this->addDOFTimeDerivative(m_names->dof.latt_temp,m_names->dxdt.latt_temp);

  this->addClosureModel(model_id);

  this->setupDOFs();
}


// ***********************************************************************
template <typename EvalT>
void charon::EquationSet_DDIonLattice<EvalT>::
buildAndRegisterEquationSetEvaluators(PHX::FieldManager<panzer::Traits>& fm,
                                      const panzer::FieldLibrary& fl,
                                      const Teuchos::ParameterList& user_data) const
{
  using panzer::EvaluatorStyle;
  using panzer::Integrator_BasisTimesScalar;
  using panzer::Traits;
  using PHX::Evaluator;
  using Teuchos::ParameterList;
  using Teuchos::RCP;
  using Teuchos::rcp;

  const charon::Names& n = *m_names;
  bool includeSoret = true;  // set "Include Soret Effect"

  // Get scaling parameters
  RCP<charon::Scaling_Parameters> scaleParams = user_data.get<Teuchos::RCP<charon::Scaling_Parameters> >("Scaling Parameter Object");

  // For now, assume that all dofs use the same basis
  RCP<panzer::IntegrationRule> ir = this->getIntRuleForDOF(m_names->dof.phi);
  RCP<panzer::BasisIRLayout> basis = this->getBasisIRLayoutForDOF(m_names->dof.phi);

  // ***************************************************************************
  // Construct the Poisson equation (solved with the continuity equations)
  // ***************************************************************************
/*
  // Compute the vacuum potential
  {
    ParameterList p("Vacuum Potential");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("Data Layout", ir->dl_scalar);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_VacuumPotential<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }
  {
    ParameterList p("Vacuum Potential");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("Data Layout", basis->functional);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_VacuumPotential<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Compute the gradient of vacuum potential at IPs
  {
    ParameterList p("Vacuum Potential Gradient");
    p.set("Name", n.field.vac_pot);
    p.set("Gradient Name", n.field.grad_vac_pot);
    p.set("Basis", basis);
    p.set("IR", ir);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::DOFGradient<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }
*/

  // Laplacian Operator: \int_{\Omega} \lambda2 \epsilon_r \grad_vacpot \cdot \grad_basis d\Omega
  {
    ParameterList p("Laplacian Residual");
    p.set("Residual Name", n.res.phi);
    // p.set("Flux Name", n.field.grad_vac_pot);
    p.set("Flux Name", n.grad_dof.phi);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set("Multiplier", scaleParams->scale_params.Lambda2);

    Teuchos::RCP<std::vector<std::string> > fms = Teuchos::rcp(new std::vector<std::string>);
    fms->push_back(n.field.rel_perm);
    p.set< Teuchos::RCP<const std::vector<std::string> > >("Field Multipliers",fms);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Integrator_GradBasisDotVector<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Source Operator
  {
    // Compute the Poisson source term: (p-n+Nd-Na) @ IP
    {
      ParameterList p("Poisson Source");
      p.set("Source Name", n.field.psrc);
      p.set("Data Layout", ir->dl_scalar);
      p.set< RCP<const charon::Names> >("Names", m_names);
      p.set("Solve Electron", solveElectron);
      p.set("Solve Hole", solveHole);
      p.set("Scaling Parameters", scaleParams);

      bool bsolveIon = false;
      if (solveIon == "True") bsolveIon = true;
      p.set<bool>("Solve Ion", bsolveIon);
      p.set<int>("Ion Charge", ion_charge);

      RCP< PHX::Evaluator<panzer::Traits> > op =
        rcp(new charon::PoissonSource<EvalT,panzer::Traits>(p));
      fm.template registerEvaluator<EvalT>(op);
    }

    // Integrate the source term
    {
      RCP<Evaluator<Traits>> op = rcp(new
        Integrator_BasisTimesScalar<EvalT, Traits>(EvaluatorStyle::CONTRIBUTES,
        n.res.phi, n.field.psrc, *basis, *ir, -1));
      fm.template registerEvaluator<EvalT>(op);
    }
  }


  // ***************************************************************************
  // Construct the electron continuity equation
  // ***************************************************************************

 if (solveElectron == "True")
 {
  // Transient Operator
  if (this->buildTransientSupport())
  {
    RCP<Evaluator<Traits>> op = rcp(new
      Integrator_BasisTimesScalar<EvalT, Traits>(EvaluatorStyle::CONTRIBUTES,
      n.res.edensity, n.dxdt.edensity, *basis, *ir));
    fm.template registerEvaluator<EvalT>(op);
  }

 if (discMethod == "FEM-EFFPG")
 {
  // Convection-Diffusion Residual
  {
   // compute electron EFFPG DDIonLattice current density
   {
    ParameterList p("Electron EFFPG DDIonLattice Current Density");
    p.set("Carrier Type", "Electron");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set<bool>("Temperature Gradient", true);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::EFFPG_DDIonLattice_CurrentDensity<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // integrate convection(/advection/drift)-diffusion term (current density)
   {
    ParameterList p("Electron Convection Diffusion Residual");
    p.set("Residual Name", n.res.edensity);
    p.set("Flux Name", n.field.elec_curr_density);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set("Multiplier", 1.0);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Integrator_GradBasisDotVector<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }
  }
 }  // end of if (discMethod == "FEM-EFFPG")

 else if ( (discMethod == "FEM-SUPG") or
           (discMethod == "FEM-SymEFFPG") )
 {
  // Construct the residual of current density dot grad_basis
  {
   // Compute the electric field whose expression depends on the model of choice.
   // Currently it is trivially set to -\grad_phi, but can be expanded to include other models.
   {
    ParameterList p("Electron Electric Field");
    p.set("Carrier Type", "Electron");
    p.set("Electric Field Model", field_model);
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Basis", basis);
    p.set("Band Gap Narrowing", haveBGN);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_ElectricField<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // Computing electron current density at IPs: Jn = n*mun*Fn + Dn*\grad_n
   {
    ParameterList p("Electron Current Density");
    p.set("Carrier Type", "Electron");
    p.set("Current Name", n.field.elec_curr_density);
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set<bool>("Temperature Gradient", true);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_CurrentDensity<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // The current density dot grad_basis residual
   {
    ParameterList p("Electron Convection Diffusion Residual");
    p.set("Residual Name", n.res.edensity+n.op.conv_diff);
    p.set("Flux Name", n.field.elec_curr_density);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set("Multiplier", 1.0);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Integrator_GradBasisDotVector<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

  } // end of the residual of current density dot grad_basis

 } // end of the else if statement

  // Source Operator: Integrate the total source term
  if (haveSource)
  {
    RCP<Evaluator<Traits>> op = rcp(new
      Integrator_BasisTimesScalar<EvalT, Traits>(EvaluatorStyle::CONTRIBUTES,
      n.res.edensity, n.field.total_recomb, *basis, *ir));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Calculate the SUPG stabilization residual
  if (discMethod == "FEM-SUPG")
    computeSUPGStabResidual(fm, fl, user_data, m_names, ir, basis, ls_type, tau_e_type, add_source_stab, "Electron", 0, includeSoret);

  // Calculate the symmetrized EFFPG stabilization residual
  else if (discMethod == "FEM-SymEFFPG")
  {
    ParameterList p("Electron SymEFFPG Stabilization Residual");
    p.set("Residual Name", n.res.edensity + n.op.sym_effpg_stab);
    p.set("Carrier Type", "Electron");
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("Basis", basis);
    p.set("IR", ir);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::SymEFFPG_Stab_Residual<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Use a sum operator to form the overall residual for the equation
  if ( (discMethod == "FEM-SUPG") or
       (discMethod == "FEM-SymEFFPG") )
  {
    ParameterList p;
    p.set("Sum Name", n.res.edensity);

    RCP<std::vector<std::string> > sum_names =
      rcp(new std::vector<std::string>);

    sum_names->push_back(n.res.edensity + n.op.conv_diff);

    // add stabilization residual
    if (discMethod == "FEM-SUPG")
    {
      sum_names->push_back(n.res.edensity + n.op.supg_conv);
      if (add_source_stab)  sum_names->push_back(n.res.edensity + n.op.supg_src);
    }
    else if (discMethod == "FEM-SymEFFPG")
      sum_names->push_back(n.res.edensity + n.op.sym_effpg_stab);

    p.set("Values Names", sum_names);
    p.set("Data Layout", basis->functional);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Sum<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

 }  // end of if (solveElectron == "True")


  // ***************************************************************************
  // Construct the hole continuity equation
  // ***************************************************************************

 if (solveHole == "True")
 {
  // Transient Operator
  if (this->buildTransientSupport())
  {
    RCP<Evaluator<Traits>> op = rcp(new
      Integrator_BasisTimesScalar<EvalT, Traits>(EvaluatorStyle::CONTRIBUTES,
      n.res.hdensity, n.dxdt.hdensity, *basis, *ir));
    fm.template registerEvaluator<EvalT>(op);
  }

 if (discMethod == "FEM-EFFPG")
 {
  // Convection-Diffusion Residual
  {
   // compute hole EFFPG DDIonLattice current density
   {
    ParameterList p("Hole EFFPG DDIonLattice Current Density");
    p.set("Carrier Type", "Hole");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set<bool>("Temperature Gradient", true);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::EFFPG_DDIonLattice_CurrentDensity<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // integrate convection(advection/drift)-diffusion term (current density)
   {
    ParameterList p("Hole Convection Diffusion Residual");
    p.set("Residual Name", n.res.hdensity);
    p.set("Flux Name", n.field.hole_curr_density);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set("Multiplier", -1.0);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Integrator_GradBasisDotVector<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }
  }
 }  // end of if (discMethod == "FEM-EFFPG")
  
 else if ( (discMethod == "FEM-SUPG") or
           (discMethod == "FEM-SymEFFPG") )
 {
  // Construct the convection-diffusion residual
  {
   // Compute the electric field whose expression depends on the model of choice.
   // Currently it is trivially set to -\grad_phi, but can be expanded to include other models.
   {
    ParameterList p("Hole Electric Field");
    p.set("Carrier Type", "Hole");
    p.set("Electric Field Model", field_model);
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Basis", basis);
    p.set("Band Gap Narrowing", haveBGN);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_ElectricField<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // Computing hole current density at IPs: Jp = p*mup*Fp - Dp*\grad_p
   {
    ParameterList p("Hole Current Density");
    p.set("Carrier Type", "Hole");
    p.set("Current Name", n.field.hole_curr_density);
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set<bool>("Temperature Gradient", true);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_CurrentDensity<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // The convection-diffusion residual
   {
    ParameterList p("Hole Convection Diffusion Residual");
    p.set("Residual Name", n.res.hdensity + n.op.conv_diff);
    p.set("Flux Name", n.field.hole_curr_density);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set("Multiplier", -1.0);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Integrator_GradBasisDotVector<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

  }  // end of constructing convection-diffusion residual

 } // end of else if statement
  
  // Source Operator: Integrate the total source term
  if (haveSource)
  {
    RCP<Evaluator<Traits>> op = rcp(new
      Integrator_BasisTimesScalar<EvalT, Traits>(EvaluatorStyle::CONTRIBUTES,
      n.res.hdensity, n.field.total_recomb, *basis, *ir));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Calculate the SUPG stabilization residual
  if (discMethod == "FEM-SUPG")
    computeSUPGStabResidual(fm, fl, user_data, m_names, ir, basis, ls_type, tau_h_type, add_source_stab, "Hole", 0, includeSoret);

  // Calculate the symmetrized EFFPG stabilization residual
  else if (discMethod == "FEM-SymEFFPG")
  {
    ParameterList p("Hole SymEFFPG Stabilization Residual");
    p.set("Residual Name", n.res.hdensity + n.op.sym_effpg_stab);
    p.set("Carrier Type", "Hole");
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("Basis", basis);
    p.set("IR", ir);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::SymEFFPG_Stab_Residual<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Use a sum operator to form the overall residual for the equation
  if ( (discMethod == "FEM-SUPG") or
       (discMethod == "FEM-SymEFFPG") )
  {
    ParameterList p;
    p.set("Sum Name", n.res.hdensity);

    RCP<std::vector<std::string> > sum_names =
      rcp(new std::vector<std::string>);

    sum_names->push_back(n.res.hdensity + n.op.conv_diff);

    if (discMethod == "FEM-SUPG")
    {
      sum_names->push_back(n.res.hdensity + n.op.supg_conv);
      if (add_source_stab)
        sum_names->push_back(n.res.hdensity + n.op.supg_src);
    }
    else if (discMethod == "FEM-SymEFFPG")
      sum_names->push_back(n.res.hdensity + n.op.sym_effpg_stab);

    p.set("Values Names", sum_names);
    p.set("Data Layout", basis->functional);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Sum<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

 }  // end of if (solveHole == "True")


  // ***************************************************************************
  // Construct the mobile ion continuity equation
  // ***************************************************************************

 if (solveIon == "True" )
 {
  // Transient Operator
  if (this->buildTransientSupport())
  {
    RCP<Evaluator<Traits>> op = rcp(new
      Integrator_BasisTimesScalar<EvalT, Traits>(EvaluatorStyle::CONTRIBUTES,
      n.res.iondensity, n.dxdt.iondensity, *basis, *ir));
    fm.template registerEvaluator<EvalT>(op);
  }

 if (discMethod == "FEM-EFFPG")
 {
  // Convection-Diffusion Residual
  {
   // compute ion EFFPG DDIonLattice current density
   {
    ParameterList p("Ion EFFPG DDIonLattice Current Density");
    p.set("Carrier Type", "Ion");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set<bool>("Temperature Gradient", true);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::EFFPG_DDIonLattice_CurrentDensity<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // integrate convection(/advection/drift)-diffusion term (current density)
   {
    ParameterList p("Ion Convection Diffusion Residual");
    p.set("Residual Name", n.res.iondensity);
    p.set("Flux Name", n.field.ion_curr_density);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set("Multiplier", -1.0);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Integrator_GradBasisDotVector<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }
  }
 }  // end of if (discMethod == "FEM-EFFPG")

 else if ( (discMethod == "FEM-SUPG") or
           (discMethod == "FEM-SymEFFPG") )
 {
  // Construct the convection-diffusion residual
  {
   // Compute the electric field whose expression depends on the model of choice.
   // Currently it is trivially set to -\grad_phi, but can be expanded to include other models.
   {
    ParameterList p("Ion Electric Field");
    p.set("Carrier Type", "Ion");
    p.set("Electric Field Model", field_model);
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Basis", basis);
    p.set("Band Gap Narrowing", false);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_ElectricField<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // Computing the ion current density at IPs:
   // Ji = Ni*vi - Di*\grad_Ni - ui*T*Si*Ni*\grad_T (i for ion)
   {
    ParameterList p("Ion Current Density");
    p.set("Carrier Type", "Ion");
    p.set("Current Name", n.field.ion_curr_density);
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set<bool>("Temperature Gradient", true);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_CurrentDensity<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // The convection-diffusion residual
   {
    ParameterList p("Ion Convection Diffusion Residual");
    p.set("Residual Name", n.res.iondensity + n.op.conv_diff);
    p.set("Flux Name", n.field.ion_curr_density);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set("Multiplier", -1.0);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Integrator_GradBasisDotVector<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

  }  // end of constructing convection-diffusion residual

 }  // end of else if statement
 
  // Source Operator: Integrate the total source term
  // Neglect the source term for Ion,

  // Calculate SUPG stabilization residual
  if (discMethod == "FEM-SUPG")
    computeSUPGStabResidual(fm, fl, user_data, m_names, ir, basis, ls_type, tau_ion_type, add_source_stab, "Ion", ion_charge, includeSoret);

  // Calculate the symmetrized EFFPG stabilization residual
  else if (discMethod == "FEM-SymEFFPG")
  {
    ParameterList p("Ion SymEFFPG Stabilization Residual");
    p.set("Residual Name", n.res.iondensity + n.op.sym_effpg_stab);
    p.set("Carrier Type", "Ion");
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("Basis", basis);
    p.set("IR", ir);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::SymEFFPG_Stab_Residual<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Use a sum operator to form the overall residual for the equation
  if ( (discMethod == "FEM-SUPG") or
       (discMethod == "FEM-SymEFFPG") )
  {
    ParameterList p;
    p.set("Sum Name", n.res.iondensity);

    RCP<std::vector<std::string> > sum_names = rcp(new std::vector<std::string>);

    sum_names->push_back(n.res.iondensity + n.op.conv_diff);

    if (discMethod == "FEM-SUPG")
      sum_names->push_back(n.res.iondensity + n.op.supg_conv);
    else if (discMethod == "FEM-SymEFFPG")
      sum_names->push_back(n.res.iondensity + n.op.sym_effpg_stab);

    p.set("Values Names", sum_names);
    p.set("Data Layout", basis->functional);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Sum<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

 }  // end of if (solveIon == "True")


  // ***************************************************************************
  // Construct the lattice temperature equation
  // ***************************************************************************
  // Transient Operator: \int_{\Omega} cL * \frac {\partial_T} {\partial_t} * basis d\Omega

  if (this->buildTransientSupport())
  {
    RCP<Evaluator<Traits>> op = rcp(new
      Integrator_BasisTimesScalar<EvalT, Traits>(EvaluatorStyle::CONTRIBUTES,
      n.res.latt_temp, n.dxdt.latt_temp, *basis, *ir,
      scaleParams->scale_params.cLPrefactor, {n.field.heat_cap}));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Laplacian Operator: \int_{\Omega} kL * \grad_T \cdot \grad_basis d\Omega
  {
    ParameterList p("Lattice Temperature Laplacian Residual");
    p.set("Residual Name", n.res.latt_temp);
    p.set("Flux Name", n.grad_dof.latt_temp);
    p.set("Basis", basis);
    p.set("IR", ir);
    p.set("Multiplier", scaleParams->scale_params.kLPrefactor);

    Teuchos::RCP<std::vector<std::string> > fms = Teuchos::rcp(new std::vector<std::string>);
    fms->push_back(n.field.kappa);
    p.set< Teuchos::RCP<const std::vector<std::string> > >("Field Multipliers",fms);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new panzer::Integrator_GradBasisDotVector<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Source Operator: \int_{\Omega} H * basis d\Omega
 {
  if (discMethod == "FEM-EFFPG")
  {
   // obtain electron electric field at IPs, used in computing the heat generation H
   {
    ParameterList p("Electron Electric Field");
    p.set("Carrier Type", "Electron");
    p.set("Band Gap Narrowing", haveBGN);
    p.set("Electric Field Model", field_model);
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Basis", basis);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_ElectricField<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // obtain hole electric field at IPs, used in computing the heat generation H
   {
    ParameterList p("Hole Electric Field");
    p.set("Carrier Type", "Hole");
    p.set("Band Gap Narrowing", haveBGN);
    p.set("Electric Field Model", field_model);
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Basis", basis);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_ElectricField<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // obtain ion electric field at IPs, used in computing the heat generation H
   {
    ParameterList p("Ion Electric Field");
    p.set("Carrier Type", "Ion");
    p.set("Band Gap Narrowing", false);
    p.set("Electric Field Model", field_model);
    p.set< RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Basis", basis);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_ElectricField<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }  
  }  // end of if (discMethod == "FEM-EFFPG")

   // Obtain the heat generation
   {
    bool enableElectron = false;
    bool enableHole = false;
    bool enableIon = false;
    if (solveElectron == "True") enableElectron = true;
    if (solveHole == "True") enableHole = true;
    if (solveIon == "True") enableIon = true;

    ParameterList p("Heat Generation");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set<bool>("Solve Electron", enableElectron);
    p.set<bool>("Solve Hole", enableHole);
    p.set<bool>("Solve Ion", enableIon);
    p.set<bool>("Have Source", haveSource);
    p.set("Scaling Parameters", scaleParams);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::DDLattice_HeatGeneration<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
   }

   // Integrate the source term
   {
    RCP<Evaluator<Traits>> op = rcp(new
      Integrator_BasisTimesScalar<EvalT, Traits>(EvaluatorStyle::CONTRIBUTES,
      n.res.latt_temp, n.field.heat_gen, *basis, *ir, -1));
    fm.template registerEvaluator<EvalT>(op);
   }
 }
}

// ***********************************************************************


// ======================================================= //
//     BEGIN computeSUPGStabResidual() for carrier       //
// ======================================================= //

template <typename EvalT>
void charon::EquationSet_DDIonLattice<EvalT>::
computeSUPGStabResidual(PHX::FieldManager<panzer::Traits>& fm,
            const panzer::FieldLibrary& /* fl */,
            const Teuchos::ParameterList& /* user_data */,
            const Teuchos::RCP<charon::Names>& m_names,
            const Teuchos::RCP<panzer::IntegrationRule>& ir,
            const Teuchos::RCP<panzer::BasisIRLayout>& basis,
            const std::string& ls_type, const std::string& tau_type,
            const bool& add_source_stab, const std::string& carr_type,
            const int& ion_charge, const bool& includeSoret) const
{

  using Teuchos::ParameterList;
  using Teuchos::RCP;
  using Teuchos::rcp;

  const charon::Names& n = *m_names;

  // carrier velocity on which carrier Peclet number depends
  // Ion Velocity is instantiated in ClosureModel_Factory
  if ((carr_type == "Electron") || (carr_type == "Hole"))
  {
    ParameterList p(carr_type+" Velocity");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Carrier Type", carr_type);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::FEM_Velocity<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Instantiate the Effective_Velocity evaluator
  {
    ParameterList p(carr_type+" Effective Velocity");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Carrier Type", carr_type);
    p.set<bool>("Include Soret Effect", includeSoret);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::Effective_Velocity<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Stabilization: PDE Residual R_carrier
  {
    ParameterList p(carr_type+" PDE Residual");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Carrier Type", carr_type);
    if(carr_type == "Ion"){ p.set<int>("Ion Charge", ion_charge); }
    p.set<bool>("Include Soret Effect", includeSoret);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::PDE_Residual_DD<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Carrier Peclet number, always make it available
  {
    ParameterList p(carr_type+" Peclet Number");
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("IR", ir);
    p.set("Length Scale", ls_type);
    p.set("Carrier Type", carr_type);
    p.set("Include Soret Effect", includeSoret);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::SUPG_Peclet<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // Stabilization: Tau_carrier
  if (tau_type == "Linear")
  {
    ParameterList p(carr_type+" "+tau_type+" Tau");
    p.set("IR", ir);
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("Add Source Stabilization", add_source_stab);
    p.set("Carrier Type", carr_type);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::SUPG_Tau_Linear<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);

    // should not be used for DDIonLattice since we did not compute the linear tau using
    // the effective velocity, which is also the reason that includeSoret is not used here.
  }
  else if (tau_type == "Tanh")
  {
    ParameterList p(carr_type+" "+tau_type+" Tau");
    p.set("IR", ir);
    p.set<RCP<const charon::Names> >("Names", m_names);
    p.set("Add Source Stabilization", add_source_stab);
    p.set("Carrier Type", carr_type);
    p.set<bool>("Include Soret Effect", includeSoret);

    RCP< PHX::Evaluator<panzer::Traits> > op =
      rcp(new charon::SUPG_Tau_Tanh<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error, "Error: "+tau_type+" for "+carr_type+" must be either Linear or Tanh !" );

  // SUPG stabilization
  // carrier convection stab.
  {
    ParameterList p(carr_type+" SUPG Convection Residual");
    // set Residual
    if(carr_type == "Electron")
      p.set("Residual Name", n.res.edensity + n.op.supg_conv);
    else if(carr_type == "Hole")
      p.set("Residual Name", n.res.hdensity+n.op.supg_conv);
    else if(carr_type == "Ion")
      p.set("Residual Name", n.res.iondensity+n.op.supg_conv);

    // set "Flux"
    if(carr_type == "Electron")
      p.set("Flux Name", n.field.elec_eff_velocity);
    else if (carr_type == "Hole")
      p.set("Flux Name", n.field.hole_eff_velocity);
    else if (carr_type == "Ion")
      p.set("Flux Name", n.field.ion_eff_velocity);

    p.set("Basis", basis);
    p.set("IR", ir);
    p.set("Multiplier", 1.0); // Multiplier = 1 has the same value for electrons, holes, and ions

    // add field multipliers
    Teuchos::RCP<std::vector<std::string> > fms = Teuchos::rcp(new std::vector<std::string>);
    if(carr_type == "Electron")
    {
      fms->push_back(n.field.tau_stab_e);
      fms->push_back(n.field.R_e);
    }
    else if(carr_type == "Hole")
    {
      fms->push_back(n.field.tau_stab_h);
      fms->push_back(n.field.R_h);
    }
    else if(carr_type == "Ion")
    {
      fms->push_back(n.field.tau_stab_ion);
      fms->push_back(n.field.R_ion);
    }
    p.set< Teuchos::RCP<const std::vector<std::string> > >("Field Multipliers",fms);

    RCP< PHX::Evaluator<panzer::Traits> > op =
        rcp(new panzer::Integrator_GradBasisDotVector<EvalT,panzer::Traits>(p));
    fm.template registerEvaluator<EvalT>(op);
  }

  // source term stab (for Electrons and Holles)
  if (add_source_stab)
  {
    using panzer::EvaluatorStyle;
    using panzer::Integrator_BasisTimesScalar;
    using panzer::Traits;
    using PHX::Evaluator;
    using std::string;
    using std::vector;

    // Note that valName == "", and if carr_type == "Ion", resName == "" as
    // well.  This is not right.
    string resName, valName;
    if (carr_type == "Electron")
      resName = n.field.R_e;
    else if (carr_type == "Hole")
      resName = n.field.R_h;
    else if (carr_type == "Ion")
      std::cout << "No SUPG source stabilization for ions!" << std::endl;
    vector<string> fieldMultipliers;
    if (carr_type == "Electron")
    {
      fieldMultipliers.push_back(n.field.recomb_deriv_e);
      fieldMultipliers.push_back(n.field.tau_stab_e);
    }
    else if (carr_type == "Hole")
    {
      fieldMultipliers.push_back(n.field.recomb_deriv_h);
      fieldMultipliers.push_back(n.field.tau_stab_h);
    } // end if carr_type is "Electron" or "Hole"
    RCP<Evaluator<Traits>> op = rcp(new
      Integrator_BasisTimesScalar<EvalT, Traits>(EvaluatorStyle::EVALUATES,
      resName, valName, *basis, *ir, -1, fieldMultipliers));
    fm.template registerEvaluator<EvalT>(op);
  }

}

// ======================================================= //
//     END computeSUPGStabResidual() for carrier       //
// ======================================================= //



// ***********************************************************************



#endif
