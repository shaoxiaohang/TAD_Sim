// Copyright 2024 Tencent Inc. All rights reserved.
//

#pragma once
#include <vector>
#include "eigen3/Eigen/Core"
#include "eigen3/Eigen/Geometry"
#include "json/json.h"
#include "tx_simulation_loop.h"

TX_NAMESPACE_OPEN(TrafficFlow)

class TAD_FollowTrajectory : public Base::txSimulationLoop {
 public:
  TAD_FollowTrajectory() TX_DEFAULT;
  virtual ~TAD_FollowTrajectory() TX_DEFAULT;

  virtual void Init(tx_sim::InitHelper& helper) TX_OVERRIDE;
  virtual void Reset(tx_sim::ResetHelper& helper) TX_OVERRIDE;
  virtual void Step(tx_sim::StepHelper& helper) TX_OVERRIDE;
  virtual void Stop(tx_sim::StopHelper& helper) TX_OVERRIDE;

 protected:
  // Getting Timestamp in "YYYYMMDD-HHMMSS_MS
  std::string getFormattedTimestamp();

  struct Frame {
    int id;
    Eigen::Vector3d position;
    Eigen::Vector3d rpy;
    Eigen::Vector3d ue_position;
    Eigen::Quaterniond quat;
  };

  struct Clip {
    int id;
    int num_frames;
    std::vector<Frame> frames;
  };

  struct Site {
    std::string name;
    std::string tags;
    int num_clips;
    std::vector<Clip> clips;
  };

  Frame ConvertFrame(int index, const Json::Value& frame);

  void PubCGLocation(tx_sim::StepHelper& helper, const Frame& frame);

  void PubHighlightEgo(tx_sim::StepHelper& helper);

  void PubSensorMeta(tx_sim::StepHelper& helper, const Frame& frame);

  Frame FRU2FLU( const Frame& frame);

 protected:
  Site site_;
  std::string location_topic_ = "LOCATION";
  std::string highlight_ego_topic_ = ".hightlight_group";
  std::string sensor_meta_topic_ = "SENSOR_META";
  int cur_frame_ = 0;
  int cur_clip_ = 0;
  int frames_so_far_ = 0;
  std::string plate_ = "DZ115";
  int first_frames_to_skip_ = 1;
  int clip_frame_offset_ms_ = 0;
};
TX_NAMESPACE_CLOSE(TrafficFlow)
