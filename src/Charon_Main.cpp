
///////////////////////////////////////////////////////////////////////////////
//
//  Include Files
//
///////////////////////////////////////////////////////////////////////////////

// C++
#include <algorithm>
#if       defined (__GNUG__) && defined (CHARON_DEBUG_FP)
#include <fenv.h>
#define FEENABLEEXCEPT
#endif // defined (__GNUG__) && defined (CHARON_DEBUG_FP) 
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

// Charon
#include "Charon_BCStrategy_Factory.hpp"
#include "Charon_ClosureModel_Factory_TemplateBuilder.hpp"
#include "Charon_ConfigDefs.hpp"
#include "Charon_CoupledModelEvaluator.hpp"
#include "Charon_CurrentConstraintList.hpp"
#include "Charon_CurrentConstraintModelEvaluator.hpp"
#include "Charon_CurrentConstraintModelEvaluatorLOCA.hpp"
#include "Charon_CVFEM_WorksetFactory.hpp"
#include "Charon_EFFPG_WorksetFactory.hpp"
#include "Charon_EquationSet_Factory.hpp"
#include <Charon_interp.hpp>
#include "Charon_Material_Properties.hpp"
#include "Charon_NOXObserverFactory.hpp"
#include "Charon_ResponseEvaluatorFactory_Current.hpp"
#include "Charon_ResponseEvaluatorFactory_HOCurrent.hpp"
#include "Charon_ResponseEvaluatorFactory_DispCurrent.hpp"
#include "Charon_RythmosObserverFactory.hpp"
#include "Charon_TempusObserverFactory.hpp"
#include "Charon_Scaling_Parameters.hpp"
#include "Charon_Schur2x2PreconditionerFactory.hpp"
#include "Charon_Version.hpp"

// Radiation
#include "Charon_PulseDamage_Spec.hpp"
#include "Charon_EmpiricalDamage_Data.hpp"

// Kokkos
#include "Kokkos_Core.hpp"
#include "Kokkos_DefaultNode.hpp"

// Panzer
#include "Panzer_ParameterLibraryUtilities.hpp"
#include "Panzer_PauseToAttach.hpp"
#include "Panzer_STK_ModelEvaluatorFactory.hpp"
//#include "Panzer_LinearObjFactory_Utilities.hpp"

// Composite Closure Model factory modeled on Panzer_ClosureModel_Factory_Composite
#include "Charon_ClosureModel_Factory_Composite.hpp"
#include "Charon_ClosureModel_Factory_Composite_TemplateBuilder.hpp"

// Charon FreqDom stuff
#include "Charon_FreqDom_Parameters.hpp"

// Seacas
#include "Ioss_SerializeIO.h"

// Teko
#include "Teko_PreconditionerFactory.hpp"

// Teuchos
#include "Teuchos_Assert.hpp"
#include "Teuchos_CommandLineProcessor.hpp"
#include "Teuchos_CommHelpers.hpp"
#include "Teuchos_ConfigDefs.hpp"
#include "Teuchos_DefaultComm.hpp"
#include "Teuchos_DefaultMpiComm.hpp"
#include "Teuchos_FancyOStream.hpp"
#include "Teuchos_GlobalMPISession.hpp"
#include "Teuchos_oblackholestream.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "Teuchos_YamlParameterListHelpers.hpp"
#include "Teuchos_StackedTimer.hpp"

// Thyra
#include "Thyra_ModelEvaluator.hpp"

// For checking exit status
#include "LOCA_Stepper.H"
#include "Piro_LOCASolver.hpp"
#include "Piro_LOCAAdaptiveSolver.hpp"
#include "Tempus_Integrator.hpp"

// Householder Constraint Solver
#include "Charon_Solver_SteadyStateConstraint.hpp"

// Boost
//#include <boost/math/special_functions/airy.hpp>

///////////////////////////////////////////////////////////////////////////////
//
//  Function Prototypes
//
///////////////////////////////////////////////////////////////////////////////

/**
 *  \brief Get Current Constraint BCs.
 *
 *  Loop over the list of boundary conditions to determine whether or not we
 *  have any Constant Current or Resistor Contact BCs.  If we do, add the valid
 *  ones to `currentConstraints`.
 *
 *  \param[in]     pl                 The "Boundary Conditions"
 *                                    `ParameterList`.
 *  \param[in,out] currentConstraints A `CurrentConstraintList`, which we'll
 *                                    populate with any current constraints
 *                                    found in the "Boundary Contitions"
 *                                    `ParameterList`.
 */
void
getCurrentConstraintBCs(
  const Teuchos::ParameterList&  pl,
  charon::CurrentConstraintList& currentConstraints);

/**
 *  \brief Build all the parameters.
 *
 *  If the input contains a "Parameters" sublist, then we create an "Active
 *  Parameter" for each entry, using its name and value.  If no "Parameters"
 *  sublist is present, we do more or less the same thing with any current
 *  constraints found in the "Boundary Conditions" sublist.
 *
 *  \param[in,out] currentConstraints The list of all the current constraints.
 *                                    A parameter index, indicating to which
 *                                    "Active Parameter" the given constraint
 *                                    corresponds, will be set for each
 *                                    constraint.
 *  \param[in,out] inputParams        The `ParameterList` created from the
 *                                    input file.
 */
void
buildParameters(
  charon::CurrentConstraintList& currentConstraints,
  Teuchos::ParameterList&        inputParams);

/**
 *  \brief Get the Scaling Information.
 *
 *  \todo This could use a detailed description.  Anyone care to fill it out?
 *
 *  \param[in,out] inputPList             The `ParameterList` corresponding to
 *                                        the input file.
 *  \param[out]    tempScale              The temperature scaling factor.
 *  \param[out]    concScale              The concentration scaling factor.
 *  \param[out]    diffScale              The diffusion scaling factor.
 *  \param[out]    userMeshScaling        A flag indicating whether or not the
 *                                        user has specified a mesh scaling
 *                                        factor.
 *  \param[out]    meshScale              The mesh scaling factor.
 *  \param[out]    doOutputUnscaling      A flag indicating whether or not the
 *                                        user wants to unscale the output in
 *                                        the Exodus file.
 *  \param[out]    doInputVariableScaling A flag indicating whether or not the
 *                                        user wants to scale the input Exodus
 *                                        variables.
 */
void
getScalingInfo(
  Teuchos::ParameterList& inputPList,
  double&                 tempScale,
  double&                 concScale,
  double&                 diffScale,
  bool&                   userMeshScaling,
  double&                 meshScale,
  bool&                   doOutputUnscaling,
  bool&                   doInputVariableScaling);

/**
 *  \brief Check to see if the empirical model is being used.
 *
 *  \todo This could use a detailed description.  Anyone care to fill it out?
 *
 *  \param[in,out] inputParams   The `ParameterList` corresponding to the input
 *                               file.
 *  \param[out]    ebEmpiricalOn A flag indicating whether or not the empirical
 *                               model has been called out and the associated
 *                               list has an "pulse type" parameter.
 *  \param[out]    cbEmpiricalOn A flag indicating whether or not the empirical
 *                               model has been called out and the associated
 *                               list has a "bcpulseType" parameter.
 *  \param[in]     scaleParams   The scaling parameters.
 */
void
checkForEmpiricalModel(
  Teuchos::ParameterList&                     inputParams,
  Teuchos::RCP<charon::Scaling_Parameters>    scaleParams,
  Teuchos::RCP<panzer::GlobalData>            globalData,
  Teuchos::RCP<charon::EmpiricalDamage_Data>  damageData);

/**
 *  \brief Check to see if the cluster model is being used and whether in situ
 *         clusters should be constructed.
 *
 *  \todo This could use a detailed description.  Anyone care to fill it out?
 *
 *  \param[in,out] cmParamList    The "Closure Models" `ParameterList` from the
 *                                input file.
 *  \param[out]    clusterManager The `ClusterManager` that will be created if
 *                                Xyce clusters are enabled.
 *  \param[out]    clusterInterp  The `clusterInterpolator` that will be
 *                                created and added to the "Defect Cluster
 *                                Recombination" sublist, if it exists, and
 *                                which will be registered with the
 *                                `ClusterManager` if Xyce clusters are
 *                                enabled.
 *
 *  \returns True if the cluster model has been called out, Xyce clusters are
 *           enabled, and the "Input File Type" is "IN SITU"; false otherwise.
 */
bool
checkForClusterModel(
  Teuchos::ParameterList&                    cmParamList,
  Teuchos::RCP<int>&                         clusterManager,
  Teuchos::RCP<charon::clusterInterpolator>& clusterInterp);

/**
 *  \brief Read the pulse files to get onset times for breakpointing.
 *
 *  \todo This could use a detailed description.  Anyone care to fill it out?
 *
 *  \param[in] pulseFilename The name of the file from which to read the pulse
 *                           information.
 *
 *  \returns A `vector` of pulse times.
 */
std::vector<double>
getPulses(
  std::string pulseFilename);

/**
 *  \brief Get the power of ten.
 *
 *  For an input double value, determine the exponent if it were to be written
 *  in scientific notation.  For instance:
 *  - `getPowerOfTen(1.2345)    ==  0`
 *  - `getPowerOfTen(0.0012345) == -3`
 *  - `getPowerOfTen(123.45)    ==  2`
 *
 *  \param[in] number The number for which you'd like to find it's exponent.
 *
 *  \returns The exponent if `number` were to be written in scientific
 *           notation.
 */
int
getPowerOfTen(
  double number);

/**
 *  \brief Scale Times in Rythmos.
 *
 *  \todo This could use a detailed description.  Anyone care to fill it out?
 *
 *  \param[in,out] inputPList  The `ParameterList` corresponding to the input
 *                             file.
 *  \param[in] scaleParams The scaling parameters.
 */
void
scaleRythmosTimes(
  Teuchos::ParameterList&                  inputPList,
  Teuchos::RCP<charon::Scaling_Parameters> scaleParams);

/**
 *  \brief Scale Times in Tempus.
 *
 *  \todo This could use a detailed description.  Anyone care to fill it out?
 *
 *  \param[in,out] inputPList  The `ParameterList` corresponding to the input
 *                             file.
 *  \param[in] scaleParams The scaling parameters.
 */
void
scaleTempusTimes(
  Teuchos::ParameterList&                  inputPList,
  Teuchos::RCP<charon::Scaling_Parameters> scaleParams);

/**
 *  \brief Is this a contact boundary condition?
 *
 *  Determines whether or not the given boundary condition is one of the
 *  contacts we currently support.  Supported "Strategy"s are:
 *  - "Ohmic Contact"
 *  - "Linear Ramp"
 *  - "Sinusoid"
 *  - "Periodic"
 *  - "BJT1D Base Contact"
 *  - "Thermal Contact"
 *  - "Constant Current"
 *  - "Resistor Contact"
 *
 *  \param[in]  bc           A `ParameterList` representing a single boundary
 *                           condition.
 *  \param[out] isConstraint A flag indicating whether or not the boundary
 *                           condition "Strategy" is either "Constant Current"
 *                           or "Resistor Contact".
 *
 *  \returns Whether or not `bc` is one of the contacts we support.
 */
bool
isContactBC(
  const Teuchos::ParameterList& bc,
  bool&                         isConstraint);

/**
 *  \brief Determine the contact sides.
 *
 *  Try to determine contact sides from the boundary condition parameter list.
 *  The result is returned as a `vector` of (sideset, element block) pairs.
 *
 *  \param[in]     bcPL               The "Boundary Conditions" `ParameterList`
 *                                    from the input file.
 *  \param[in,out] currentConstraints The list of all the current constraints.
 *                                    A response index, indicating to which
 *                                    response the given constraint
 *                                    corresponds, will be set for each
 *                                    constraint.
 *  \param[out]    contactSides       A `vector` of pairs of element block and
 *                                    sideset IDs indicating the contact sides.
 *  \param[out]    contactStrategy    A vector of contact strategy strings                                  
 */
void
determineContactSides(
  const Teuchos::ParameterList&                     bcPL,
  charon::CurrentConstraintList&                    currentConstraints,
  std::vector<std::pair<std::string, std::string>>& contactSides,
  std::vector<std::string>&                         contactStrategy);

/**
 *  \brief Check to see if EFFPG or CVFEM equations sets are being used.
 *
 *  \todo This could use a detailed description.  Anyone care to fill it out?
 *
 *  \param[in]  inputPList The `ParameterList` corresponding to the input file.
 *  \param[out] isEFFPG    A flag indicating whether or not we should be using
 *                         an EFFPG finite element method.
 *  \param[out] isCVFEM    A flag indicating whether or not we should be using
 *                         a control volume finite element method.
 */
void
checkEqnSetType(
  const Teuchos::ParameterList& inputPList,
  bool&                         isEFFPG,
  bool&                         isCVFEM);

/**
 *  \brief Add Current Responses.
 *
 *  Create the current responses for each terminal to which we've hooked up a
 *  supported contact boundary condition, and then add them to the
 *  `ModelEvaluatorFactory`, and to the Rythmos and NOX Observer Factories.
 *
 *  \param[in]     contactSides       A list of unique (sideset, element block)
 *                                    ID pairs where supported contact boundary
 *                                    conditions exist.
 *  \param[in]     contactStrategy    A list of contact strategies for supported contacts                                  
 *  \param[in]     currentHighOrder   A flag indicating whether or not we
 *                                    should use the high-order current
 *                                    calculation.
 *  \param[in]     scaleParams        The scaling parameters, which will be
 *                                    passed to the
 *                                    `HOCurrentResponse_Builder`.
 *  \param[in]     currentConstraints The list of all current constraints.
 *  \param[in]     writeCurrentGraph  A flag indicating whether or not to write
 *                                    a current Graphviz file.
 *  \param[in]     isSingleFreq       A flag indicating whether or not a single
 *                                    freq is used in the frequency domain analysis   
 *  \param[in]     frequency          A double value storing the frequency value 
 *                                    in [Hz] for a single-freq FD simulation                             
 *  \param[in]     isTransient        A flag indicating whether or not it is 
 *                                    a transient time domain simulation
 *  \param[in,out] meFactory          The `ModelEvaluatorFactory` to which
 *                                    we'll add the current responses.
 *  \param[in,out] rof                The `RythmosObserverFactory`, to which
 *                                    we'll add the response names.
 *  \param[in,out] nof                The `NOXObserverFactory`, to which we'll
 *                                    add the response names.
 *  \param[in,out] cmFactory          The closure model factory used to build
 *                                    the closure models.  (I spent a half-hour
 *                                    trying to determine if this gets modified
 *                                    and couldn't figure it out.  It's listed
 *                                    as "in,out" to let you know if might get
 *                                    modified.)
 *  \param[out]    responseNames      A list of response names.
 */
