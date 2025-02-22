
#ifndef CHARON_BCSTRATEGY_DIRICHLET_SINUSOID_DECL_HPP
#define CHARON_BCSTRATEGY_DIRICHLET_SINUSOID_DECL_HPP

#include <vector>
#include <string>

#include "Teuchos_RCP.hpp"
#include "Panzer_PureBasis.hpp"
#include "Panzer_BCStrategy_Dirichlet_DefaultImpl.hpp"
#include "Panzer_Traits.hpp"
#include "Phalanx_FieldManager.hpp"

namespace charon {

  template <typename EvalT>
  class BCStrategy_Dirichlet_Sinusoid : public panzer::BCStrategy_Dirichlet_DefaultImpl<EvalT> {

  public:

    BCStrategy_Dirichlet_Sinusoid(const panzer::BC& bc, const Teuchos::RCP<panzer::GlobalData>& global_data);

    void setup(const panzer::PhysicsBlock& side_pb,
               const Teuchos::ParameterList& user_data);

    void buildAndRegisterEvaluators(PHX::FieldManager<panzer::Traits>& fm,
                                    const panzer::PhysicsBlock& pb,
                                    const panzer::ClosureModelFactory_TemplateManager<panzer::Traits>& factory,
                                    const Teuchos::ParameterList& models,
                                    const Teuchos::ParameterList& user_data) const;

    std::string residual_name;
    Teuchos::RCP<panzer::PureBasis> basis;

    bool isLatTDof;
    bool isIonDof;
    bool isFermiPin;

    double ionDens; 

  };

}

#endif
