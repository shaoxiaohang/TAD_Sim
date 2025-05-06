#pragma once
#include "tx_scene_sketch.h"
#include "tx_tadsim_flags.h"
#include "tx_traffic_element_system.h"
TX_NAMESPACE_OPEN(TrafficFlow)

class TAD_SumoSystem : public Base::TrafficElementSystem {
  using ParentClass = Base::TrafficElementSystem;

 public:
  TAD_SumoSystem();

  virtual ~TAD_SumoSystem();

  /**
   * @brief 系统初始化
   *
   * @return Base::txBool
   */
  virtual Base::txBool Initialize(Base::ISceneLoaderPtr) TX_NOEXCEPT TX_OVERRIDE;

  /**
   * @brief 系统更新
   *
   * @param timeMgr 事件管理器
   * @return Base::txBool 更新成功返回true
   */
  virtual Base::txBool Update(const Base::TimeParamManager& timeMgr) TX_NOEXCEPT TX_OVERRIDE;

  /**
   * @brief 资源释放
   *
   * @return Base::txBool 释放成功返回true
   */
  virtual Base::txBool Release() TX_NOEXCEPT TX_OVERRIDE ;

  /**
   * @brief 是否支持所指定的场景类型
   *
   * @param _sceneType 所检查的场景类型
   * @return Base::txBool
   */
  virtual Base::txBool IsSupportSceneType(const Base::ISceneLoader::ESceneType _sceneType) const TX_NOEXCEPT
      TX_OVERRIDE;

 public:
#if USE_EgoGroup
  virtual Base::txBool UpdatePlanningCarHighlight(Base::TimeParamManager const& timeMgr,
                                                  const Base::txString& highlightStr) TX_NOEXCEPT TX_OVERRIDE;
#endif

  /**
   * @brief 注册规划车辆
   *
   * 此函数用于注册一辆规划车辆。
   *
   * @return Base::txBool 函数执行成功返回 true，否则返回 false
   */
  virtual Base::txBool RegisterPlanningCar() TX_NOEXCEPT TX_OVERRIDE;

 protected:
  /**
   * @brief 初始化Assemble对象
   *
   */
  virtual void CreateAssemblerCtx() TX_NOEXCEPT TX_OVERRIDE;

  /**
   * @brief 初始化element manager对象
   *
   */
  virtual void CreateElemMgr() TX_NOEXCEPT TX_OVERRIDE;

protected:


};