void
addCurrentResponses(
  const std::vector<std::pair<std::string, std::string>>& contactSides,
  const std::vector<std::string>&                         contactStrategy,
  const bool&                                             currentHighOrder,
  const Teuchos::RCP<charon::Scaling_Parameters>&         scaleParams,
  const charon::CurrentConstraintList&                    currentConstraints,
  const bool&                                             writeCurrentGraph,
  panzer_stk::ModelEvaluatorFactory<double>&              meFactory,
  Teuchos::RCP<charon::RythmosObserverFactory>&           rof,
  Teuchos::RCP<charon::TempusObserverFactory>&            tof,
  Teuchos::RCP<charon::NOXObserverFactory>&               nof,
  panzer::ClosureModelFactory_TemplateManager<panzer::Traits>&
                                                          cmFactory,
  std::vector<std::string>&                               responseNames,
  const std::vector<std::string>&                         responseNames_fd_suffix,
  bool                                                    isFreqDom,
  bool                                                    isSingleFreq,
  bool                                                    isTransient,
  double                                                  frequency );

void checkHeterointerfacesCharge(Teuchos::ParameterList& bcPL);

void checkGateTunneling(Teuchos::ParameterList& bcPL);

void adjustSchottkyContacts_params(Teuchos::ParameterList& bcPL,
				   Teuchos::ParameterList& user_data);

void gateContacts_params(Teuchos::ParameterList& bcPL,
			 Teuchos::ParameterList& user_data);

void checkTID_params(const Teuchos::ParameterList& bcPL,
		      const Teuchos::ParameterList& cmPL);

void blockMaterial_params(const Teuchos::ParameterList& blphPL,
                          const Teuchos::ParameterList& phPL,
			  const Teuchos::ParameterList& cmPL,
			  Teuchos::ParameterList& user_data);

// Bring in the version file so we can print out the string.
const char* versionString =
  #include "charonVersionString.txt"
  ;

///////////////////////////////////////////////////////////////////////////////
//
//  main()
//
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
  using   charon::BCFactory;
  using   charon::ClosureModelFactory_TemplateBuilder;
  using   charon::clusterInterpolator;
  using   charon::CurrentConstraintList;
  using   charon::CurrentConstraintModelEvaluator;
  using   charon::CVFEM_WorksetFactory;
  using   charon::EFFPG_WorksetFactory;
  using   charon::EquationSet_Factory;
  using   charon::Scaling_Parameters;
  using   charon::Schur2x2PreconditionerFactory;
  using   charon::version;

  using   Ioss::SerializeIO;

  using   std::cerr;
  using   std::cout;
  using   std::endl;
  using   std::exception;
  using   std::pair;
  using   std::runtime_error;
  using   std::string;
  using   std::vector;
  
  using   Teko::AutoClone;
  using   Teko::PreconditionerFactory;
  
  using   Teuchos::Comm;
  using   Teuchos::CommandLineProcessor;
  using   Teuchos::DefaultComm;
  using   Teuchos::FancyOStream;
  using   Teuchos::GlobalMPISession;
  using   Teuchos::MpiComm;
  using   Teuchos::oblackholestream;
  using   Teuchos::ParameterList;
  using   Teuchos::parameterList;
  using   Teuchos::RCP;
  using   Teuchos::rcp;
  using   Teuchos::rcp_dynamic_cast;
  using   Teuchos::Time;
  using   Teuchos::TimeMonitor;
  using   Teuchos::updateParametersFromXmlFileAndBroadcast;
  using   Teuchos::updateParametersFromYamlFileAndBroadcast;

  typedef Thyra::ModelEvaluatorBase::InArgs<double>  InArgs;
  typedef Thyra::ModelEvaluatorBase::OutArgs<double> OutArgs;

  // Initial setup.
  int  status(0);
  bool outputTimings(true);

  // Catch floating point errors when CHARON_DEBUG_FP is defined.
#if defined (FEENABLEEXCEPT)
  feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
