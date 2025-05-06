// Copyright 2024 Tencent Inc. All rights reserved.
//
// Author: kekesong@tencent.com
//
#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>
#include "data_queue.h"
#include "json/json.h"
#include "osi_datarecording.pb.h"
#include "sensor_raw.pb.h"
#include "thread_pool.h"
#include "txsim_module.h"

class sim_label final : public tx_sim::SimModule {
 public:
  sim_label();
  virtual ~sim_label();

  void Init(tx_sim::InitHelper &helper) override;
  void Reset(tx_sim::ResetHelper &helper) override;
  void Step(tx_sim::StepHelper &helper) override;
  void Stop(tx_sim::StopHelper &helper) override;

 protected:
  // struct ClipInfo{
  //   std::string record_time;
  //   std::string pack_time;
  //   std::string plate;
  //   std::string start;
  //   std::string end_timestamp;
  //   std::string status;
  //   std::string tags;
  //   int clip_id;
  //   std::string mapping_type;
  // };

  std::vector<std::string> GetSensorFolders(const sim_msg::SensorRaw &sensors);

  std::string GetSensorFolder(const sim_msg::SensorRaw::Sensor &sensor);

  std::string GetCameraName(int id);

  std::string GetFisheyeName(int id);

  bool LoadCalibration();

  void WriteClipAttributes(sim_msg::SensorMeta sensor_meta);

  void WriteClipPose(sim_msg::SensorMeta sensor_meta);

  void WriteSiteAttributes();

  std::string getFormattedTimestamp();

  void generateSensorCalibration();

  Eigen::Affine3d PoseToAffine(double roll, double pitch, double yaw, double x, double y, double z);

  void AffineToPose(const Eigen::Affine3d &affine, double &x, double &y, double &z, double &roll, double &pitch, double &yaw);

 private:
  std::string savePathBase = "/home/sim/data/display_pic_dir";
  std::string savePath;
  std::string site_name;

  std::string CalibrationPath;
  Json::Value Calibration;

  sim_msg::SensorMeta cur_sensor_meta;

  std::string device;

  double minArea = 5;
  double completeness = 0.2;
  double maxDistance = 200;
  int disNum = 30;
  bool saveScenarioDir = true;
  bool debugFiles = false;
  bool fullBox = false;
  std::shared_ptr<ThreadPool> threads;
  std::shared_ptr<DataQueue> queues;

  std::set<std::string> saved_files;
  std::mutex save_mutex;
  std::string config_dir;
  int ego_id = -1;

  std::vector<sim_msg::ClipInfo> clip_infos;
  std::map<int, std::map<double, sim_msg::UnrealPose>> poses;
  std::map<std::string, Eigen::Affine3d> camera_to_vcs_map;

  // start generating image from this time
  // 100ms
  int begin_frames_to_skip = 1;
  int cur_clip_id = -1;

  bool parseImage(const sim_msg::SensorRaw::Sensor &sensor, ImageInfo &info);
  bool parseLidar(const std::string &buf, PcInfo &info);
  bool saveFile(const std::string &fname, const std::string &buf);
  bool saveOpenCVImage(const std::string &fname, const std::string &buf, int width, int height,
                       sim_msg::SensorRaw_Type type);
  void saveImageLabel(const ImagePackage &info);
  void saveDepthImageLabel(const ImagePackage &info);
  void savePcdLabel(const PcdPackage &info);
  std::string getUTC();
  bool getSensorConfig(const std::string &buffer, const std::string &groupname);
  bool saveMaskJson(const std::string &fpath);
};
