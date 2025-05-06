#include "tad_sumo_system.h"

TX_NAMESPACE_OPEN(TrafficFlow)

TAD_SumoSystem::TAD_SumoSystem() {}

TAD_SumoSystem::~TAD_SumoSystem();

Base::txBool TAD_SumoSystem::Initialize(Base::ISceneLoaderPtr) TX_NOEXCEPT {}

Base::txBool TAD_SumoSystem::Update(const Base::TimeParamManager& timeMgr) TX_NOEXCEPT {}

Base::txBool TAD_SumoSystem::Release() TX_NOEXCEPT { return ParentClass::Release(); }

Base::txBool TAD_SumoSystem::IsSupportSceneType(const Base::ISceneLoader::ESceneType _sceneType) const
    TX_NOEXCEPT {
  return true;
}

#if USE_EgoGroup
Base::txBool TAD_SumoSystem::UpdatePlanningCarHighlight(Base::TimeParamManager const& timeMgr,
                                                                  const Base::txString& highlightStr) TX_NOEXCEPT {}
#endif

Base::txBool TAD_SumoSystem::RegisterPlanningCar() TX_NOEXCEPT {}

void TAD_SumoSystem::CreateAssemblerCtx() TX_NOEXCEPT {}

void TAD_SumoSystem::CreateElemMgr() TX_NOEXCEPT {}

TX_NAMESPACE_CLOSE(TrafficFlow)