#endif

  oblackholestream blackhole;
  GlobalMPISession mpiSession(&argc, &argv, &blackhole);
  Kokkos::initialize(argc, argv);

  const auto stackedTimer = Teuchos::rcp(new Teuchos::StackedTimer("Charon: Main Driver"));
  Teuchos::TimeMonitor::setStackedTimer(stackedTimer);

  auto out  = rcp(new FancyOStream(rcp(&cout, false)));
  auto pout = rcp(new FancyOStream(rcp(&cout, false)));
  if (mpiSession.getNProc() > 1)
  {
    out->setShowProcRank(true);
    out->setOutputToRootOnly(0);
    pout->setShowProcRank(true);
  }

  // Print out the copyright information.
  if (mpiSession.getRank() == 0)
  {
    *out << "*****************************************************************"
         << "******" << endl << "             " << version() << endl
         << "TCAD code for simulation of semiconductor physics " << endl
         << endl
         << "Copyright 2020 National Technology & Engineering Solutions of Sandia," << endl
         << "LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the" << endl
         << "U.S. Government retains certain rights in this software."
         << endl
         << "Questions? Contact the Charon team via email:" << endl
         << "    charon@sandia.gov" << endl
         << "*****************************************************************"
         << "******" << endl;
  }

  // Add in the Teko preconditioner factory.
  PreconditionerFactory::addPreconditionerFactory("Schur2x2",
    rcp(new AutoClone<Schur2x2PreconditionerFactory>()));

  // ACH: Try adding a different preconditioner factory.
  PreconditionerFactory::addPreconditionerFactory("FreqDomPreconditioner",
    rcp(new AutoClone<Schur2x2PreconditionerFactory>()));

  string inputFileName("charon.xml");

  // This is the main try block.
  try
  {
    // Create a timer and get the MPI communicator.
    RCP<Time> totalTime = TimeMonitor::getNewTimer("Charon: Total Time");
    TimeMonitor timer(*totalTime);
    RCP<const Comm<int>> comm = DefaultComm<int>::getComm();

    // Parse the command-line arguments.
    bool writeOnSolveFail(false);
    int exodusIONumProcs(0);
    bool computeCurrent(false), currentHighOrder(true), computeCurrentFD(false),
      writeCurrentGraph(false);
    {
      CommandLineProcessor clp;
      clp.setOption("i", &inputFileName, "Charon input xml/yaml filename");
      clp.setOption("exodus-io-num-procs", &exodusIONumProcs, "Number of "    \
        "processes that can access the file system at the same time to read " \
        "their portion of a sliced exodus file in parallel.  Defaults to 0 "  \
        "- implies all processes for the run can access the file system at "  \
        "the same time.");
      clp.setOption("write-on-solver-failure", "no-write-on-solver-failure",
                    &writeOnSolveFail, "Output should be performed even when " \
                    "the nonlinear solver fails to converge.");
      clp.setOption("current", "no-current", &computeCurrent, "Do "           \
        "computation of scalar electric current?");
      clp.setOption("current-ho", "current-lo", &currentHighOrder, "Use "     \
        "high-order current calculation");
      clp.setOption("currentFD", "no-currentFD", &computeCurrentFD, "Do "           \
        "computation of scalar electric current, outputting in frequency domain?");
      clp.setOption("current-graph", "no-current-graph", &writeCurrentGraph,
        "Output an assembly graph for the current calculation?");
      CommandLineProcessor::EParseCommandLineReturn parseReturn =
        clp.parse(argc, argv, &cerr);
      TEUCHOS_TEST_FOR_EXCEPTION(parseReturn !=
        CommandLineProcessor::PARSE_SUCCESSFUL, runtime_error,
        "Failed to parse command line!")
      TEUCHOS_ASSERT(exodusIONumProcs <= comm->getSize());
      if (exodusIONumProcs != 0)
        SerializeIO::setGroupFactor(exodusIONumProcs);
    } // end of parsing the command-line arguments

    // Parse the input file and broadcast to the other processes.
    auto inputParams = rcp(new ParameterList("Charon Parameters"));
    {
      const string checkXML(".xml"), checkYAML(".yaml");
      if (inputFileName.find(checkXML) != string::npos)
        updateParametersFromXmlFileAndBroadcast(inputFileName,
          inputParams.ptr(), *comm);
      else if (inputFileName.find(checkYAML) != string::npos)
        updateParametersFromYamlFileAndBroadcast(inputFileName,
          inputParams.ptr(), *comm);
      else // if the input file has neither an XML nor YAML extension
        TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error, "ERROR:  Input file "
          << inputFileName << " requires a suffix of type \".xml\" or "       \
          "\".yaml\" to determine the parser type.")
    } // end of input file parsing
    *out << *inputParams << endl;

    // Pause to attach to MPI_COMM_WORLD, if needed.
    if (inputParams->get("Pause To Attach", false))
      panzer::pauseToAttach();
    inputParams->remove("Pause To Attach");

    // Detect the input file option to disable otuput of timings to the
    // screen if the user so requested it
    if (inputParams->sublist("Options").isParameter("Output Timings"))
    {
      outputTimings = inputParams->sublist("Options").get<bool>("Output Timings");
    }

    // Check if "Mixed Mode" is specified as a boundary condition strategy.
    // If so, set the appropriate underlying BC Strategy. Mixed Mode options: 
    //    Current -> Charon provides a voltage to the coupled problem, and receives a current to be imposed as a BC
    //               Correspondingly, use "Constant Current" with "Xyce Coupled Current" parameter
    //    Voltage -> Charon proviides a current to the coupled problem, and receives a voltage to be impose as a BC
    //               Correspondingly, use "Ohmic Contact" with "Xyce Varying Voltage" parameter
    // This needs to be done before determining the CurrentConstraintList because a current mixed mode 
    // requires contact current calculation, whereas a vltage mixed mode strategy does not.
    Teuchos::ParameterList& bcParams = inputParams->sublist("Boundary Conditions");
    Teuchos::RCP<Teuchos::ParameterList> xyceCouplingPL = Teuchos::rcp(new Teuchos::ParameterList);
    xyceCouplingPL->set<Teuchos::RCP<Teuchos::ParameterList> >("Contact Coupling Map", Teuchos::rcp(new Teuchos::ParameterList));
    Teuchos::RCP<Teuchos::ParameterList> contact_coupling_map = 
      xyceCouplingPL->get<Teuchos::RCP<Teuchos::ParameterList> >("Contact Coupling Map");
    bool mixedModeCouplingME = false;
    for(auto itr = bcParams.begin(); itr != bcParams.end(); ++itr)
    {
      const Teuchos::ParameterEntry& entry = bcParams.entry(itr);
      TEUCHOS_ASSERT(entry.isList());
      Teuchos::ParameterList& bc = Teuchos::getValue<Teuchos::ParameterList>(entry);
      // Current mode
      if (bc.get<string>("Strategy") == "Mixed Mode via Current"){
        bc.set<string>("Strategy", "Constant Current");
        std::cout << "Mixed Mode strategy choice imposes this BC Strategy:: "
                  << bc.get<string>("Strategy") << std::endl;
        computeCurrent = true;
        mixedModeCouplingME = true;
        if(!bc.sublist("Data").isType<std::string>("Netlist")){
	  std::cout << "Missing 'Netlist' in 'Data' sublist of 'Mixed Mode via Current' BC Strategy" << std::endl;
          TEUCHOS_ASSERT(false);
	}
        if(!bc.sublist("Data").isType<std::string>("Node Name")){
	  std::cout << "Missing 'Node Name' in 'Data' sublist of 'Mixed Mode via Current' BC Strategy" << std::endl;
          TEUCHOS_ASSERT(false);
	}
        xyceCouplingPL->set<int>("Initial Xyce Coupling on Step Number", 
                                 bc.sublist("Data").get<int>("Initial Xyce Coupling on Step Number",0));
        xyceCouplingPL->set<std::string>("Netlist", bc.sublist("Data").get<string>("Netlist"));
        contact_coupling_map->set<string>(bc.get<string>("Sideset ID"),bc.sublist("Data").get<string>("Node Name"));
      }
      // Voltage mode
      if(bc.get<string>("Strategy") == "Mixed Mode via Voltage"){
        mixedModeCouplingME = true;
        bc.set<string>("Strategy", "Ohmic Contact");
        std::cout << "Mixed Mode strategy choice imposes this BC Strategy:: "
                  << bc.get<string>("Strategy") << std::endl;
        computeCurrent = true;
      }
    }

    // Check to see if a current contraint is added at the boundary, and if so,
    // get the appropriate information from it and turn on the contact current
    // computation.
    CurrentConstraintList currentConstraints(mixedModeCouplingME);
    getCurrentConstraintBCs(inputParams->sublist("Boundary Conditions"),
      currentConstraints);
    if (not currentConstraints.empty())
      computeCurrent = true;

    // Create the global data.
    RCP<panzer::GlobalData> globalData = panzer::createGlobalData();

    // Build all the parameters (register them with Sacado and modify the input
    // deck to register them with Panzer).
    buildParameters(currentConstraints, *inputParams);

    // Obtain the temperature and concentration scaling parameters.
    double tempScale(0), concScale(0), diffScale(0), meshScale(0);
    bool userMeshScaling(false), doOutputUnscaling(false),
      doInputVariableScaling(false);
    getScalingInfo(*inputParams, tempScale, concScale, diffScale,
      userMeshScaling, meshScale, doOutputUnscaling, doInputVariableScaling);

    // Instantiate the Scaling_Parameters class.
    auto scaleParams = rcp(new Scaling_Parameters(tempScale, concScale,
      diffScale, userMeshScaling, meshScale,
      doOutputUnscaling | doInputVariableScaling));

    // Add the empirical data structure to the user data for access in
    // various locations throughout the code. This has to be set
    // irrespective of whether the run involves the empirical damage
    // model to avoid problems when the code validates the ParameterList
    RCP<charon::EmpiricalDamage_Data> damageData =
      rcp(new charon::EmpiricalDamage_Data);
    inputParams->sublist("User Data").set("empirical damage data", damageData);

    checkForEmpiricalModel(*inputParams, scaleParams, globalData, damageData);

    // Construct the cluster manager, if coupled to Xyce.
    RCP<clusterInterpolator> clusterInterp;
    RCP<int> clusterManager;
    bool constructClusters = checkForClusterModel(
      inputParams->sublist("Closure Models"), clusterManager, clusterInterp);
    if (not constructClusters)
    {
      // Don't really do anything here.  I'm just suppressing a warning until
      // the whole coupleToXyce thing settles into something fixed.
      constructClusters = false;
    } // end if (constructClusters) or not
    if (constructClusters)  //This code-defined if not coupled to xyce
      {                     //catches a case in which in situ clusters
	mpiSession.abort(); //have been requested.  Result is abort.
      }
    // Scale the times specified in the Rythmos section before sending them to
    // Rythmos for a transient simulation.
    const string piroSolver =
      inputParams->sublist("Solution Control").get<string>("Piro Solver");
    bool isLOCA(false);
    bool isTransient(false);

    if (piroSolver == "Rythmos")
    {
      isTransient = true;  // determine transient displacement current calculation 
      scaleRythmosTimes(*inputParams, scaleParams);
    }
    else if (piroSolver == "Tempus")
    {
      isTransient = true;  // determine transient displacement current calculation 
      scaleTempusTimes(*inputParams, scaleParams);
    }
    else if ( (piroSolver == "LOCA") || (piroSolver == "LOCA-Constrained") )
      isLOCA = true;

    // frequency domain logic to check if a freq domain analysis is specified;
    // if so, read in parameters to build a FreqDomParameters object,
    // then pass a RCP, pointing to it, to the input parameter list                      
    // HB mod: add a FreqDomParameters object to the input parameters list, if "Frequency Domain" is specified
    ParameterList::ConstIterator pb_name_iterator = inputParams->sublist("Physics Blocks").begin(); // we need an iterator to access the physics block names
    std::string pb_name = pb_name_iterator->first;  // Semiconductor, Semiconductor1, Semiconductor2, InGaP, GaAs

    // determine if a frequency domain analysis is specified (should be true for them all, or none at all)
    // TODO: guard against combining freq-dom and time-dom equation set specifications
    bool isFreqDom = inputParams->sublist("Physics Blocks").sublist(pb_name).sublist("child0").get<std::string>("Type") == "Frequency Domain";
    //std::cout << "Charon_Main detected a frequency domain simulation: " << isFreqDom << std::endl;

    // if(isFreqDom): create a RCP<ParameterList> for the "Frequency Domain Options" pl from the input deck
    Teuchos::RCP<Teuchos::ParameterList> freqDomPL = ( isFreqDom ? 
        Teuchos::rcpFromRef(inputParams->sublist("Physics Blocks").sublist(pb_name).sublist("child0").sublist("Frequency Domain Options"))
      : Teuchos::rcp(new Teuchos::ParameterList()) );

    // create the frequency domain parameters object using the freqDomPL
    Teuchos::RCP<FreqDomParameters> freqDomParamsRCP = ( isFreqDom ? 
        Teuchos::rcp(new FreqDomParameters(freqDomPL)) : Teuchos::rcp(new FreqDomParameters()));

    // if(isFreqDom): add a RCP<FreqDomParameters> to the parameter list, 
    // to be used by the equation set and closure models 
    // we add it to the "Options" pl because that gets passed to the closure models!
    if(isFreqDom)
      while(pb_name_iterator != inputParams->sublist("Physics Blocks").end())
      {
        std::string pb_name = pb_name_iterator->first;
        inputParams->sublist("Physics Blocks").sublist(pb_name).sublist("child0").sublist("Options").set<Teuchos::RCP<FreqDomParameters> >(
          "Frequency Domain Parameters", freqDomParamsRCP );
        ++pb_name_iterator;
      }
    //std::cout << "Charon_Main added a RCP<FreqDomParameters> to the parameter list" << std::endl;
    
    // Add in the application-specific equation set factory.
    auto eqSetFactory = rcp(new EquationSet_Factory);

    // Get the appropriate suffixes for frequency domain simulation
    // (used for both the closure models' charon::Names objects and also for calculating current responses
    std::vector<std::string> fd_suffixes = {};
    if(isFreqDom)
      for(int tp = 0 ; tp < freqDomParamsRCP->getNumTimeCollocationPoints() ; tp++)
	fd_suffixes.push_back("_TP" + std::to_string(tp) + "_");
    else if(!isFreqDom)
      fd_suffixes = {""};  // include only one closure model with no FD suffix

    // Create the composite closure model factory
    // the length of fd_suffixes equals the number of closure models
    std::vector<charon::ClosureModelFactory_TemplateBuilder> cmBuilders;
    std::vector<Teuchos::RCP<panzer::ClosureModelFactory_TemplateManager<panzer::Traits> > > cmFactories;

    for(unsigned int i = 0 ; i < fd_suffixes.size() ; i++){
        cmBuilders.push_back(charon::ClosureModelFactory_TemplateBuilder(scaleParams,true,"Charon Parameters->", fd_suffixes[i]));
        cmFactories.push_back(Teuchos::rcp(new panzer::ClosureModelFactory_TemplateManager<panzer::Traits>));
        cmFactories[i]->buildObjects(cmBuilders[i]);
    }

    charon::ClosureModelFactoryComposite_TemplateBuilder cmBuilderComposite;
    for(auto factory : cmFactories)
        cmBuilderComposite.addFactory(factory);

    panzer::ClosureModelFactory_TemplateManager<panzer::Traits> cmFactory;
    cmFactory.buildObjects(cmBuilderComposite);

    // A GlobalStatistics closure model requires the comm to be set in the user
    // data.
    inputParams->sublist("User Data").set("Comm", comm);

    //  Schottky contacts
    adjustSchottkyContacts_params(inputParams->sublist("Boundary Conditions"),
				  inputParams->sublist("User Data"));

    gateContacts_params(inputParams->sublist("Boundary Conditions"),
			inputParams->sublist("User Data"));

    checkTID_params(inputParams->sublist("Boundary Conditions"),
		    inputParams->sublist("Closure Models"));

    blockMaterial_params(inputParams->sublist("Block ID to Physics ID Mapping"),
                         inputParams->sublist("Physics Blocks"),
			 inputParams->sublist("Closure Models"),
                         inputParams->sublist("User Data"));

    checkHeterointerfacesCharge(inputParams->sublist("Boundary Conditions"));

    checkGateTunneling(inputParams->sublist("Boundary Conditions"));
    
    // Add in the application-specific BC factory.
    BCFactory bcFactory;

    // Set up a STK response library for file IO.
    auto stkIOResponseLibrary = rcp(new panzer::ResponseLibrary<panzer::Traits>());

    // Add in the application-specific observer factories.
    Teuchos::RCP<charon::NOXObserverFactory> nof;
    Teuchos::RCP<charon::RythmosObserverFactory> rof;
    Teuchos::RCP<charon::TempusObserverFactory> tof;
    {
      if (piroSolver == "Tempus")
      {
        tof = Teuchos::rcp(new charon::TempusObserverFactory(stkIOResponseLibrary,
                                                             globalData->pl,
                                                             scaleParams));

        Teuchos::RCP<Teuchos::ParameterList> observersToBuild =
          parameterList(inputParams->sublist("Solver Factories").sublist("Time Integration Observers"));
        if (computeCurrent)
          observersToBuild->set<std::string>("Output Responses", "ON");
        else
          observersToBuild->set<std::string>("Output Responses", "OFF");

        tof->setParameterList(observersToBuild);
      }
      // Create the Rythmos observer factory.
      else if (piroSolver == "Rythmos")
      {
        rof = rcp(new charon::RythmosObserverFactory(stkIOResponseLibrary, globalData->pl,
          scaleParams));
        RCP<ParameterList> observersToBuild =
          parameterList(inputParams->sublist("Solver Factories").
          sublist("Time Integration Observers"));
        // Set whether the observer should output responses.  Right now
        // this only applies to the scalar electric current at the contacts.
        if (computeCurrent)
          observersToBuild->set<std::string>("Output Responses", "ON");
        else
          observersToBuild->set<std::string>("Output Responses", "OFF");

        rof->setParameterList(observersToBuild);
      } // end of creating the Rythmos observer factory

      // Create the NOX observer factory.
      {

        nof = rcp(new charon::NOXObserverFactory(stkIOResponseLibrary,
                                                 scaleParams->varScaleFactors,
                                                 globalData->pl,
                                                 isLOCA,
                                                 writeOnSolveFail));
        RCP<ParameterList> observersToBuild =
          parameterList(inputParams->sublist("Solver Factories").
          sublist("NOX Observers"));

        // Set whether the observer should output responses.  Right now
        // this only applies to the scalar electric current at the contacts.
        observersToBuild->set<bool>("Output Responses", computeCurrent);
        nof->setParameterList(observersToBuild);
      } // end of creating the NOX observer factory
      inputParams->remove("Solver Factories");
    } // end of adding in the application-specific observer factories

    // Check if the equation sets are EFFPG or CVFEM, and select the correct
    // workset factory.
    bool isEFFPG(false), isCVFEM(false);
    checkEqnSetType(*inputParams, isEFFPG, isCVFEM);
    RCP<panzer_stk::WorksetFactory> wf;
    if (isCVFEM)
       wf = rcp(new CVFEM_WorksetFactory());
    else if (isEFFPG)
       wf = rcp(new EFFPG_WorksetFactory());
    else
      wf = rcp(new panzer_stk::WorksetFactory());

    // Create the ModelEvaluatorFactory.
    panzer_stk::ModelEvaluatorFactory<double> meFactory;
    inputParams->sublist("User Data").set("Scaling Parameter Object",
      scaleParams);
    meFactory.setParameterList(inputParams);
    meFactory.setNOXObserverFactory(nof);

    // set transient observers depending on which time integrator is
    // being used
    if (piroSolver == "Rythmos")
      meFactory.setRythmosObserverFactory(rof);
    else if (piroSolver == "Tempus")
      meFactory.setTempusObserverFactory(tof);
      
    meFactory.setUserWorksetFactory(wf);
    meFactory.buildObjects(comm, globalData, eqSetFactory, bcFactory,
      cmFactory);

    // Determine the contact (sideset ID, element block ID) pairs, as well as
    // which of those pertain to current constraints, if any.
    vector<pair<string, string>> contactSides;
    vector<string> contactStrategy;
    determineContactSides(inputParams->sublist("Boundary Conditions"),
      currentConstraints, contactSides, contactStrategy);

    // Add the current reponses and parameters.
    vector<string> responseNames;

    // Get the appropriate suffixes for frequency domain simulation
    std::vector<std::string> fd_response_suffixes = {};
    //bool isSmallSignal = freqDomParamsRCP->queryIsSmallSignal();
    
    std::vector<double> inputFreqs = {};  // in [Hz]
    bool isSingleFreq = false;  // determine single-frequ contact displacement current calculation
    double frequency = 0.0;     

    if(isFreqDom)
    {
      bool td_response = false;
      if(td_response)
        fd_response_suffixes = fd_suffixes;
      if(!td_response)
      {
        for(auto h : *(freqDomParamsRCP->getRemappedHarmonics()))
        {
          fd_response_suffixes.push_back("_CosH" + std::to_string(h) + "_");
          if(h > 0.5)
            fd_response_suffixes.push_back("_SinH" + std::to_string(h) + "_");
        }
        
        // get unremapped frequency in [Hz]   
        for (auto h : *(freqDomParamsRCP->getUnRemappedHarmonics()))
          inputFreqs.push_back(h);

        // get the frequency for a single-frequency FD analysis
        if (inputFreqs.size() == 2)
        {
          isSingleFreq = true; 
          frequency = inputFreqs[1];  // [0] corresponds to DC
        } 
      }  
    }
    else if(!isFreqDom)
      fd_response_suffixes = {""};  // include no suffix if not frequency domain

    if (computeCurrent)
    {
      addCurrentResponses(contactSides, contactStrategy, currentHighOrder, scaleParams,
                          currentConstraints, writeCurrentGraph, meFactory, rof, tof,
                          nof, cmFactory, responseNames, fd_response_suffixes, isFreqDom,
                          isSingleFreq, isTransient, frequency);
    }

    // Build the physics object.
    RCP<Thyra::ModelEvaluator<double>> physics = meFactory.getPhysicsModelEvaluator();

    // If Mixed Mode was set to use the Current strategy, then set up the CoupledModelEvaluator decorator.
    if (mixedModeCouplingME)
    {
      Teuchos::RCP<panzer::GlobalIndexer> ugi = meFactory.getGlobalIndexer();
      Teuchos::RCP<panzer_stk::STK_Interface> mesh = meFactory.getMesh();
      xyceCouplingPL->set<bool>("Mixed Mode", true);
      xyceCouplingPL->set<RCP<panzer::GlobalIndexer> >("Unique Global Indexer", ugi);
      xyceCouplingPL->set<RCP<panzer::GlobalData> >("Global Data", globalData);
      xyceCouplingPL->set<Teuchos::ParameterList>("Boundary Conditions", inputParams->sublist("Boundary Conditions"));
      physics = rcp(new charon::CoupledModelEvaluator<double>(physics,
        *rcp_dynamic_cast<const MpiComm<int>>(comm, true)->getRawMpiComm(),
	xyceCouplingPL, currentConstraints, mesh));
    }

    // If we have any current constraints, wrap the physics object in a
    // CurrentConstraintModelEvaluator.
    bool useHouseholder = false;
    if (inputParams->sublist("Solution Control").get<std::string>("Piro Solver") == "NOX-Constrained" ||
        inputParams->sublist("Solution Control").get<std::string>("Piro Solver") == "LOCA-Constrained")
      useHouseholder = true;
    if (not currentConstraints.empty())
    {
      if (useHouseholder) {
        physics = rcp(new charon::CurrentConstraintModelEvaluatorLOCA<double>(physics,
          *rcp_dynamic_cast<const MpiComm<int>>(comm, true)->getRawMpiComm(),
	  currentConstraints, meFactory.getMesh()->getDimension(), xyceCouplingPL));
      }
      else {
        physics = rcp(new CurrentConstraintModelEvaluator<double>(physics,
          *rcp_dynamic_cast<const MpiComm<int>>(comm, true)->getRawMpiComm(),
          currentConstraints, meFactory.getMesh()->getDimension()));
      }
    } // end if we have any current constraints

    // Set the model evaluator in the object factories so it can be used in
    // observers for outputting responses.
    nof->setModelEvaluator(physics);

    if (not rof.is_null())
      rof->setModelEvaluator(physics);

    if (not tof.is_null())
      tof->setModelEvaluator(physics);

    // Create the solver.
    RCP<Thyra::ModelEvaluator<double>> solver;
    if (!useHouseholder)
      solver = meFactory.buildResponseOnlyModelEvaluator(physics, globalData);

    // Setup up the IO response object.
    {
      RCP<panzer::ResponseLibrary<panzer::Traits>> rLibrary = meFactory.getResponseLibrary();
      vector<RCP<panzer::PhysicsBlock>> physicsBlocks = meFactory.getPhysicsBlocks();
      stkIOResponseLibrary->initialize(*rLibrary);
      ParameterList userData(inputParams->sublist("User Data"));
      userData.set<int>("Workset Size",
        inputParams->sublist("Assembly").get<int>("Workset Size"));
      stkIOResponseLibrary->buildResponseEvaluators(physicsBlocks, cmFactory,
        inputParams->sublist("Closure Models"), userData);
    } // end of setting up the IO response object

    // Solve the system.
    if (useHouseholder) {
      solve_steadystate_constraint_problem(inputParams,physics,currentConstraints,contactSides,out,pout,meFactory,*nof);
    }
    else {
      // Set the inputs and outputs.
      InArgs  inArgs  = solver->createInArgs();
      OutArgs outArgs = solver->createOutArgs();

      RCP<Thyra::VectorBase<double>> gx = createMember(physics->get_x_space());
      outArgs.set_g(outArgs.Ng() - 1, gx); // why do we need to subtract 1 here?

      // Now try solve the problem and return the responses.
      try
      {
        solver->evalModel(inArgs, outArgs);
      }
      catch (exception& e)
      {
        *pout << "SOLVE FAILURE: " << e.what() << endl;
        if (currentConstraints.size() > 1)
          *pout << "This is likely due to having an over-constrained system.  "
               << "Double-check your boundary conditions to ensure you don't "
               << "have any duplicate constraints." << endl;
        status = 1;
      } // end of trying to solve the problem and return the responses

      // Check for convergence by casting the ME to the correct
      // underlying solver type and querying. Not supported for
      // Rythmos.
      auto nox_check = Teuchos::rcp_dynamic_cast<Piro::NOXSolver<double>>(solver);
      if (nox_check) {
        if (nox_check->getSolver()->getNOXSolver()->getStatus() != NOX::StatusTest::Converged)
          status = 2;
      }
      auto loca_check = Teuchos::rcp_dynamic_cast<Piro::LOCASolver<double>>(solver);
      if (loca_check) {
        if (loca_check->getStepper()->getIteratorStatus() != LOCA::Abstract::Iterator::Finished)
          status = 2;
      }
      auto loca_adapt_check = Teuchos::rcp_dynamic_cast<Piro::LOCAAdaptiveSolver<double>>(solver);
      if (loca_adapt_check) {
        if (loca_adapt_check->getStepper()->getIteratorStatus() != LOCA::Abstract::Iterator::Finished)
          status = 2;
      }
      auto tempus_check = Teuchos::rcp_dynamic_cast<Tempus::Integrator<double>>(solver);
      if (tempus_check) {
        if (tempus_check->getStatus() != Tempus::PASSED)
          status = 2;
      }
    } // end of solving the system
  } // end of the main try block
  catch (exception& e)
  {
    *pout << "*********** Caught Exception: Begin Error Report ***********"
         << endl << e.what() << endl
         << "************ Caught Exception: End Error Report ************"
         << endl;
    status = -1;
  }
  catch (string& msg)
  {
    *pout << "*********** Caught Exception: Begin Error Report ***********"
         << endl << msg << endl
         << "************ Caught Exception: End Error Report ************"
         << endl;
    status = -1;
  }
  catch (...)
  {
    *pout << "*********** Caught Exception: Begin Error Report ***********"
         << endl << "Caught UNKOWN exception" << endl
         << "************ Caught Exception: End Error Report ************"
         << endl;
    status = -1;
  } // end of all the catch statements for the main try block
  if (status == 0)
  {
    stackedTimer->stopBaseTimer();
    if (outputTimings) {
      auto filename = inputFileName;
      auto search = filename.rfind(".xml");
      if (search != std::string::npos)
        filename = filename.substr(0,filename.size()-4);
      search = filename.rfind(".yaml");
      if (search != std::string::npos)
        filename = filename.substr(0,filename.size()-5);
      search = filename.rfind(".inp");
      if (search != std::string::npos)
        filename = filename.substr(0,filename.size()-4);

      filename = filename + ".timings";
      std::fstream ofs(filename.c_str(),std::fstream::out|std::fstream::trunc);

      //TimeMonitor::summarize(*out, false, true, false);
      Teuchos::StackedTimer::OutputOptions options;
      options.output_fraction = true;
      options.output_minmax = true;
      options.output_histogram = true;
      options.num_histogram = 5;
      stackedTimer->report(ofs, Teuchos::DefaultComm<int>::getComm(), options);

      ofs.close();
    }

    *out << "Charon run completed." << endl;
  }
  else
  {
    // All processors exit in the event that one or more processors
    // throws a fatal exception
    mpiSession.abort();
  }

  Kokkos::finalize();

  return status;

} // end of main()

///////////////////////////////////////////////////////////////////////////////
//
//  getCurrentConstraintBCs()
//
///////////////////////////////////////////////////////////////////////////////
void
getCurrentConstraintBCs(
  const Teuchos::ParameterList&  pl,
  charon::CurrentConstraintList& currentConstraints)
{
  using std::invalid_argument;
  using std::string;
  using Teuchos::getValue;
  using Teuchos::ParameterEntry;
  using Teuchos::ParameterList;

  // Loop over the specified boundary conditions.
  currentConstraints.clear();
  for (auto itr = pl.begin(); itr != pl.end(); ++itr)
  {
    // Get the ParameterList for this boundary condition.
    const ParameterEntry& entry = pl.entry(itr);
    TEUCHOS_ASSERT(entry.isList());
    const ParameterList& bc = getValue<ParameterList>(entry);

    // Make sure the given BC's Strategy is either Constant Current or Resistor
    // Contact.
    const string& strategy(bc.get<string>("Strategy"));
    if ((strategy == "Constant Current") or (strategy == "Resistor Contact"))
    {
      // Grab all the bits of information we need to construct the current
      // constraint.
      string sidesetId(bc.get<string>("Sideset ID")),
        elementBlockId(bc.get<string>("Element Block ID"));
      const ParameterList& data = bc.sublist("Data");
      double contactLength(1), contactArea(1e4), initialVoltage(0);
      if (data.isParameter("Simulation Contact Length"))
        contactLength = data.get<double>("Simulation Contact Length");
      if (data.isParameter("Device Contact Area"))
        contactArea = data.get<double>("Device Contact Area");
      if (data.isParameter("Initial Voltage"))
        initialVoltage = data.get<double>("Initial Voltage");
      string baseDopingType("");
      if (data.isParameter("Base Doping Type"))
        baseDopingType = data.get<string>("Base Doping Type");

      // Validate those bits of information.
      TEUCHOS_TEST_FOR_EXCEPTION(sidesetId == "", invalid_argument,
        "Error:  Attempting to create a current constraint with an empty "    \
        "Sideset ID.")
      TEUCHOS_TEST_FOR_EXCEPTION(elementBlockId == "", invalid_argument,
        "Error:  Attempting to create a current constraint with an empty "    \
        "Element Block ID.")
      TEUCHOS_TEST_FOR_EXCEPTION(contactLength <= 0, invalid_argument,
        "Error:  Attempting to create a current constraint with Simulation "  \
        "Contact Length = " << contactLength << " (must be > 0).")
      TEUCHOS_TEST_FOR_EXCEPTION(contactArea <= 0, invalid_argument,
        "Error:  Attempting to create a current constraint with Device "      \
        "Contact Area = " << contactArea << " (must be > 0).")
      TEUCHOS_TEST_FOR_EXCEPTION(
        (data.isParameter("Simulation Contact Length")) and
        (not data.isParameter("Device Contact Area")), invalid_argument,
        "Error:  Attempting to create a current constraint with Simulation "  \
        "Contact Length = " << contactLength << ", but no Device Contact "    \
        "Area.")
      TEUCHOS_TEST_FOR_EXCEPTION(
        (not data.isParameter("Simulation Contact Length")) and
        (data.isParameter("Device Contact Area")), invalid_argument,
        "Error:  Attempting to create a current constraint with Device "      \
        "Contact Area = " << contactArea << ", but no Simulation Contact "    \
        "Length.")
      TEUCHOS_TEST_FOR_EXCEPTION((baseDopingType != "") and
        (baseDopingType != "Acceptor") and (baseDopingType != "Donor"),
        invalid_argument, "Error:  Attempting to create a current "           \
        "constraint with Base Doping Type = " << baseDopingType << ", which " \
        "is neither Acceptor nor Donor.")

      // If it's a constant current BC, we need that current value.  Otherwise,
      // we need the resistor value and applied voltage.
      if (strategy == "Constant Current")
      {
        double currentValue(data.get<double>("Current Value"));
        currentConstraints.addConstantCurrent(currentValue, sidesetId,
          contactLength, contactArea, initialVoltage, elementBlockId);
      }
      else if (strategy == "Resistor Contact")
      {
        double resistorValue(data.get<double>("Resistor Value")),
          appliedVoltage(data.get<double>("Applied Voltage"));
        TEUCHOS_TEST_FOR_EXCEPTION(resistorValue <= 0, invalid_argument,
          "Error:  Attempting to create a resistor contact constraint with "  \
          "Resistor Value = " << resistorValue << " (must be > 0).")
        currentConstraints.addResistorContact(resistorValue, appliedVoltage,
          sidesetId, contactLength, contactArea, initialVoltage,
          elementBlockId);
      } // end if (strategy == "Constant Current" or "Resistor Contact")
    } // end if this is a BC strategy we support
  } // end loop over the specified boundary conditions.
} // end of getCurrentConstraintBCs()

///////////////////////////////////////////////////////////////////////////////
//
//  buildParameters()
//
///////////////////////////////////////////////////////////////////////////////
void
buildParameters(
  charon::CurrentConstraintList& currentConstraints,
  Teuchos::ParameterList&        inputParams)
{
  using std::size_t;
  using std::string;
  using std::stringstream;
  using std::vector;
  using Teuchos::ParameterList;

  // Create a voltage parameter corresponding to each of the current
  // constraints.
  vector<string> names;
  vector<double> values;
  for (const auto& constraint : currentConstraints)
  {
    names.push_back(constraint->sidesetId() + constraint->type() + "Voltage");
    values.push_back(constraint->initialVoltage());
  } // end loop over currentConstraints

  // Modify or build the "Active Parameters" sublist, registering the
  // parameters with Panzer.
  ParameterList& activeParams = inputParams.sublist("Active Parameters");
  int numParameters(activeParams.get<int>("Number of Parameter Vectors", 0));
  for (size_t i(0); i < names.size(); ++i)
  {
    // Add in an "Active Parameter" for each parameter in the names vector.
    stringstream ss;
    ss << "Parameter Vector " << i + numParameters;
    currentConstraints[i]->parameterIndex(i + numParameters);
    ParameterList& current = activeParams.sublist(ss.str());
    current.set<int>("Number", 1);
    current.set<string>("Parameter 0", names[i]);
    current.set<double>("Initial Value 0", values[i]);
  }
  activeParams.set<int>("Number of Parameter Vectors", numParameters +
    names.size());
  inputParams.sublist("User Data").set<int>("Tangent Dimension",
    names.size());
} // end of buildParameters()

///////////////////////////////////////////////////////////////////////////////
//
//  getScalingInfo()
//
///////////////////////////////////////////////////////////////////////////////
void
getScalingInfo(
  Teuchos::ParameterList& inputPList,
  double&                 tempScale,
  double&                 concScale,
  double&                 diffScale,
  bool&                   userMeshScaling,
  double&                 meshScale,
  bool&                   doOutputUnscaling,
  bool&                   doInputVariableScaling)
{
  using   charon::Material_Properties;
  using   std::logic_error;
  using   std::runtime_error;
  using   std::string;
  using   std::vector;
  using   Teuchos::getValue;
  using   Teuchos::ParameterEntry;
  using   Teuchos::ParameterList;
  using   Teuchos::RCP;

  // Initialize the output variables.
  tempScale = 0;
  concScale = 0;
  diffScale = 0;
  meshScale = 0;
  userMeshScaling        = false;
  doOutputUnscaling      = false;
  doInputVariableScaling = false;

  // Get the various sublists we need from the input ParameterList.
  const ParameterList& exoParamList =
    inputPList.sublist("Mesh").sublist("Exodus File");
  const ParameterList& pbParamList = inputPList.sublist("Physics Blocks");
  const ParameterList& icParamList = inputPList.sublist("Initial Conditions");
  ParameterList& outParamList = inputPList.sublist("Output");
  vector<bool> isTempConst;
  int count(0);

  // Loop over the equation sets.
  for (auto eq = pbParamList.begin(); eq != pbParamList.end(); ++eq)
  {
    ++count;
    const ParameterEntry& entry = eq->second;
    const ParameterList&  pList = getValue<ParameterList>(entry);

    // Allow only one equation set per physics block.
    if (pList.numParams() > 1)
      TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error, "The physics block "
        << pList.name() << " has more than one equation set!")

    const ParameterList& eqSetPList = pList.sublist("child0");
    string eqnSetType = eqSetPList.get<string>("Type");

    // Set the isTempConst value for each eqnSetType.
    if ((eqnSetType == "Laplace"                  )  or
        (eqnSetType == "SGCVFEM Laplace"          )  or
        (eqnSetType == "NLPoisson"                )  or
        (eqnSetType == "SGCVFEM NLPoisson"        )  or
        (eqnSetType == "Heterojunction"           )  or
        (eqnSetType == "Drift Diffusion"          )  or
        (eqnSetType == "EFFPG Drift Diffusion"    )  or
        (eqnSetType == "SGCVFEM Drift Diffusion"  )  or
        (eqnSetType == "SGCharon1 Drift Diffusion")  or
        (eqnSetType == "DDIon"                    )  or
        (eqnSetType == "Frequency Domain"         ))
      isTempConst.push_back(false);
    else if ((eqnSetType == "DDLattice"         )  or
             (eqnSetType == "Lattice"           )  or
             (eqnSetType == "DDIonLattice"      )  or
             (eqnSetType == "EFFPG DDIonLattice"))
      isTempConst.push_back(true);
    else
      TEUCHOS_TEST_FOR_EXCEPTION(true, logic_error,
        "Error:  Invalid equation set type!");

    // Check if the elements of isTempConst have the same value.
    if ((count > 1) and (isTempConst[count - 1] != isTempConst[count - 2]))
      TEUCHOS_TEST_FOR_EXCEPTION(true, logic_error, "Error:  Equation sets "  \
        "have conflicting temperature profiles !")
  } // end loop over the equation sets

  // Get any scale factor specified by the user.
  if (exoParamList.isParameter("Scale Factor"))
  {
    userMeshScaling = true;
    meshScale = exoParamList.get<double>("Scale Factor");
  } // end if (exoParamList.isParameter("Scale Factor"))
  if (icParamList.isParameter("Scale Input Exodus Variables"))
    doInputVariableScaling =
      icParamList.get<bool>("Scale Input Exodus Variables");

  // See if the user wants to unscale the output in the Exodus file.
  if (outParamList.isParameter("Unscale Variables"))
  {
    string paramName("Unscale Variables");
    doOutputUnscaling = outParamList.get<bool>(paramName);

    // Panzer will complain about this parameter, which is unknown to Panzer,
    // so remove it prior to any Panzer checks.
    outParamList.remove(paramName);
  } // end if (outParamList.isParameter("Unscale Variables"))

  // If you scale variables from the input file, then you currently have to
  // scale the output variables.
  if (doInputVariableScaling)
    doOutputUnscaling = true;

  auto& matProperty = Material_Properties::getInstance();
  const ParameterList& cmParamList = inputPList.sublist("Closure Models");

  // Determine the value of tempScale, or using Lattice Temperature as the
  // scaling factor if none is specified.
  tempScale = matProperty.getPropertyValue("Lattice Temperature");
  if ((isTempConst[0]) and (cmParamList.isParameter("Temperature Scaling")))
    tempScale = cmParamList.get<double>("Temperature Scaling");
  else if (cmParamList.isParameter("Lattice Temperature"))
    tempScale = cmParamList.get<double>("Lattice Temperature");

  // Get the concentration scaling, either from that specified by the user, or
  // from Material_Properties.
  if (cmParamList.isParameter("Concentration Scaling"))
    concScale = cmParamList.get<double>("Concentration Scaling");
  else // if (not cmParamList.isParameter("Concentration Scaling"))
    concScale = matProperty.getPropertyValue("Concentration Scaling");

  string refMaterial("");
  bool isRefMatGiven(false);
  if (cmParamList.isParameter("Reference Material"))
  {
    refMaterial = cmParamList.get<string>("Reference Material");
    isRefMatGiven = true;
  } // end if (cmParamList.isParameter("Reference Material"))

  // When Reference Material is given, set diffScale = its Electron Diffusion
  // Coefficient.
  if (isRefMatGiven)
    diffScale = matProperty.getPropertyValue(refMaterial,
      "Electron Diffusion Coefficient");
  else // if (not isRefMatGiven)
  {
    // Otherwise, get the first material in the closure models.
    for (auto cm = cmParamList.begin(); cm != cmParamList.end(); ++cm)
    {
      // Check if the entry is a ParameterList.
      if (cm->second.isList())
      {
        const ParameterEntry& entry = cm->second;
        const ParameterList& pList = getValue<ParameterList>(entry);
        string firstMaterial = pList.get<string>("Material Name");
        const string matType = matProperty.getMaterialType(firstMaterial);

        // Set diffScale when the first material is a Semiconductor.
        diffScale = matProperty.getPropertyValue(
          (matType == "Semiconductor") ? firstMaterial : "Silicon",
          "Electron Diffusion Coefficient");
        break;
      } // end if the entry is a ParameterList
    } // end loop over the closure models
  } // end if (isRefMatGiven) or not
} // end of getScalingInfo()

///////////////////////////////////////////////////////////////////////////////
//
//  checkForEmpiricalModel()
//
///////////////////////////////////////////////////////////////////////////////
void
checkForEmpiricalModel(
  Teuchos::ParameterList&                     inputPList,
  Teuchos::RCP<charon::Scaling_Parameters>    scaleParams,
  Teuchos::RCP<panzer::GlobalData>            globalData,
  Teuchos::RCP<charon::EmpiricalDamage_Data> /* damageData */)
{
  using   charon::Delta_PulseDamage_Spec;
  using   charon::File_PulseDamage_Spec;
  using   charon::Gaussian_PulseDamage_Spec;
  using   charon::GaussianLog_PulseDamage_Spec;
  using   charon::PulseDamage_Spec;
  using   charon::Square_PulseDamage_Spec;
  using   charon::EmpiricalDamage_Data;
  using   std::endl;
  using   std::logic_error;
  using   std::string;
  using   Teuchos::getValue;
  using   Teuchos::ParameterEntry;
  using   Teuchos::ParameterList;
  using   Teuchos::RCP;
  using   Teuchos::rcp;

  bool ebEmpiricalOn(false);
  bool cbEmpiricalOn(false);

  // Get the necessary parameter lists
  ParameterList& bcPL = inputPList.sublist("Boundary Conditions");
  ParameterList& cmPL = inputPList.sublist("Closure Models");
  ParameterList& rythmosPL =
    inputPList.sublist("Solution Control").sublist("Rythmos");

  // Check to see if the empirical model has been called out.
  if (cmPL.isSublist("Empirical Defect Recombination"))
  {
    double t0(scaleParams->scale_params.t0);
    ParameterList& emParamList =
      cmPL.sublist("Empirical Defect Recombination");

    RCP<PulseDamage_Spec> damageSpec;

    RCP<EmpiricalDamage_Data> damageData =
      rcp(new EmpiricalDamage_Data);

    // Check for a user-specified pulse data file.
    if (emParamList.isParameter("pulse data file"))
    {
      ebEmpiricalOn = true;
      damageSpec = rcp(new File_PulseDamage_Spec(emParamList, t0));
    }
    else if (emParamList.isParameter("pulse type"))
    {
      ebEmpiricalOn = true;
      string pulseType = emParamList.get<string>("pulse type");
      PulseDamage_Spec::Shape pShape = PulseDamage_Spec::shape(pulseType);
      switch (pShape)
      {
        case PulseDamage_Spec::Delta:
          damageSpec = rcp(new Delta_PulseDamage_Spec(emParamList, t0));
          break;
        case PulseDamage_Spec::Square:
          damageSpec = rcp(new Square_PulseDamage_Spec(emParamList, t0));
          break;
        case PulseDamage_Spec::Gaussian:
          damageSpec = rcp(new Gaussian_PulseDamage_Spec(emParamList, t0));
          break;
        case PulseDamage_Spec::GaussianLog:
          damageSpec = rcp(new GaussianLog_PulseDamage_Spec(emParamList, t0));
          break;
        default:
          TEUCHOS_TEST_FOR_EXCEPTION(true, logic_error,
            "Error:  Unknown pulse shape specified.")
      } // end of switch (pShape)
    }
    else // if ((not emParamList.isParameter("pulse data file"))  and
         //     (not emParamList.isParameter("pulse type")    ))
      TEUCHOS_TEST_FOR_EXCEPTION(true, logic_error, "Error:  Empirical "
        << "Defect Recombination is on but no pulse type was specified."
        << endl << "One of the following must be specified:" << endl
        << "   pulse type" << endl
        << "   pulse data file")

    // Add the damage spec object to the ParameterList for use in the
    // evaluators.
    emParamList.set("damage spec object", damageSpec);

    rythmosPL.sublist("Rythmos Step Control Strategy").
      set("Break Points", damageSpec->rythmosBPlist());

    // If the empirical model is on then find contact values/models in
    // the input file
    if (ebEmpiricalOn || cbEmpiricalOn)
    {
      for (auto itr=bcPL.begin(); itr != bcPL.end(); ++itr)
      {
        ParameterEntry const& entry = bcPL.entry(itr);
        TEUCHOS_ASSERT(entry.isList());
        ParameterList const& bcList = getValue<ParameterList>(entry);
        string const& contact = bcList.get<string>("Sideset ID");
        ParameterList const& dataList = bcList.sublist("Data");

        bool spec_error(false);
        std::ostringstream err_msg;
        if (contact == "emitter" ||
            contact == "collector" ||
            contact == "base")
        {
          std::string strategy = bcList.get<string>("Strategy");
          double c_voltage_(0.0);
          if (strategy == "Ohmic Contact" ||
              strategy == "BJT1D Base Contact")
          {
            c_voltage_ = dataList.get<double>("Voltage");
            damageData->addDirichletContact(contact,c_voltage_);
          }
          else if (strategy == "Constant Current")
          {
            c_voltage_ = dataList.get<double>("Initial Voltage");
            damageData->addConstantCurrentContact(contact,c_voltage_,globalData);
          }
          else // Unrecognized/unimplemented strategy
          {
            spec_error = true;
            err_msg << "For the " << contact << " contact "
                    << "boundary condition specification an "
                    << "unknown strategy:" << std::endl
                    << "   " << strategy << std::endl
                    << "was encountered. When using the empirical radiation damage model presently" << std::endl
                    << "only the following strategies are supported:" << std::endl
                    << "   Ohmic Contact" << std::endl
                    << "   Constant Current" << std::endl
                    << "   BJT1D Base Contact";
          }
        }

        TEUCHOS_TEST_FOR_EXCEPTION(spec_error, std::logic_error, err_msg.str());
      }
    }

    emParamList.set("empirical damage data", damageData);

  } // end if the empirical model has been called out

} // end of checkForEmpiricalModel()

///////////////////////////////////////////////////////////////////////////////
//
//  checkForClusterModel()
//
///////////////////////////////////////////////////////////////////////////////
bool
checkForClusterModel(
  Teuchos::ParameterList&                    cmParamList,
  Teuchos::RCP<int>&                         /* clusterManager */,
  Teuchos::RCP<charon::clusterInterpolator>& clusterInterp)
{
  using   charon::clusterInterpolator;
  using   std::string;
  using   Teuchos::ParameterList;
  using   Teuchos::RCP;
  using   Teuchos::rcp;

  // Check to see if the cluster model has been called out.
  for (auto itr = cmParamList.begin(); itr != cmParamList.end(); ++itr)
  {
    if (cmParamList.isSublist(itr->first))
    {
      ParameterList& subParamList = cmParamList.sublist(itr->first);
      if (subParamList.isSublist("Defect Cluster Recombination"))
      {
        ParameterList& dcParamList =
          subParamList.sublist("Defect Cluster Recombination");
        clusterInterp = rcp(new clusterInterpolator);
        dcParamList.set<RCP<clusterInterpolator>>("cluster interpolator",
          clusterInterp);
        const string& inputType(dcParamList.get<string>("Input File Type"));
        if (inputType == "IN SITU")
        {
	  std::cout<<" ERROR!!!  In Situ clusters have been requested, but this build of Charon is not enabled for such things. Exiting.  "<<std::endl;
	  return true;
	}
        return false;
      } // end if the "Defect Cluster Recombination" sublist exists
    } // end if this is a sublist
  } // end loop over cmParamList
  return false;
} // end of checkForClusterModel()

///////////////////////////////////////////////////////////////////////////////
//
//  getPulses()
//
///////////////////////////////////////////////////////////////////////////////
std::vector<double>
getPulses(
  std::string pulseFilename)
{
  std::vector<double> localPulses;
  std::ifstream       ifile(pulseFilename);
  if (not ifile.is_open())
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Hold it, bubba.  I "  \
      "couldn't open the requested pulses filename, " << pulseFilename << ".")
  double time, magnitude;
  while (ifile >> time >> magnitude)
    localPulses.push_back(time);
  ifile.close();
  return localPulses;
} // end of getPulses()

///////////////////////////////////////////////////////////////////////////////
//
//  getPowerOfTen()
//
///////////////////////////////////////////////////////////////////////////////
int
getPowerOfTen(
  double number)
{
  int power(0);
  if (number < 1)
  {
    while (number < 1)
    {
      ++power;
      number *= 10;
    }
    power *= -1;
  }
  else // if (number >= 1)
  {
    while (number > 1)
    {
      ++power;
      number /= 10;
    }
    --power;
  }
  return power;
} // end of getPowerOfTen()

///////////////////////////////////////////////////////////////////////////////
//
//  scaleTempusTimes()
//
///////////////////////////////////////////////////////////////////////////////
void
scaleTempusTimes(Teuchos::ParameterList& i_param_list,
                 Teuchos::RCP<charon::Scaling_Parameters> scaleParams)
{
  auto out  = Teuchos::rcp(new Teuchos::FancyOStream(Teuchos::rcp(&std::cout, false)));
  int nprocs;
  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
  if (nprocs > 1)
  {
    out->setShowProcRank(true);
    out->setOutputToRootOnly(0);
  } // end if (mpiSession.getNProc() > 1)

  // Retrieve the time scaling [s].
  double t0 = scaleParams->scale_params.t0;
  *out << "Time scaling t0 = " << t0 << " in [seconds]" << std::endl;

  // Return directly when the Rythmos section is not specified.
  if (!(i_param_list.sublist("Solution Control").isSublist("Tempus")))
  {
    *out << "Tempus section is NOT specified; not a transient simulation; return!" << std::endl;
    return;
  }

  // Otherwise, get the Rythmos ParameterList.
  Teuchos::ParameterList& t_param_list =
    i_param_list.sublist("Solution Control").sublist("Tempus").sublist("Default Integrator").sublist("Time Step Control");


  // Scale and re-set final time
  if (t_param_list.isParameter("Final Time"))
  {
    double timeInSecs(t_param_list.get<double>("Final Time"));
    double scaledTime(timeInSecs / t0);
    t_param_list.set<double>("Final Time", scaledTime);
    *out << "Final Time = " << timeInSecs << " [s], scaled value = "
         << scaledTime << std::endl;
  }
  else 
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error,
                               "Tempus->Final Time is NOT specified!");

  // scale and re-set initial time step size
  if (t_param_list.isParameter("Initial Time Step"))
  {
    double timeInSecs(t_param_list.get<double>("Initial Time Step"));
    double scaledTime(timeInSecs / t0);
    t_param_list.set<double>("Initial Time Step", scaledTime);
    *out << "Initial Time Step = " << timeInSecs << " [s], scaled value = "
         << scaledTime << std::endl;
  }
  else 
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error,
                               "Tempus->Initial Time Step is NOT specified!");

  // Get the time step control strategy sublist to determine if it is a
  // variable or constant step problem
  Teuchos::ParameterList& tscs_param_list =
    t_param_list.sublist("Time Step Control Strategy");

  if (tscs_param_list.isParameter("Strategy Type"))
  {
    if (tscs_param_list.get<std::string>("Strategy Type") == "Basic VS")
    {
      double min_ts = t_param_list.get<double>("Minimum Time Step");
      min_ts = min_ts / t0;
      t_param_list.set<double>("Minimum Time Step", min_ts);

      double max_ts = t_param_list.get<double>("Maximum Time Step");
      max_ts = max_ts / t0;
      t_param_list.set<double>("Maximum Time Step", max_ts);
    }
    else if (tscs_param_list.get<std::string>("Strategy Type") == "Constant")
    {
      // no params needed for "Constant" step strategy
    }
    else
    {
      std::ostringstream err_msg;
      err_msg << "Tempus->unknown Strategy Type \""
              << tscs_param_list.get<std::string>("Strategy Type")
              << "\" for time step control strategy";
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error, err_msg.str());
    }
  }
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::runtime_error,
                               "Tempus->No time step control strategy section found!");

}

///////////////////////////////////////////////////////////////////////////////
//
//  scaleRythmosTimes()
//
///////////////////////////////////////////////////////////////////////////////
void
scaleRythmosTimes(
  Teuchos::ParameterList&                  inputPList,
  Teuchos::RCP<charon::Scaling_Parameters> scaleParams)
{
  using   std::cout;
  using   std::endl;
  using   std::runtime_error;
  using   std::setprecision;
  using   std::size_t;
  using   std::string;
  using   std::stringstream;
  using   std::vector;
  using   Teuchos::ParameterList;
  using   Teuchos::ParameterEntry;
  using   Teuchos::FancyOStream;
  using   Teuchos::RCP;
  using   Teuchos::rcp;

  auto out  = rcp(new FancyOStream(rcp(&cout, false)));
  int nprocs;
  MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
  if (nprocs > 1)
  {
    out->setShowProcRank(true);
    out->setOutputToRootOnly(0);
  } // end if (mpiSession.getNProc() > 1)

  // Retrieve the time scaling [s].
  double t0 = scaleParams->scale_params.t0;
  *out << "Time scaling t0 = " << t0 << " in [seconds]" << endl;

  // Return directly when the Rythmos section is not specified.
  if (not inputPList.sublist("Solution Control").isSublist("Rythmos"))
  {
    *out << "Rythmos section is NOT specified; not a transient simulation; return!" << endl;
    return;
  } // end if the Rythmos section is not specified

  // Otherwise, get the Rythmos ParameterList.
  ParameterList& rythmosPList =
    inputPList.sublist("Solution Control").sublist("Rythmos");


  // Scale and re-set Final Time.
  if (rythmosPList.isParameter("Final Time"))
  {
    double timeInSecs(rythmosPList.get<double>("Final Time"));
    double scaledTime(timeInSecs / t0);
    inputPList.sublist("Solution Control").sublist("Rythmos").
      set<double>("Final Time", scaledTime);
    *out << "Final Time = " << timeInSecs << " [s], scaled value = "
         << scaledTime << endl;
  }
  else // if (not rythmosPList.isParameter("Final Time"))
    TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error,
      "Rythmos->Final Time is NOT specified!")

  // Retrieve the Stepper Type.
  const string stepType(rythmosPList.get<string>("Stepper Type"));

  // Scale the times.
  if (stepType == "Backward Euler")
  {
    if (not rythmosPList.isSublist("Rythmos Integration Control"))
      TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error, "Rythmos Integration "  \
        "Control parameterlist must be specified for Backward Euler!")

    // Scale and re-set Fixed dt.
    if (rythmosPList.sublist("Rythmos Integration Control").
      isParameter("Fixed dt"))
    {
      double timeInSecs = rythmosPList.sublist("Rythmos Integration Control").
        get<double>("Fixed dt");
      double scaledTime(timeInSecs / t0);
      inputPList.sublist("Solution Control").sublist("Rythmos").
        sublist("Rythmos Integration Control").
        set<double>("Fixed dt", scaledTime);
      *out << "Fixed dt = " << timeInSecs << " [s], scaled value = "
           << scaledTime << endl;
    }
    else
      TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error,
        "Fixed dt must be specified for Backward Euler!")

    // Scale and re-set Max dt.
    if (rythmosPList.sublist("Rythmos Integration Control").
      isParameter("Max dt"))
    {
      double timeInSecs = rythmosPList.sublist("Rythmos Integration Control").
        get<double>("Max dt");
      double scaledTime(timeInSecs / t0);
      inputPList.sublist("Solution Control").sublist("Rythmos").
        sublist("Rythmos Integration Control").
        set<double>("Max dt", scaledTime);
      *out << "Max dt = " << timeInSecs << " [s], scaled value = "
           << scaledTime << endl;
    }
  }
  else if (stepType == "BDF")
  {
    if (not rythmosPList.isParameter("Step Control Strategy Type"))
      TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error,
        "Step Control Strategy Type parameter must be specified for BDF!")
    if (not rythmosPList.isSublist("Rythmos Step Control Strategy"))
      TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error, "Rythmos Step Control " \
        "Strategy parameterlist must be specified for BDF!")
    if (rythmosPList.get<string>("Step Control Strategy Type") ==
      "ImplicitBDFRamping")
    {
      ParameterList& rythStepCtrl =
        rythmosPList.sublist("Rythmos Step Control Strategy");

      // Scale and re-set Initial Step Size.
      if (rythStepCtrl.isParameter("Initial Step Size"))
      {
        double timeInSecs(rythStepCtrl.get<double>("Initial Step Size")),
          scaledTime(timeInSecs / t0);
        inputPList.sublist("Solution Control").sublist("Rythmos").
          sublist("Rythmos Step Control Strategy").
          set<double>("Initial Step Size", scaledTime);
        *out << "Initial Step Size = " << timeInSecs
             << " [s], scaled value = " << scaledTime << endl;
      }
      else
        TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error,
          "Initial Step Size must be specified for ImplicitBDFRamping.")

      // Scale and re-set Min Step Size.
      if (rythStepCtrl.isParameter("Min Step Size"))
      {
        double timeInSecs(rythStepCtrl.get<double>("Min Step Size")),
          scaledTime(timeInSecs / t0);
        inputPList.sublist("Solution Control").sublist("Rythmos").
          sublist("Rythmos Step Control Strategy").
          set<double>("Min Step Size", scaledTime);
        *out << "Min Step Size = " << timeInSecs << " [s], scaled value = "
             << scaledTime << endl;
      }
      else
        TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error,
          "Min Step Size must be specified for ImplicitBDFRamping.")

      // Scale and re-set Max Step Size.
      if (rythStepCtrl.isParameter("Max Step Size"))
      {
        double timeInSecs(rythStepCtrl.get<double>("Max Step Size")),
          scaledTime(timeInSecs / t0);
        inputPList.sublist("Solution Control").sublist("Rythmos").
          sublist("Rythmos Step Control Strategy").
          set<double>("Max Step Size", scaledTime);
        *out << "Max Step Size = " << timeInSecs << " [s], scaled value = "
             << scaledTime << endl;
      }
      else
        TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error,
          "Max Step Size must be specified for ImplicitBDFRamping.")

      // Rythmos Integration Control could be specified for BDF.
      if (rythmosPList.isSublist("Rythmos Integration Control"))
      {
        // Scale and re-set Max dt.  The smaller value of Max dt and Max Step
        // Size is used when both are given.
        if (rythmosPList.sublist("Rythmos Integration Control").
          isParameter("Max dt"))
        {
          double timeInSecs =
            rythmosPList.sublist("Rythmos Integration Control").
            get<double>("Max dt");
          double scaledTime(timeInSecs / t0);
          inputPList.sublist("Solution Control").sublist("Rythmos").
            sublist("Rythmos Integration Control").
            set<double>("Max dt", scaledTime);
          *out << "Max dt = " << timeInSecs << " [s], scaled value = "
               << scaledTime << endl;
        }

        // Scale and re-set Fixed dt.
        if (rythmosPList.sublist("Rythmos Integration Control").
          isParameter("Fixed dt"))
        {
          double timeInSecs =
            rythmosPList.sublist("Rythmos Integration Control").
            get<double>("Fixed dt");
          double scaledTime(timeInSecs / t0);
          inputPList.sublist("Solution Control").sublist("Rythmos").
            sublist("Rythmos Integration Control").
            set<double>("Fixed dt", scaledTime);
          *out << "Fixed dt = " << timeInSecs << " [s], scaled value = "
               << scaledTime << endl;
        }
      } // end of Rythmos Integration Control block
    } // end of ImplicitBDFRamping
    else
      TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error, "Only "                 \
        "ImplicitBDFRamping step control strategy is currently supported!")
  }
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error, "Invalid Stepper Type, "  \
      "must be either Backward Euler or BDF!");

  // Get the model IDs.
  std::unordered_set<string> modelId;

  ParameterList bIDpID_list = inputPList.sublist("Block ID to Physics ID Mapping");
  for (auto liter=bIDpID_list.begin(); liter != bIDpID_list.end(); ++liter)
  {
    ParameterEntry const& entry = bIDpID_list.entry(liter);
    auto physicsID = Teuchos::getValue<std::string>(entry);

    // Iterate over the sublists inserting unique materials.
    ParameterList scPL =
      inputPList.sublist("Physics Blocks").sublist(physicsID);
    for (auto scPLIt = scPL.begin(); scPLIt != scPL.end(); ++scPLIt)
    {
      const string key(scPLIt->first);
      if (scPL.isSublist(key))
      {
        const ParameterList& submodels = scPL.sublist(key);
        string mid(submodels.get<string>("Model ID"));
        modelId.insert(mid);
      }
    }
  }


  bool foundOpt=false;
  for (auto modelIter=modelId.begin(); modelIter != modelId.end(); ++modelIter)
  {
    if (inputPList.sublist("Closure Models").isSublist(*modelIter))
    {
      if (inputPList.sublist("Closure Models").sublist(*modelIter).isSublist("Optical Generation"))
      {
	ParameterList& OptGenPList = inputPList.sublist("Closure Models").
          sublist(*modelIter).sublist("Optical Generation").sublist("Function1");
	if (OptGenPList.isParameter("Function Type"))
	  {
	    if (OptGenPList.get<string>("Function Type") == "Gauss")
	      {
		double peakTime(OptGenPList.get<double>("Time Peak Location"));
		double sigmaSpan;
		double FWHM=0.0;
		if (OptGenPList.isParameter ("Time Width"))
		  {
		    sigmaSpan = OptGenPList.get<double>("Time Width");
		    FWHM = sigmaSpan/4.71;
		  }
		if (OptGenPList.isParameter ("Time FWHM"))
		  FWHM = OptGenPList.get<double>("Time FWHM");
		double trueSigma = FWHM/2.355;
		std::vector<double> pulseTimes;
		//double startTime = peakTime - FWHM;
		//double endTime = peakTime + FWHM;
		double startTime = peakTime - 5*trueSigma;
		double endTime = peakTime + 5*trueSigma;
		int discretization = 25;  //Default Value
		if (OptGenPList.isParameter("Pulse Time Steps"))  //user defined number of steps in pulse
		  discretization = OptGenPList.get<int>("Pulse Time Steps");
		if (OptGenPList.isParameter("Pulse Maximum Time Step"))  //user defined number of steps in pulse
		  {
		    double pMTS = OptGenPList.get<double>("Pulse Maximum Time Step");
		    if (pMTS <= 0.0)
		      TEUCHOS_TEST_FOR_EXCEPTION((pMTS <= 0), runtime_error, "Pulse maximum time stepsize MUST be greater than 0.0");
		    discretization = int((endTime-startTime)/pMTS);
		  }
		pulseTimes.push_back(startTime);
		double times = startTime;
		if (times < 0) times = 0.0;
		double timeIncrement = (endTime-startTime)/double(discretization);
		while (times <= endTime)
		  {
		    times+=timeIncrement;
		    pulseTimes.push_back(times);
		  }
		foundOpt = true;
		stringstream timeStringStream;
		timeStringStream<<pulseTimes[0]/t0;
		for (size_t bt=1 ; bt<pulseTimes.size() ; bt++)
		  timeStringStream<<","<<pulseTimes[bt]/t0;
		string timeString = timeStringStream.str();
		rythmosPList.sublist("Rythmos Step Control Strategy").
		  set("Break Points", timeString);
	      }
	  }
      }
    }
    if (foundOpt) break;
  }

}

///////////////////////////////////////////////////////////////////////////////
//
//  isContactBC()
//
///////////////////////////////////////////////////////////////////////////////
bool
isContactBC(
  const Teuchos::ParameterList& bc,
  bool&                         isConstraint)
{
  // Get the boundary condition information from the ParameterList.
  std::string bctype   = bc.get<std::string>("Type"    );
  std::string strategy = bc.get<std::string>("Strategy");
  // If performing a harmonic balance simulation, use the "Time Domain Strategy" as the strategy
  if (strategy == "Frequency Domain")
    strategy = bc.sublist("Data").get<std::string>("Time Domain Strategy");

  // Determine whether the contact is a constraint.
  isConstraint = false;
  if ((bctype == "Dirichlet")                and
      ((strategy == "Constant Current")   or
       (strategy == "Resistor Contact")))
    isConstraint = true;

  // Determine whether the boundary condition is one of the contacts we
  // currently support.

  if ((bctype == "Dirichlet")                     and
      ((strategy == "Ohmic Contact"        )   or
       (strategy == "Dirichlet Schottky Contact"        )   or
       (strategy == "Linear Ramp"          )   or
       (strategy == "Sinusoid"             )   or
       (strategy == "Periodic"             )   or
       (strategy == "BJT1D Base Contact"   )   or
       (strategy == "Thermal Contact"      )   or
       (strategy == "Constant Current"     )   or
       (strategy == "Resistor Contact"     )   or 
       (strategy == "Contact On Insulator" )))
    return true;
  else
    return false;
} // end of isContactBC()

///////////////////////////////////////////////////////////////////////////////
//
//  determineContactSides()
//
///////////////////////////////////////////////////////////////////////////////
void
determineContactSides(
  const Teuchos::ParameterList&                     bcPL,
  charon::CurrentConstraintList&                    currentConstraints,
  std::vector<std::pair<std::string, std::string>>& contactSides, 
  std::vector<std::string>&                         contactStrategy)
{
  using std::make_pair;
  using std::map;
  using std::string;
  using Teuchos::getValue;
  using Teuchos::ParameterEntry;
  using Teuchos::ParameterList;

  // Create structs to bundle together the four pieces of information we'll
  // care about below.
  struct SideKey
  {
    string sidesetId_;
    string elementBlockId_;
    string strategy_;
  }; // end of struct SideKey
  struct SideValue
  {
    bool isConstraint_;
    int  responseIndex_;
  }; // end of struct SideValue

  // Create a comparator that sorts SideKeys first by sideset ID, and then by
  // element block ID.
  struct SideKeySort
  {
    bool
    operator()(
      const SideKey& lhs,
      const SideKey& rhs) const
    {
      return (( lhs.sidesetId_      <  rhs.sidesetId_      )  or
              ((lhs.sidesetId_      == rhs.sidesetId_     )   and
               (lhs.elementBlockId_ <  rhs.elementBlockId_)));
    } // end of operator()
  }; // end of struct SideKeySort

  // Create a comparator that sorts SideValues by the response index.
  struct SideValueSort
  {
    bool
    operator()(
      const SideValue& lhs,
      const SideValue& rhs) const
    {
      return (lhs.responseIndex_ < rhs.responseIndex_);
    } // end of operator()
  }; // end of struct SideValueSort

  // Loop over the entries in the ParameterList.
  map<SideKey, SideValue, SideKeySort> uniqueSides;
  int i(0);
  for (auto itr = bcPL.begin(); itr != bcPL.end(); ++itr)
  {
    // Get the entry and ensure it's also a ParameterList.
    const ParameterEntry& entry = bcPL.entry(itr);
    TEUCHOS_ASSERT(entry.isList());
    const ParameterList& bc = getValue<ParameterList>(entry);

    // Determine if a given boundary condition corresponds to a contact.
    bool isConstraint(false);
    if (isContactBC(bc, isConstraint))
    {
      // Create a SideKey corresponding this this boundary condition.
      SideKey key;
      key.sidesetId_      = bc.get<string>("Sideset ID");
      key.elementBlockId_ = bc.get<string>("Element Block ID");

      // Determine the time domain strategy
      string strategy = bc.get<string>("Strategy"); 
      if (strategy == "Frequency Domain")
        strategy = bc.sublist("Data").get<std::string>("Time Domain Strategy");
      key.strategy_ = strategy; 

      // If we've already seen this (sideset ID, element block ID) pair, check
      // to see if there's a constraint we haven't seen yet, and then move on
      // to the next boundary condition.
      if (uniqueSides.count(key) == 1)
      {
        if (isConstraint)
          uniqueSides[key].isConstraint_ = true;
        continue;
      }

      // If not, store this boundary condition's information in the uniqueSides
      // map.
      SideValue val;
      val.isConstraint_  = isConstraint;
      val.responseIndex_ = i++;
      uniqueSides[key] = val;
    } // end if a given boundary condition corresponds to a contact
  } // end loop over the entries in the ParameterList

  // Reorder uniqueSides such that it's sorted by the response index instead of
  // the (sideset ID, element block ID) pair.
  map<SideValue, SideKey, SideValueSort> uniqueSides2;
  for (const auto& side : uniqueSides)
    uniqueSides2[side.second] = side.first;

  // Transfer the unique side IDs from the set into a vector.
  i = 0;
  contactSides.clear();
  contactStrategy.clear(); 
  for (const auto& side : uniqueSides2)
  {
    contactSides.push_back(make_pair(side.second.sidesetId_,
      side.second.elementBlockId_));
    contactStrategy.push_back(side.second.strategy_); 
    
    if (side.first.isConstraint_)
      currentConstraints[i++]->responseIndex(side.first.responseIndex_);
  } // end loop over uniqueSides2
} // end of determineContactSides()

///////////////////////////////////////////////////////////////////////////////
//
//  checkEqnSetType()
//
///////////////////////////////////////////////////////////////////////////////
void
checkEqnSetType(
  const Teuchos::ParameterList& inputPList,
  bool&                         isEFFPG,
  bool&                         isCVFEM)
{
  using   std::runtime_error;
  using   std::string;
  using   std::vector;
  using   Teuchos::getValue;
  using   Teuchos::ParameterEntry;
  using   Teuchos::ParameterList;
  using   Teuchos::RCP;

  const ParameterList& pbParamList = inputPList.sublist("Physics Blocks");
  vector<bool> isTempConst;
  isEFFPG = false;
  isCVFEM = false;

  // Loop over the equation sets.
  for (auto eq = pbParamList.begin(); eq != pbParamList.end(); ++eq)
  {
    const ParameterEntry& entry = eq->second;
    const ParameterList& pList = getValue<ParameterList>(entry);

    // Allow only one equation set per physics block.
    if (pList.numParams() > 1)
      TEUCHOS_TEST_FOR_EXCEPTION(true, runtime_error, "The physics block "
        << pList.name() << " has more than one equation set!")

    const ParameterList& eqSetPList = pList.sublist("child0");
    string eqnSetType(eqSetPList.get<string>("Type"));

    // Check to see if the equation sets are control volume or EFFPG.  These
    // sets will require specialized Workset Factories.
    if ((eqnSetType == "SGCVFEM Laplace"          )  or
        (eqnSetType == "SGCVFEM NLPoisson"        )  or
        (eqnSetType == "SGCVFEM Drift Diffusion"  )  or
        (eqnSetType == "SGCharon1 Drift Diffusion"))
      isCVFEM = true;
    else if ((eqnSetType == "EFFPG Drift Diffusion") or
             (eqnSetType == "EFFPG DDIonLattice"   ))
      isEFFPG = true;

    // Check if the discretization is FEM-SUPG, FEM-EFFPG, or FEM-symEFFPG.
    // This is used for the DDLattice and DDIonLattice equation sets
    // since they support multiple discretizations. 
    if (eqSetPList.isSublist("Options"))
    {
      const ParameterList& optList = eqSetPList.sublist("Options");
      if (optList.isParameter("Discretization Method"))
      { 
        const string& discMethod = optList.get<string>("Discretization Method");
        if (discMethod.find("EFFPG") != string::npos)
          isEFFPG = true; 
      }
    } // end if (eqSetPList.isSublist("Options"))

    if(eqnSetType == "Frequency Domain")
    {
      std::string td_eqnset_type  = pList.sublist("child0").sublist("Options").get<std::string>("Time Domain Equation Set");
      if((td_eqnset_type == "SGCVFEM Laplace"          )  or
         (td_eqnset_type == "SGCVFEM NLPoisson"        )  or
         (td_eqnset_type == "SGCVFEM Drift Diffusion"  )  or
         (td_eqnset_type == "SGCharon1 Drift Diffusion"))
        isCVFEM = true;
    }

  } // end loop over the equation sets
} // end of checkEqnSetType()

///////////////////////////////////////////////////////////////////////////////
//
//  addCurrentResponses()
//
///////////////////////////////////////////////////////////////////////////////
void
addCurrentResponses(
  const std::vector<std::pair<std::string, std::string>>& contactSides,
  const std::vector<std::string>&                         contactStrategy,
  const bool&                                             currentHighOrder,
  const Teuchos::RCP<charon::Scaling_Parameters>&         scaleParams,
  const charon::CurrentConstraintList&                    currentConstraints,
  const bool&                                             writeCurrentGraph,
  panzer_stk::ModelEvaluatorFactory<double>&              meFactory,
  Teuchos::RCP<charon::RythmosObserverFactory>&           rof,
  Teuchos::RCP<charon::TempusObserverFactory>&            tof,
  Teuchos::RCP<charon::NOXObserverFactory>&               nof,
  panzer::ClosureModelFactory_TemplateManager<panzer::Traits>&
                                                          cmFactory,
  std::vector<std::string>&                               responseNames,
  const std::vector<std::string>&                         responseNames_fd_suffices,
  bool                                                    isFreqDom,
  bool                                                    isSingleFreq,
  bool                                                    isTransient,
  double                                                  frequency)
{
  using charon::CurrentResponse_Builder;
  using charon::HOCurrentResponse_Builder;
  using charon::DispCurrentResponse_Builder;
  using panzer::LinearObjFactory;
  using panzer::sidesetDescriptor;
  using panzer::sidesetVolumeDescriptor;
  using panzer::Traits;
  using panzer::GlobalIndexer;
  using panzer::GlobalIndexer;
  using panzer::WorksetDescriptor;
  using std::logic_error;
  using std::size_t;
  using std::string;
  using std::vector;
  using Teuchos::RCP;
  using Teuchos::rcp_dynamic_cast;

  // Clear out the output variables.
  responseNames.clear();

  // Get the linear object factory and global indexer.
  RCP<LinearObjFactory<Traits>> linearObjFactory = meFactory.getLinearObjFactory();
  RCP<GlobalIndexer>  globalIndexer = meFactory.getGlobalIndexer();

  // Add the current responses.
 for(unsigned int h = 0 ; h < responseNames_fd_suffices.size() ; h++)
 {
  for (size_t i(0); i < contactSides.size(); ++i)
  {
    string strategy = contactStrategy[i]; 
    bool isContOnIns(false);
    if (strategy == "Contact On Insulator") isContOnIns = true; 

    if (!isContOnIns) // exclude conduction current for contact on insulator
    {
      if (currentHighOrder)
      {
        HOCurrentResponse_Builder<int, int> builder;
        builder.comm             = MPI_COMM_WORLD;
        builder.fd_suffix        = responseNames_fd_suffices[h];
        builder.cubatureDegree   = 2;
        builder.scaleParams      = scaleParams;
        builder.linearObjFactory = linearObjFactory;
        builder.globalIndexer    = rcp_dynamic_cast<GlobalIndexer>(globalIndexer);
        builder.isFreqDom = isFreqDom;
      
        vector<WorksetDescriptor> sidesets;
        sidesets.push_back(sidesetVolumeDescriptor(contactSides[i].second,
          contactSides[i].first));
        string respName(contactSides[i].first + "_" + contactSides[i].second +
                      + "" + responseNames_fd_suffices[h] + "_Current");
        size_t index(meFactory.addResponse(respName, sidesets, builder));
      
        TEUCHOS_TEST_FOR_EXCEPTION(responseNames.size() != index,
          logic_error, "Error:  Added a response to the ModelEvaluatorFactory " \
          "in addCurrentResponses(), but the index returned (" << index << ") " \
          "is not what we'd expect.");
        responseNames.push_back(respName);
      }
      else // if (not currentHighOrder)
      {
        CurrentResponse_Builder<int, int> builder;
        builder.comm             = MPI_COMM_WORLD;
        // builder.enableHoles      = true;
        // builder.enableElectrons  = true;
        builder.fd_suffix = responseNames_fd_suffices[h];
        builder.cubatureDegree   = 2;
        builder.linearObjFactory = linearObjFactory;
        builder.globalIndexer    = rcp_dynamic_cast<GlobalIndexer>(globalIndexer);
        builder.isFreqDom = isFreqDom;
      
        vector<WorksetDescriptor> sidesets;
        sidesets.push_back(sidesetDescriptor(contactSides[i].second,
          contactSides[i].first));
        string respName(contactSides[i].first + "_" + contactSides[i].second +
          + "" + responseNames_fd_suffices[h] + "_Current");
        size_t index(meFactory.addResponse(respName, sidesets, builder));
      
        TEUCHOS_TEST_FOR_EXCEPTION(responseNames.size() != index,
          logic_error, "Error:  Added a response to the ModelEvaluatorFactory " \
          "in addCurrentResponses(), but the index returned (" << index << ") " \
          "is not what we'd expect.");
        responseNames.push_back(respName);
      } // end if (currentHighOrder) or not
    }  // end if (!isContOnIns)

    // compute the radian frequency 
    double pi = 4.0*std::atan(1.0);
    double ssOmega = 2.0*pi*frequency; 

    // compute contact displacement current for time and frequency domain simulation
    if (currentHighOrder)
    {
      if ((isFreqDom && h > 0) or isTransient)  // for HB simuation, exclude the DC harmonic
      {
        DispCurrentResponse_Builder<int, int> builder;
        builder.comm             = MPI_COMM_WORLD;
        builder.isTransient      = isTransient;   
        builder.isSingleFreq     = isSingleFreq;  
        builder.isFreqDom        = isFreqDom;
        builder.omega            = ssOmega;       
        builder.fd_suffix        = responseNames_fd_suffices[h];
        builder.cubatureDegree   = 2;
        builder.scaleParams      = scaleParams;
        builder.linearObjFactory = linearObjFactory;
        builder.globalIndexer    = rcp_dynamic_cast<GlobalIndexer>(globalIndexer);
      
        vector<WorksetDescriptor> sidesets;
        sidesets.push_back(sidesetDescriptor(contactSides[i].second,
          contactSides[i].first));
        string respName(contactSides[i].first + "_" + contactSides[i].second +
                      + "" + responseNames_fd_suffices[h] + "_DispCurrent");
        size_t index(meFactory.addResponse(respName, sidesets, builder));
      
        TEUCHOS_TEST_FOR_EXCEPTION(responseNames.size() != index,
          logic_error, "Error:  Added a response to the ModelEvaluatorFactory " \
          "in addCurrentResponses(), but the index returned (" << index << ") " \
          "is not what we'd expect.");
        responseNames.push_back(respName);
      }
    }  // end displacement current 

  } // end loop over contactSides

  // Add the responses to the Rythmos and NOX Observer Factories.
  if (not rof.is_null())
    rof->setResponseNames(responseNames);

  if (not tof.is_null())
    tof->setResponseNames(responseNames);

  nof->setResponseNames(responseNames);

  // Set up the Rythmos Observer Factory for pretty-printing of the voltage
  // control parameters.
  vector<string> paramNames;
  for (const auto& constraint : currentConstraints)
    paramNames.push_back(constraint->sidesetId() + constraint->type() +
      "Voltage");
  if (not paramNames.empty())
  {
    if (not rof.is_null())
      rof->setParameterNames(paramNames);
    if (not tof.is_null())
      tof->setParameterNames(paramNames);
  }

 } // end loop over responseNames_fd_suffices

  // Build the responses.
  meFactory.buildResponses(cmFactory, writeCurrentGraph, "charon_current_");

} // end of addCurrentResponses()


void checkHeterointerfacesCharge(Teuchos::ParameterList& bcPL) {
  using Teuchos::ParameterList;
  using Teuchos::ParameterEntry;
  using std::string;
  using std::vector;

  vector<string> hj_sidesetID;
  vector<string> hj_elBlockID;
  vector<string> hj_elBlockID2;
  vector<string> hj_eqSetName;
  vector<string> hj_eqSetName2;
  vector<string> hj_discMethod;
  vector<double> hj_fixedCharge;
  for (auto itr = bcPL.begin(); itr != bcPL.end(); ++itr) {
    const ParameterEntry& entry = bcPL.entry(itr);
    TEUCHOS_ASSERT(entry.isList());
    const ParameterList& bc = Teuchos::getValue<ParameterList>(entry);
    if(bc.get<string>("Strategy") == "Interface Heterojunction" &&
                                bc.get<string>("Type") == "Interface") {
      std::size_t foundpot1 = 
	bc.get<string>("Equation Set Name").find("ELECTRIC_POTENTIAL");
      std::size_t foundpot2 = 
	bc.get<string>("Equation Set Name2").find("ELECTRIC_POTENTIAL");
      if(foundpot1 == string::npos || foundpot2 == string::npos) break;

      const string sidesetID =  bc.get<string>("Sideset ID");
      const string elBlockID = bc.get<string>("Element Block ID");
      const string elBlockID2 = bc.get<string>("Element Block ID2");
      const string eqSetName = bc.get<string>("Equation Set Name");
      const string eqSetName2 = bc.get<string>("Equation Set Name2");
      const ParameterList& data = bc.sublist("Data");
      const string discMethod = data.get<string>("Discretization Method");
      const double fixedCharge = data.get<double>("Fixed Charge");
      // check if interface already exists
      vector<string>::iterator it = 
	std::find(hj_sidesetID.begin(),hj_sidesetID.end(),sidesetID);
      if (it != hj_sidesetID.end()) {
        // interface with sidesideID already exists, check the 
	// consistenty of the associated BCs
	int index = std::distance(hj_sidesetID.begin(),it);
        // check for Element Block ID
	TEUCHOS_TEST_FOR_EXCEPTION(elBlockID != hj_elBlockID2[index], std::invalid_argument,
	   "Error:  Wrong 'Element Block ID' for interface '" <<  sidesetID << "'!");
	// check for Equation Set Name
	if (hj_eqSetName2[index] == "ELECTRIC_POTENTIAL") {
	  TEUCHOS_TEST_FOR_EXCEPTION(eqSetName != "ELECTRIC_POTENTIAL", std::invalid_argument,
	       "Error:  Wrong 'Equation Set Name' for interface '" <<  sidesetID << "'!");
	} else {
	  TEUCHOS_TEST_FOR_EXCEPTION(eqSetName != hj_eqSetName2[index], std::invalid_argument,
	       "Error:  Wrong 'Equation Set Name' for interface '" <<  sidesetID << "'!");
	}
	if (hj_eqSetName[index] == "ELECTRIC_POTENTIAL") {
	  TEUCHOS_TEST_FOR_EXCEPTION(eqSetName2 != "ELECTRIC_POTENTIAL", std::invalid_argument,
	       "Error:  Wrong 'Equation Set Name2' for interface '" <<  sidesetID << "'!");
	} else {
	  TEUCHOS_TEST_FOR_EXCEPTION(eqSetName2 != hj_eqSetName[index], std::invalid_argument,
	       "Error:  Wrong 'Equation Set Name2' for interface '" <<  sidesetID << "'!");
	}
	// check for Discretization Method
	TEUCHOS_TEST_FOR_EXCEPTION(discMethod != hj_discMethod[index], std::invalid_argument,
	   "Error:  Wrong 'Discretization Method' for interface '" <<  sidesetID << "'!");
	// check for Fixed Charge
	TEUCHOS_TEST_FOR_EXCEPTION(fixedCharge != hj_fixedCharge[index], std::invalid_argument,
	   "Error:  'Fixed Charge' on both sides of the interface '" <<  sidesetID << "' must be equal!");
      } else {
        // not found, record it
	hj_sidesetID.push_back(sidesetID);
	hj_elBlockID.push_back(elBlockID);
        hj_elBlockID2.push_back(elBlockID2);
        hj_eqSetName.push_back(eqSetName);
        hj_eqSetName2.push_back(eqSetName2);
	hj_discMethod.push_back(discMethod);
	hj_fixedCharge.push_back(fixedCharge);
      }

     }
  }
}


void checkGateTunneling(Teuchos::ParameterList& bcPL) {
  using Teuchos::ParameterList;
  using Teuchos::ParameterEntry;
  using std::string;
  
  for (auto itr = bcPL.begin(); itr != bcPL.end(); ++itr) {
    const ParameterEntry& entry = bcPL.entry(itr);
    TEUCHOS_ASSERT(entry.isList());
    const ParameterList& bc = Teuchos::getValue<ParameterList>(entry);
    if(bc.get<string>("Strategy") == "Gate Tunneling") {
      const ParameterList& data = bc.sublist("Data");
      const string gate_sidesetID = data.get<string>("Gate Sideset ID");
      bool isValidGate = false;
      // check if there is a real gate contact for 'gate_sidesetID'
      for (auto itr1 = bcPL.begin(); itr1 != bcPL.end(); ++itr1) {
	const ParameterEntry& entry1 = bcPL.entry(itr1);
	TEUCHOS_ASSERT(entry1.isList());
	const ParameterList& bc1 = Teuchos::getValue<ParameterList>(entry1);
	if(bc1.get<string>("Strategy") == "Contact On Insulator") {
	  const string sidesetID1 = bc1.get<string>("Sideset ID");
	  if (sidesetID1 == gate_sidesetID) {
	    isValidGate = true;
	    break;
	  }
	}
      }
      TEUCHOS_TEST_FOR_EXCEPTION(!isValidGate, std::invalid_argument,
	 "Error: Gate Sideset ID '" <<  gate_sidesetID << "' not valid!"); 
    }
  }
}


void adjustSchottkyContacts_params(Teuchos::ParameterList& bcPL,
				   Teuchos::ParameterList& user_data) {
  using Teuchos::ParameterList;
  using Teuchos::ParameterEntry;
  using std::string;

  // for Schottky contacts copy the Workfunction and voltage
  // from Dirichlet input to Neumann input
  for (auto itr = bcPL.begin(); itr != bcPL.end(); ++itr) {
    const ParameterEntry& entry = bcPL.entry(itr);
    TEUCHOS_ASSERT(entry.isList());
    const ParameterList& bc = Teuchos::getValue<ParameterList>(entry);
    if(bc.get<string>("Strategy") == "Neumann Schottky Contact") {
      string sidesetID = bc.get<string>("Sideset ID");
      string elBlockID = bc.get<string>("Element Block ID");
      double WF = 0.0;
      bool withVoltage = false;
      bool withVaryingVoltage = false;
      double Vapp = 0.0;
      bool found_pair = false;
      for (auto itr1 = bcPL.begin(); itr1 != bcPL.end(); ++itr1) {
	const ParameterEntry& entry1 = bcPL.entry(itr1);
	TEUCHOS_ASSERT(entry1.isList());
	const ParameterList& bc1 = Teuchos::getValue<ParameterList>(entry1);
	if(bc1.get<string>("Strategy") == "Dirichlet Schottky Contact") {
	  string sidesetID1 = bc1.get<string>("Sideset ID");
	  string elBlockID1 = bc1.get<string>("Element Block ID");
	  if(sidesetID1 == sidesetID and elBlockID1 == elBlockID) {
	    found_pair = true;
	    const ParameterList& data = bc1.sublist("Data");
	    WF = data.get<double>("Work Function");
	    if (data.isParameter("Voltage")) {
	      withVoltage = true;
	      Vapp = data.get<double>("Voltage");
	    }
	    if (data.isParameter("Varying Voltage"))
	      withVaryingVoltage = true;
	    break;
	    } 
	}
      }
      TEUCHOS_TEST_FOR_EXCEPTION(!found_pair, std::invalid_argument,
	   "Error:  Schottky Contact '" <<  sidesetID << "' does not have a Dirichlet condition imposed!");
      TEUCHOS_TEST_FOR_EXCEPTION(withVoltage == withVaryingVoltage, std::invalid_argument,
	   "Error:  Schottky Contact '" <<  sidesetID << "' cannot have fixed and varying voltage simultaneously!");
      bcPL.sublist(itr->first).sublist("Data").set("Work Function",WF);
      if(withVoltage)
	bcPL.sublist(itr->first).sublist("Data").set("Voltage",Vapp);
      if(withVaryingVoltage)
	bcPL.sublist(itr->first).sublist("Data").set("Varying Voltage","Parameter");
    }
  }

  Teuchos::RCP<std::vector<string>> in1 = Teuchos::rcp(new std::vector<string>);
  Teuchos::RCP<std::vector<string>> in2 = Teuchos::rcp(new std::vector<string>);
  Teuchos::RCP<std::vector<double>> in3 = Teuchos::rcp(new std::vector<double>);
  Teuchos::RCP<std::vector<double>> in4 = Teuchos::rcp(new std::vector<double>);
  for (auto itr = bcPL.begin(); itr != bcPL.end(); ++itr) {
    const ParameterEntry& entry = bcPL.entry(itr);
    const ParameterList& bc = Teuchos::getValue<ParameterList>(entry);
    if(bc.get<string>("Strategy") == "Dirichlet Schottky Contact") {
       string sidesetID = bc.get<string>("Sideset ID");
       string elBlockID = bc.get<string>("Element Block ID");
       const ParameterList& data = bc.sublist("Data");
       double WF = data.get<double>("Work Function");
       double Vapp = 0.0;
       if( data.isParameter("Voltage")) 
	 Vapp = data.get<double>("Voltage");
       
       in1->push_back(sidesetID); in2->push_back(elBlockID);
       in3->push_back(WF); in4->push_back(Vapp);
    }
  }  
  user_data.set("SchottkyContacts",in1);
  user_data.set("SchottkyBlocks",in2);
  user_data.set("SchottkyWF",in3);
  user_data.set("SchottkyVapp",in4);
} // end of adjustSchottkyContacts_params


void gateContacts_params(Teuchos::ParameterList& bcPL,
			 Teuchos::ParameterList& user_data) {
  using Teuchos::ParameterList;
  using Teuchos::ParameterEntry;
  using std::string;

  Teuchos::RCP<std::vector<string>> in1 = Teuchos::rcp(new std::vector<string>);
  Teuchos::RCP<std::vector<string>> in2 = Teuchos::rcp(new std::vector<string>);
  Teuchos::RCP<std::vector<double>> in3 = Teuchos::rcp(new std::vector<double>);
  Teuchos::RCP<std::vector<double>> in4 = Teuchos::rcp(new std::vector<double>);

  for (auto itr = bcPL.begin(); itr != bcPL.end(); ++itr) {
    const ParameterEntry& entry = bcPL.entry(itr);
    TEUCHOS_ASSERT(entry.isList());
    const ParameterList& bc = Teuchos::getValue<ParameterList>(entry);
    if(bc.get<string>("Strategy") == "Contact On Insulator") {
      string sidesetID = bc.get<string>("Sideset ID");
      string elBlockID = bc.get<string>("Element Block ID");
      const ParameterList& data = bc.sublist("Data");
      double WF = data.get<double>("Work Function");
      double Vapp = 0.0;

      if (data.isParameter("Voltage")) {
	Vapp = data.get<double>("Voltage");
      }
      
      in1->push_back(sidesetID); in2->push_back(elBlockID);
      in3->push_back(WF); 
      in4->push_back(Vapp);
    }
  }

  user_data.set("GateContacts",in1);
  user_data.set("GateBlocks",in2);
  user_data.set("GateWF",in3);
  user_data.set("GateVapp",in4);

}

void checkTID_params(const Teuchos::ParameterList& bcPL,
		      const Teuchos::ParameterList& cmPL) {
  using Teuchos::ParameterList;
  using Teuchos::ParameterEntry;
  using std::string;
   
  auto& matProperty = charon::Material_Properties::getInstance();

  for (auto cm = cmPL.begin(); cm != cmPL.end(); ++cm) {
    if (cm->second.isList()) {
      const ParameterEntry& entry = cm->second;
      const ParameterList& pList = Teuchos::getValue<ParameterList>(entry);
 
      if (pList.isSublist("TID")) {
	string mat = pList.get<string>("Material Name");
	const string matType = matProperty.getMaterialType(mat);
	if (matProperty.getMaterialType(mat) == "Insulator") {
	  const ParameterList& TID_pList = pList.sublist("TID");
	  if (TID_pList.isSublist("Kimpton Model")) {
	    const ParameterList& Kimpton_pList = TID_pList.sublist("Kimpton Model");

	    if (Kimpton_pList.isParameter("Gate Contact")) {
	      const string gate_name = Kimpton_pList.get<string>("Gate Contact");
	      // search if specified gate if valid
	      bool valid = false;
	      for (auto itr = bcPL.begin(); itr != bcPL.end(); ++itr) {
		const ParameterEntry& entry = bcPL.entry(itr);
		TEUCHOS_ASSERT(entry.isList());
		const ParameterList& bc = Teuchos::getValue<ParameterList>(entry);
		if(bc.get<string>("Strategy") == "Contact On Insulator") {
		  string sidesetID = bc.get<string>("Sideset ID");
		  if (sidesetID == gate_name) {
		    const ParameterList& data = bc.sublist("Data");
		    if(data.isParameter("Varying Voltage")) {
		      if (data.get<string>("Varying Voltage") == "Parameter") 
			{ valid = true; break; }
		    }  
		  } 
		}
	      } 
	      TEUCHOS_TEST_FOR_EXCEPTION(!valid, std::invalid_argument,
		     "Error: TID Gate Contact '" <<  gate_name << "' not valid!"); 
	    } // gate contact check
	  } // Kimpton Model
	} // insulator
      } // TID model
    }

  } // loop over the closure models 
}


void blockMaterial_params(const Teuchos::ParameterList& blphPL,
			  const Teuchos::ParameterList& phPL,
			  const Teuchos::ParameterList& cmPL,
			  Teuchos::ParameterList& user_data) {
  using Teuchos::ParameterEntry;
  using Teuchos::ParameterList;
  using charon::Material_Properties;
  using std::string;

  // blockId to material map
  Teuchos::RCP<std::map<string,string>> block_mat = 
    Teuchos::rcp(new std::map<string,string>);

  for (auto blph = blphPL.begin(); blph != blphPL.end(); ++blph) {
    string blockId = blph->first;
    ParameterEntry const& entry = blphPL.entry(blph);
    string physicsId = Teuchos::getValue<std::string>(entry);
    // find model id associated with physics block
    string modelId;
    for (auto ph = phPL.begin(); ph != phPL.end(); ++ph) {
      if(ph->first == physicsId) {
	const ParameterEntry& entry1 = ph->second;
	const ParameterList& pList = Teuchos::getValue<ParameterList>(entry1);
	const ParameterList& eqSetPList = pList.sublist("child0"); 
	modelId = eqSetPList.get<string>("Model ID");
	break;
      }
    }
    // find material name
    string mat;
    for (auto cm = cmPL.begin(); cm != cmPL.end(); ++cm) {
      if (cm->second.isList() && (cm->first == modelId) ) {
	const ParameterEntry& entry2 = cm->second;
	const ParameterList& pList1 = Teuchos::getValue<ParameterList>(entry2);
	mat = pList1.get<string>("Material Name");
	// validate material
	Material_Properties::getInstance().validateMaterialName(mat);
	block_mat->insert(std::pair<string,string>(blockId,mat));
	break;
      }
    }
  }
  
  user_data.set("block2mat",block_mat);
}


// end of Charon_Main.cpp
