/**
 * @file label.cpp
 * @author kekesong (kekesong@tencent.com)
 * @brief
 * @version 0.1
 * @date 2024-03-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "label.h"
#include <stdio.h>
#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <opencv2/opencv.hpp>
#include <regex>
#include "camera_sensor.h"
#include "catalog.h"
#include "common/coord_trans.h"
#include "fisheye_sensor.h"
#include "google/protobuf/util/json_util.h"
#include "image_label.h"
#include "lidar_sensor.h"
#include "osi_datarecording.pb.h"
#include "scene.pb.h"
#include "sensor_meta.pb.h"
#include "sensor_raw.pb.h"
#include "visable_calculate.h"
/**
 * @brief define FS_TRY
 * @param f : the function to try
 */
#define FS_TRY(f)                       \
  try {                                 \
    f;                                  \
  } catch (const std::exception &e) {   \
    std::cout << e.what() << std::endl; \
  }

/**
 * @brief Construct a new sim label::sim label object
 *
 */
sim_label::sim_label() {}

/**
 * @brief Destroy the sim label::sim label object
 *
 */
sim_label::~sim_label() {}

/**
 * @brief init the label
 *
 * @param helper : the helper of tx_sim
 */
void sim_label::Init(tx_sim::InitHelper &helper) {
  // subscribe sensor truth
  helper.Subscribe("TXSIM_SENSOR_OBJECT");
  helper.Subscribe("SENSOR_META");
  // choose the device ids to recoard. default all devices are selected.
  device = helper.GetParameter("-device");
  if (device == "all") {
    device.clear();
  }

  // set the num of displays
  // default is 30
  // we will subcribe the topic "DISPLAYSENSOR_{id}"
  auto NumOfDisplay = helper.GetParameter("NumOfDisplay");
  if (!NumOfDisplay.empty()) {
    disNum = std::atoi(NumOfDisplay.c_str());
  }

  disNum = 1;

  std::cout << "display num " << disNum << std::endl;

  // set the path of the data saved.
  if (!helper.GetParameter("DataSavePath").empty()) savePathBase = helper.GetParameter("DataSavePath");
  if (!helper.GetParameter("CalibrationPath").empty()) CalibrationPath = helper.GetParameter("CalibrationPath");
  if (!helper.GetParameter("FramesToSkip").empty())
    begin_frames_to_skip = std::atoi(helper.GetParameter("FramesToSkip").c_str());

  if (!boost::filesystem::exists(CalibrationPath)) {
    std::cout << "CalibrationPath " << CalibrationPath << " does not exist" << std::endl;
    return;
  }
  std::cout << "CalibrationPath " << CalibrationPath << " exists" << std::endl;
  LoadCalibration();

  // subcribe display topic from 0 to NumOfDisplay
  for (int i = 0; i < disNum; i++) {
    helper.Subscribe(std::string("DISPLAYSENSOR_") + std::to_string(i));
    helper.Subscribe(std::string("DISPLAYPOSE_") + std::to_string(i));
  }
  // need create scenario dir or not?
  auto tmp = helper.GetParameter("CreateScenarioDir");
  if (tmp == "0" || tmp == "false" || tmp == "disable") saveScenarioDir = false;

  tmp = helper.GetParameter("FullBox");
  if (tmp == "1" || tmp == "true" || tmp == "enable") fullBox = true;

  tmp = helper.GetParameter("DebugFiles");
  if (tmp == "1" || tmp == "true" || tmp == "enable") debugFiles = true;
  debugFiles = true;
  config_dir = helper.GetParameter(tx_sim::constant::kInitKeyModuleSharedLibDirectory);
  // write path to logs
  std::cout << "savePathBase=" << savePathBase << std::endl;
}

/**
 * @brief reset the label
 *
 * @param helper the helper of tx_sim
 */
void sim_label::Reset(tx_sim::ResetHelper &helper) {
  // Load the sensor configuration
  getSensorConfig(helper.scene_pb(), helper.group_name());
  // Output the number of cameras and lidars loaded
  std::cout << "Load " << semantics.size() << " semantic." << std::endl;
  std::cout << "Load " << cameras.size() << " cameras." << std::endl;
  std::cout << "Load " << lidars.size() << " lidars." << std::endl;
  std::cout << "Load " << fisheyes.size() << " fisheyes." << std::endl;

  // Initialize the save paths
  savePath = savePathBase;
  if (savePath.back() == '/' || savePath.back() == '\\') {
    savePath.pop_back();
  }
  // If the saveScenarioDir flag is set, create a new subdirectory for the
  // scenario
  // if (saveScenarioDir) {
  //   auto scenario = boost::filesystem::path(helper.scenario_file_path()).stem().string();
  //   savePath += "/";
  //   savePath += scenario;
  // }
  // if (boost::filesystem::exists(savePath)) {
  //   FS_TRY(boost::filesystem::rename(savePath,
  //                                    savePath + "_" +
  //                                    std::to_string(rand())));
  // }
  // create ego dir
  // savePath += "/";
  // savePath += helper.group_name();
  // // Create the directories for storing data in the savePath
  // FS_TRY(boost::filesystem::create_directories(savePath + "/lidar/pcd"));
  // FS_TRY(boost::filesystem::create_directories(savePath + "/camera/jpg"));
  // FS_TRY(boost::filesystem::create_directories(savePath + "/semantic/png"));
  // FS_TRY(boost::filesystem::create_directories(savePath + "/fisheye/jpg"));
  // FS_TRY(boost::filesystem::create_directories(savePath + "/depth/png"));
  // FS_TRY(boost::filesystem::create_directories(savePath + "/normal/png"));

  // Output the savePath
  std::cout << "savePath=" << savePath << std::endl;

  // Initialize the thread pool and data queue
  threads = std::make_shared<ThreadPool>(8);
  queues = std::make_shared<DataQueue>();
  queues->setImageCallback(std::bind(&sim_label::saveImageLabel, this, std::placeholders::_1));
  queues->setDepthImageCallback(std::bind(&sim_label::saveDepthImageLabel, this, std::placeholders::_1));

  queues->setPcdCallback(std::bind(&sim_label::savePcdLabel, this, std::placeholders::_1));

  // if (!semantics.empty()) {
  //   saveMaskJson(savePath + "/semantic/mask.json");
  // }

  ego_id = std::atoi(helper.group_name().substr(helper.group_name().length() - 3).c_str());
  sim_msg::Scene scene;
  scene.ParseFromString(helper.scene_pb());
  // std::cout << scene.DebugString();
  Catalog::getInstance().init(scene);
  Catalog::getInstance().load_contour(config_dir);

  cur_clip_id = -1;

  clip_infos.clear();
  site_name = "";
  poses.clear();
}

/**
 * @brief timestamp to string
 *
 * @param timestamp The timestamp to be converted
 */
template <class T>
std::string timeStarmString(T timestamp) {
  // Create a string stream object to store the result
  std::stringstream ss;
  ss << std::setw(10) << std::setfill('0') << (std::int64_t)(timestamp);
  return ss.str();
}

/**
 * @brief Step method implementation for SimLabel class
 *
 * @param helper helper of txsim
 */
void sim_label::Step(tx_sim::StepHelper &helper) {
  // Initialize a map to keep track of timestamps for displaying objects
  static std::map<int, double> display_timstamp;
  if (helper.timestamp() == 0) {
    display_timstamp.clear();
  }

  // Check if truth mode is enabled
  // Get subscribed messages from various sources
  // Process Display Pose messages
  sim_msg::DisplayPose trafficPose_all;
  std::set<std::int64_t> egoid, carid, staid, dynid;
  // Iterate over DISPLAYPOSE_* channels
  for (int i = 0; i < disNum; i++) {
    std::string payload_;
    helper.GetSubscribedMessage(std::string("DISPLAYPOSE_") + std::to_string(i), payload_);

    // Deserialize incoming message
    sim_msg::DisplayPose trafficPose;
    if (payload_.empty() || !trafficPose.ParseFromString(payload_)) {
    }

    // std::cout << " DISPLAYPOSE " << trafficPose.DebugString() << std::endl;

    // Print current simulation timestamp followed by a colon
    //std::cout << "step timestamp " << helper.timestamp() << ": " << std::endl;

    // Update the timestamp field
    trafficPose_all.set_timestamp(trafficPose.timestamp());
    // Extract unique EGO IDs,
    for (const auto &obj : trafficPose.egos()) {
      //std::cout << " OBJ ID " << obj.id() << std::endl;
      //std::cout << " EGO ID " << ego_id << std::endl;
      if (obj.id() == ego_id) {
        continue;
      }
      if (egoid.find(obj.id()) == egoid.end()) {
        *trafficPose_all.add_egos() = obj;
        egoid.insert(obj.id());
      }
    }
    // Extract unique CAR IDs,
    for (const auto &obj : trafficPose.cars()) {
      // if (carid.find(obj.id()) == carid.end()) {
      *trafficPose_all.add_cars() = obj;
      // carid.insert(obj.id());
      //}
    }
    // Extract unique STATIONARY OBJECT IDs
    for (const auto &obj : trafficPose.staticobstacles()) {
      if (staid.find(obj.id()) == staid.end()) {
        *trafficPose_all.add_staticobstacles() = obj;
        staid.insert(obj.id());
      }
    }
    // Extract unique DYNAMIC OBJECT IDs
    for (const auto &obj : trafficPose.dynamicobstacles()) {
      if (dynid.find(obj.id()) == dynid.end()) {
        *trafficPose_all.add_dynamicobstacles() = obj;
        dynid.insert(obj.id());
      }
    }
  }
  // Send processed Display Pose message back out via queue
  std::cout << "[ traffic pose timestamp " << trafficPose_all.timestamp() << ": " << trafficPose_all.egos_size()
            << " ego | " << trafficPose_all.cars_size() << " car | " << trafficPose_all.staticobstacles_size()
            << " static | " << trafficPose_all.dynamicobstacles_size() << " dynamic] " << std::endl;
  queues->addObject(trafficPose_all);

  // Handle updates to displayed objects' ids
  std::map<int, std::set<std::int64_t>> display_ids;
  for (int i = 0; i < disNum; i++) {
    std::string payload_;
    helper.GetSubscribedMessage(std::string("DISPLAYSENSOR_") + std::to_string(i), payload_);
    sim_msg::SensorRaw sensorraw;
    if (payload_.empty() || !sensorraw.ParseFromString(payload_)) continue;

    //std::cout << "sensor timestamp " << sensorraw.timestamp() << std::endl;

    // time is not now
    // if (sensorraw.timestamp() == display_timstamp[i]) {
    //   std::cout << "sensor timestamp is not now " << sensorraw.timestamp() << " " << display_timstamp[i] <<
    //   std::endl; continue;
    // }
    display_timstamp[i] = sensorraw.timestamp();

    //std::cout << "sensor size " << sensorraw.sensor().size() << std::endl;

    if (sensorraw.timestamp() < begin_frames_to_skip * 100) {
      std::cout << " skip first " << begin_frames_to_skip << " frames" << std::endl;
      return;
    }

    if (sensorraw.sensor().size() > 0) {
      std::string payload;
      helper.GetSubscribedMessage("SENSOR_META", payload);
      sim_msg::SensorMeta sensor_meta;
      if (payload.empty() || !sensor_meta.ParseFromString(payload)) {
        std::cout << "sensor meta error" << std::endl;
        return;
      }

      poses[sensor_meta.clip().id()][sensor_meta.timestamp()] = sensor_meta.pose();

      site_name = sensor_meta.site().name();
      if (sensor_meta.cur_clip() != cur_clip_id || cur_clip_id == -1) {
        if (cur_clip_id == -1) {
          cur_sensor_meta = sensor_meta;
        } else {
          std::cout << " clip " << cur_clip_id << " end" << std::endl;
          WriteClipAttributes(cur_sensor_meta);
          WriteClipPose(cur_sensor_meta);
          cur_sensor_meta = sensor_meta;
        }

        std::cout << " clip " << sensor_meta.clip().id() << " start" << std::endl;
        std::string clip_record_time = sensor_meta.clip().record_time();
        savePath = savePathBase + "/" + site_name + "/" + clip_record_time;
        std::vector<std::string> folders = GetSensorFolders(sensorraw);
        std::cout << "creatte folders for clip id " << sensor_meta.clip().id() << std::endl;
        for (const auto &folder : folders) {
          std::string folder_path = savePath + "/" + folder;
          std::cout << "create folder " << folder_path << std::endl;
          FS_TRY(boost::filesystem::create_directories(folder_path));
        }
        cur_clip_id = sensor_meta.clip().id();
      } else {
        if (sensor_meta.cur_clip() == sensor_meta.site().num_clips() - 1 &&
            (sensor_meta.cur_frame() == sensor_meta.clip().num_frames() ||
             sensor_meta.cur_frame() == sensor_meta.clip().num_frames() - 1)) {
          std::cout << " last frame " << sensor_meta.cur_frame() << " " << sensor_meta.clip().num_frames() << std::endl;
          std::cout << " last clip " << sensor_meta.cur_clip() << " " << sensor_meta.site().num_clips() << std::endl;
          WriteClipAttributes(cur_sensor_meta);
          WriteClipPose(cur_sensor_meta);
          WriteSiteAttributes();
        }
      }
      //std::cout << "sensor meta " << sensor_meta.DebugString() << std::endl;
      queues->addSensorMeta(sensor_meta);
    }

    // Output channel index and timestamp
    // std::cout << "display id " << i << ", time stamp " << sensorraw.timestamp() << std::endl;
    // Process raw sensor data
    for (const auto &sensor : sensorraw.sensor()) {
      //std::cout << "sensor id " << sensor.id() << std::endl;
      //std::cout << "sensor type " << sensor.type() << std::endl;
      //std::cout << "sensor timestamp " << sensorraw.timestamp() << std::endl;
      // Add parsed image information to queue
      if (sensor.type() == sim_msg::SensorRaw::TYPE_CAMERA) {
        ImageInfo info;
        if (parseImage(sensor, info)) {
          //std::cout << "camera(" << info.id << "," << info.timestamp << ")=" << info.size << std::endl;
          queues->addCamera(info);
        }
      } else if (sensor.type() == sim_msg::SensorRaw::TYPE_DEPTH) {
        ImageInfo info;
        if (parseImage(sensor, info)) {
          //std::cout << "depth(" << info.id << "," << info.timestamp << ")=" << info.size << std::endl;
          queues->addDepth(info);
        }
      } else if (sensor.type() == sim_msg::SensorRaw::TYPE_SEMANTIC) {
        ImageInfo info;
        if (parseImage(sensor, info)) {
          //std::cout << "semantic(" << info.id << "," << info.timestamp << ")=" << info.size << std::endl;
          queues->addSenmantic(info);
        }
      } else if (sensor.type() == sim_msg::SensorRaw::TYPE_ULTRASONIC) {
        ImageInfo info;
        if (parseImage(sensor, info)) {
          //std::cout << "normal(" << info.id << "," << info.timestamp << ")=" << info.size << std::endl;
          queues->addNormal(info);
        }
      } else if (sensor.type() == sim_msg::SensorRaw::TYPE_FISHEYE) {
        ImageInfo info;
        if (parseImage(sensor, info)) {
          //std::cout << "fisheye(" << info.id << "," << info.timestamp << ")=" << info.size << std::endl;
          queues->addFisheye(info);
        }
      } else if (sensor.type() == sim_msg::SensorRaw::TYPE_FISHEYE_DEPTH) {
        ImageInfo info;
        if (parseImage(sensor, info)) {
          //std::cout << "fisheye_depth(" << info.id << "," << info.timestamp << ")=" << info.size << std::endl;
          queues->addFisheyeDepth(info);
        }
      } else if (sensor.type() == sim_msg::SensorRaw::TYPE_FISHEYE_NORMAL) {
        ImageInfo info;
        if (parseImage(sensor, info)) {
          //std::cout << "fisheye_normal(" << info.id << "," << info.timestamp << ")=" << info.size << std::endl;
          queues->addFisheyeNormal(info);
        }
      } else if (sensor.type() == sim_msg::SensorRaw::TYPE_FISHEYE_SEMANTIC) {
        ImageInfo info;
        if (parseImage(sensor, info)) {
          //std::cout << "fisheye_semantic(" << info.id << "," << info.timestamp << ")=" << info.size << std::endl;
          queues->addFisheyeSemantic(info);
        }
      } else if (sensor.type() == sim_msg::SensorRaw::TYPE_LIDAR) {
        PcInfo info;
        if (parseLidar(sensor.raw(), info)) {
          //std::cout << "ADD LIDAR " << std::endl;
          //std::cout << "lidar(" << info.id << "," << info.timestamp << ")=" << info.size << std::endl;
          queues->addLidar(info);
        }
      }
    }
    // Close square brackets and comma after processing sensors
    queues->update();
    // Clear the output buffer and print newline character
    std::cout << "\n";
  }
}

/**
 * @brief Get the Sensor Folders
 *
 * @param sensors
 * @return std::vector<std::string>
 */

std::vector<std::string> sim_label::GetSensorFolders(const sim_msg::SensorRaw &sensors) {
  std::vector<std::string> folders;
  for (const auto &sensor : sensors.sensor()) {
    std::string folder = GetSensorFolder(sensor);
    folders.push_back(folder);
  }
  return folders;
}

std::string sim_label::GetSensorFolder(const sim_msg::SensorRaw::Sensor &sensor) {
  if (sensor.type() == sim_msg::SensorRaw::TYPE_CAMERA) {
    return GetCameraName(sensor.id());
  }
  if (sensor.type() == sim_msg::SensorRaw::TYPE_FISHEYE) {
    return GetFisheyeName(sensor.id());
  }
  if (sensor.type() == sim_msg::SensorRaw::TYPE_FISHEYE_DEPTH) {
    return "depth_" + GetFisheyeName(sensor.id());
  }
  if (sensor.type() == sim_msg::SensorRaw::TYPE_FISHEYE_NORMAL) {
    return "normal_" + GetFisheyeName(sensor.id());
  }
  if (sensor.type() == sim_msg::SensorRaw::TYPE_FISHEYE_SEMANTIC) {
    return "semantic_" + GetFisheyeName(sensor.id());
  }
  if (sensor.type() == sim_msg::SensorRaw::TYPE_DEPTH) {
    return "depth_" + GetCameraName(sensor.id());
  }
  if (sensor.type() == sim_msg::SensorRaw::TYPE_ULTRASONIC) {
    return "normal_" + GetCameraName(sensor.id());
  }
  if (sensor.type() == sim_msg::SensorRaw::TYPE_SEMANTIC) {
    return "semantic_" + GetCameraName(sensor.id());
  }
  return "unknown sensor type " + std::to_string(sensor.type());
}

void sim_label::WriteClipAttributes(sim_msg::SensorMeta sensor_meta) {
  Json::Value clip_attributes;

  static const std::vector<std::string> sensor_keys = {"camera_front",       "camera_front_30fov", "camera_front_left",
                                                       "camera_front_right", "camera_rear_left",   "camera_rear_right",
                                                       "camera_rear",        "fisheye_front",      "fisheye_left",
                                                       "fisheye_right",      "fisheye_rear"};

  // static const std::string<std::string> calibration_keys = {
  //     "camera_front",        "camera_front_30fov",   "camera_frontleft",    "camera_frontright",
  //     "camera_rearleft",     "camera_rearright",     "camera_rear",         "camera_fisheye_front",
  //     "camera_fisheye_left", "camera_fisheye_right", "camera_fisheye_rear",
  // };

  static std::map<std::string, std::string> sensor_2_chassis_keys = {
      {"camera_front", "camera_front"},
      {"camera_front_30fov", "camera_front_30fov"},
      {"camera_front_left", "camera_frontleft"},
      {"camera_front_right", "camera_frontright"},
      {"camera_rear_left", "camera_rearleft"},
      {"camera_rear_right", "camera_rearright"},
      {"camera_rear", "camera_rear"},
      {"fisheye_front", "camera_fisheye_front"},
      {"fisheye_left", "camera_fisheye_left"},
      {"fisheye_right", "camera_fisheye_right"},
      {"fisheye_rear", "camera_fisheye_rear"}
  };

  std::string pack_time = getFormattedTimestamp();

  Json::Value info;
  info["pack"] = pack_time;
  info["plate"] = sensor_meta.clip().plate();
  info["status"] = sensor_meta.clip().status();
  info["start_timestamp"] = sensor_meta.clip().start_timestamp();
  info["end_timestamp"] = sensor_meta.clip().end_timestamp();

  clip_attributes[pack_time] = info;
  clip_attributes["calibration"] = Calibration;

  for (const auto& sensor_key : sensor_keys) {
    std::string sensor_2_chassis_key = sensor_key + "_2_chassis";
    if (sensor_2_chassis_keys.find(sensor_key) != sensor_2_chassis_keys.end()) {
      std::cout << "override " << sensor_key << " 2 chassis" << std::endl;
      Eigen::Matrix4d matrix = camera_to_vcs_map[sensor_2_chassis_keys[sensor_key]].matrix();
      Json::Value jsonMatrix;
      for (int row = 0; row < 4; ++row) {
        Json::Value jsonRow(Json::arrayValue);
        for (int col = 0; col < 4; ++col) {
          jsonRow[col] = matrix(row, col);
          if(sensor_key == "camera_front_2_chassis"){
            std::cout << "camera_front_2_chassis " << matrix(row, col) << std::endl;
          }
        }
        jsonMatrix[row] = jsonRow;
      }
      clip_attributes["calibration"][sensor_2_chassis_key] = jsonMatrix;
    }else{
      std::cout << "no override " << sensor_key << " 2 chassis" << std::endl;
    }
  }

  Json::Value frames = {};

  for (int time = sensor_meta.clip().start_timestamp(); time <= sensor_meta.clip().end_timestamp(); time += 100) {
    frames.append(timeStarmString(time));
  }

  for (const auto &sensor_key : sensor_keys) {
    clip_attributes["unsync"][sensor_key] = frames;
    clip_attributes["sync"][sensor_key] = frames;
  }

  sensor_meta.mutable_clip()->set_pack_time(pack_time);

  clip_infos.push_back(sensor_meta.clip());

  std::string save_path = savePath + "/attribute.json";
  std::ofstream ofs(save_path);
  ofs << clip_attributes.toStyledString();
  ofs.close();
  std::cout << "write clip attributes " << sensor_meta.clip().id() << " to " << save_path << std::endl;
}

void sim_label::WriteClipPose(sim_msg::SensorMeta sensor_meta) {
  std::string pose_path = savePath + "/pose_ue.txt";

  std::ofstream file(pose_path);
  if (!file.is_open()) {
    std::cerr << "Failed to open file!" << std::endl;
    return;
  }
  // file << std::fixed << std::setprecision(6);

  int clip_id = sensor_meta.clip().id();
  for (const auto &pose : poses[clip_id]) {
    file << std::setw(10) << std::setfill('0') << pose.first;
    file << " " << pose.second.x() << " " << pose.second.y() << " " << pose.second.z() << " " << pose.second.qx() << " "
         << pose.second.qy() << " " << pose.second.qz() << " " << pose.second.qw() << std::endl;
  }

  std::cout << "write clip pose " << clip_id << " to " << pose_path << std::endl;

  file.close();
}

void sim_label::WriteSiteAttributes() {
  Json::Value site;
  Json::Value clips;
  for (const auto &clip_info : clip_infos) {
    Json::Value clip;
    Json::Value clip_info_json;
    clip["clipid"] = clip_info.id();
    clip["mapping_type"] = clip_info.mapping_type();
    clip_info_json["start_timestamp"] = clip_info.start_timestamp();
    clip_info_json["end_timestamp"] = clip_info.end_timestamp();
    clip_info_json["pack"] = clip_info.pack_time();
    clip_info_json["plate"] = clip_info.plate();
    clip_info_json["status"] = clip_info.status();
    clip_info_json["tags"] = clip_info.tags();

    clip[clip_info.pack_time()] = clip_info_json;
    clips[clip_info.record_time()] = clip;

    std::cout << "start_timestamp " << clip_info.start_timestamp();
    std::cout << "end_timestamp " << clip_info.end_timestamp();
  }
  site["clips"] = clips;

  std::string site_path = savePathBase + "/" + site_name + "/attribute.json";
  std::ofstream ofs(site_path);
  ofs << site.toStyledString();
  ofs.close();
  std::cout << "write site attributes "
            << " to " << site_path << std::endl;
}

std::string sim_label::getFormattedTimestamp() {
  // Get current time with milliseconds precision
  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);

  // Convert to time_t for date components
  auto now_c = std::chrono::system_clock::to_time_t(now);

  // Convert to tm struct for local time
  std::tm tm = *std::localtime(&now_c);

  // Get milliseconds component
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  // Format the string
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y%m%d-%H%M%S") << "_" << std::setfill('0') << std::setw(3) << ms.count();

  return oss.str();
}

std::string sim_label::GetCameraName(int id) {
  static const std::vector<std::string> camera_names = {
      "camera_front",     "camera_front_30fov", "camera_front_left", "camera_front_right",
      "camera_rear_left", "camera_rear_right",  "camera_rear",
  };
  if (id >= 0 && id < camera_names.size()) {
    return camera_names[id];
  }
  return "unknown camera id " + std::to_string(id);
}

std::string sim_label::GetFisheyeName(int id) {
  static const std::vector<std::string> fisheye_names = {"fisheye_front", "fisheye_front_left", "fisheye_front_right",
                                                         "fisheye_rear"};
  if (id >= 0 && id < fisheye_names.size()) {
    return fisheye_names[id];
  }
  return "unknown fisheye id " + std::to_string(id);
}

bool sim_label::LoadCalibration() {
  std::ifstream ifs(CalibrationPath);
  if (!ifs.is_open()) {
    std::cout << "CalibrationPath " << CalibrationPath << " does not exist" << std::endl;
    return false;
  }
  Json::Reader reader;
  Json::Value root;
  if (!reader.parse(ifs, root)) {
    std::cout << "CalibrationPath " << CalibrationPath << " is not a valid json file" << std::endl;
    return false;
  }

  std::vector<std::string> in_keys = {"lidar_top_2_camera_front",
                                      "lidar_top_2_camera_rear",
                                      "lidar_top_2_camera_frontleft",
                                      "lidar_top_2_camera_frontright",
                                      "lidar_top_2_camera_rearleft",
                                      "lidar_top_2_camera_rearright",
                                      "lidar_top_2_camera_front_30fov",
                                      "lidar_top_2_camera_fisheye_front",
                                      "lidar_top_2_camera_fisheye_rear",
                                      "lidar_top_2_camera_fisheye_left",
                                      "lidar_top_2_camera_fisheye_right",
                                      "lidar_top_2_vcs",
                                      "falconk_2_vcs",
                                      "atf_2_vcs",
                                      "atl_2_vcs",
                                      "atr_2_vcs",
                                      "atb_2_vcs",
                                      "qtb_2_vcs",
                                      "qtf_2_vcs",
                                      "qtl_2_vcs",
                                      "qtr_2_vcs",
                                      "atx_2_vcs",
                                      "camera_front_2_chassis",
                                      "camera_rear_2_chassis",
                                      "camera_frontleft_2_chassis",
                                      "camera_frontright_2_chassis",
                                      "camera_rearleft_2_chassis",
                                      "camera_rearright_2_chassis",
                                      "camera_front_30fov_2_chassis",
                                      "camera_fisheye_front_2_chassis",
                                      "camera_fisheye_rear_2_chassis",
                                      "camera_fisheye_left_2_chassis",
                                      "camera_fisheye_right_2_chassis",
                                      "camera_front",
                                      "camera_rear",
                                      "camera_frontleft",
                                      "camera_frontright",
                                      "camera_rearleft",
                                      "camera_rearright",
                                      "camera_front_30fov",
                                      "camera_fisheye_front",
                                      "camera_fisheye_rear",
                                      "camera_fisheye_left",
                                      "camera_fisheye_right"};

  std::vector<std::string> out_keys = {"lidar_top_2_camera_front",
                                       "lidar_top_2_camera_rear",
                                       "lidar_top_2_camera_front_left",
                                       "lidar_top_2_camera_front_right",
                                       "lidar_top_2_camera_rear_left",
                                       "lidar_top_2_camera_rear_right",
                                       "lidar_top_2_camera_front_30fov",
                                       "lidar_top_2_fisheye_front",
                                       "lidar_top_2_fisheye_rear",
                                       "lidar_top_2_fisheye_left",
                                       "lidar_top_2_fisheye_right",
                                       "lidar_top_2_chassis",
                                       "falconk_2_chassis",
                                       "atf_2_chassis",
                                       "atl_2_chassis",
                                       "atr_2_chassis",
                                       "atb_2_chassis",
                                       "qtb_2_chassis",
                                       "qtf_2_chassis",
                                       "qtl_2_chassis",
                                       "qtr_2_chassis",
                                       "atx_2_chassis",
                                       "camera_front_2_chassis",
                                       "camera_rear_2_chassis",
                                       "camera_front_left_2_chassis",
                                       "camera_front_right_2_chassis",
                                       "camera_rear_left_2_chassis",
                                       "camera_rear_right_2_chassis",
                                       "camera_front_30fov_2_chassis",
                                       "fisheye_front_2_chassis",
                                       "fisheye_rear_2_chassis",
                                       "fisheye_left_2_chassis",
                                       "fisheye_right_2_chassis",
                                       "camera_front",
                                       "camera_rear",
                                       "camera_front_left",
                                       "camera_front_right",
                                       "camera_rear_left",
                                       "camera_rear_right",
                                       "camera_front_30fov",
                                       "fisheye_front",
                                       "fisheye_rear",
                                       "fisheye_left",
                                       "fisheye_right"

  };

  std::cout << "in_keys " << in_keys.size() << std::endl;
  std::cout << "out_keys " << out_keys.size() << std::endl;

  if (in_keys.size() != out_keys.size()) {
    std::cout << "in_keys and out_keys size mismatch" << std::endl;
    return false;
  }

  std::string distortion_key = "d";

  for (int i = 0; i < in_keys.size(); i++) {
    std::cout << "in_keys " << in_keys[i] << " out_keys " << out_keys[i] << std::endl;
    if (root.isMember(in_keys[i])) {
      auto &node = root[in_keys[i]];
      if (node.isObject() && node.isMember(distortion_key)) {
        Json::Value &d_params = node[distortion_key];
        if (d_params.isArray()) {
          for (int i = 0; i < d_params.size(); i++) {
            d_params[i] = 0.0;
          }
        }
      }
      Calibration[out_keys[i]] = node;
    } else {
      std::cout << "in_keys " << in_keys[i] << " not found" << std::endl;
      return false;
    }
  }

  const static std::vector<std::string> camera_names = {
      "camera_front",        "camera_front_30fov",   "camera_frontleft",   "camera_frontright",
      "camera_rearleft",     "camera_rearright",     "camera_rear",        "camera_fisheye_front",
      "camera_fisheye_left", "camera_fisheye_right", "camera_fisheye_rear"};

  for (size_t i = 0; i < camera_names.size(); ++i) {
    std::string camera_info_key = camera_names[i] + "_json";
    auto base_calib_info = root[camera_info_key];
    auto camera_to_local = PoseToAffine(base_calib_info["roll"].asDouble(), base_calib_info["pitch"].asDouble(),
                                        base_calib_info["yaw"].asDouble(), base_calib_info["camera_x"].asDouble(),
                                        base_calib_info["camera_y"].asDouble(), base_calib_info["camera_z"].asDouble());
    auto local_to_vcs = PoseToAffine(
        base_calib_info["vcs"]["rotation"][0].asDouble(), base_calib_info["vcs"]["rotation"][1].asDouble(),
        base_calib_info["vcs"]["rotation"][2].asDouble(), base_calib_info["vcs"]["translation"][0].asDouble(),
        base_calib_info["vcs"]["translation"][1].asDouble(), base_calib_info["vcs"]["translation"][2].asDouble());

    auto camera_to_vcs = local_to_vcs * camera_to_local;
    camera_to_vcs_map[camera_names[i]] = camera_to_vcs;
    double x, y, z, roll, pitch, yaw;
    AffineToPose(camera_to_vcs, x, y, z, roll, pitch, yaw);
    std::cout << "camera_to_vcs " << camera_names[i] << " " << camera_to_vcs.matrix() << std::endl;
    std::cout << "camera_to_vcs " << camera_names[i] << " xyz_rpy " << x << " " << y << " " << z << " " << roll << " "
              << pitch << " " << yaw << std::endl;
  }

  return true;
}

Eigen::Affine3d sim_label::PoseToAffine(double roll, double pitch, double yaw, double x, double y, double z) {
  // Create a quaternion from Euler angles
  Eigen::Quaterniond q(Eigen::AngleAxisd(yaw, Eigen::Vector3d::UnitZ())*
                       Eigen::AngleAxisd(pitch, Eigen::Vector3d::UnitY()) *
                       Eigen::AngleAxisd(roll, Eigen::Vector3d::UnitX())
                       );

  Eigen::Vector3d translation(x, y, z);

  // Create an Affine3d transformation
  Eigen::Affine3d transformation = Eigen::Affine3d::Identity();  // Start with an identity matrix
  transformation.translate(translation);                         // Apply translation
  transformation.rotate(q);                                      // Apply rotation
  return transformation;
}

void sim_label::AffineToPose(const Eigen::Affine3d &affine, double &x, double &y, double &z, double &roll,
                             double &pitch, double &yaw) {
  Eigen::Vector3d xyz = affine.translation();
  Eigen::Vector3d rpy =
      affine.rotation().eulerAngles(2, 1, 0);
  x = xyz[0];
  y = xyz[1];
  z = xyz[2];
  //to degree
  roll = rpy[2] * 180 / M_PI;
  pitch = rpy[1] * 180 / M_PI;
  yaw = rpy[0] * 180 / M_PI;
}

/**
 * @brief Stop method implementation for SimLabel class
 *
 * @param helper helper of tx_sim
 */
void sim_label::Stop(tx_sim::StopHelper &helper) {
  // Reset thread pool and queues
  queues.reset();
  threads.reset();

  // Acquire save mutex before clearing saved_files vector
  {
    std::unique_lock<std::mutex> lock(save_mutex);
    saved_files.clear();
  }

  // Log stop event
  std::cout << "stop.\n";
}

/**
 * @brief Parse image sensor data into appropriate format
 *
 * @param prefix Prefix of image file name
 * @param buf Raw sensor data
 * @return true on success
 * @return false
 */
bool sim_label::parseImage(const sim_msg::SensorRaw::Sensor &sensor, ImageInfo &info) {
  // sensor proto
  sim_msg::CameraRaw camera;
  if (!camera.ParseFromString(sensor.raw())) {
    std::cout << "camera error";
    return false;
  }

  std::string folder = GetSensorFolder(sensor);

  // the path to write
  auto ffnm = folder + "/" + timeStarmString(camera.timestamp());
  if (camera.type() == "JPEG") {
    ffnm += ".jpg";
  } else if (camera.type() == "PNG") {
    ffnm += ".png";
  } else if (camera.type() == "EXR") {
    return false;
    ffnm += ".exr";
  } else {
    return false;
  }

  // std::cout << "camera raw timestamp " << camera.timestamp() << std::endl;

  // the info of image
  info.id = camera.id();
  info.fpath = ffnm;
  info.size = camera.image_data().size();
  info.timestamp = static_cast<std::int64_t>(camera.timestamp());
  info.height = camera.height();
  info.width = camera.width();
  info.buffer = camera.image_data();
  info.pose.x = camera.pose().longitude();
  info.pose.y = camera.pose().latitude();
  info.pose.z = camera.pose().altitude();
  info.pose.roll = camera.pose().roll();
  info.pose.pitch = camera.pose().pitch();
  info.pose.yaw = camera.pose().yaw();
  return true;
}

/**
 * @brief lidar from pb string
 *
 * @param prefix Prefix of lidar file name
 * @param buf Raw sensor data
 * @return true
 * @return false
 */
bool sim_label::parseLidar(const std::string &buf, PcInfo &info) {
  sim_msg::LidarRaw lidar;
  if (!lidar.ParseFromString(buf)) {
    std::cout << "lidar error";
    return false;
  }
  std::int64_t timestamp = static_cast<std::int64_t>((lidar.timestamp_begin() + lidar.timestamp_end()) * 0.5);
  // the path of pcd file to be saved
  auto ffnm = timeStarmString(timestamp) + "_" + std::to_string(lidar.id()) + ".pcd";

  size_t num = lidar.count();

  std::stringstream pcdbuf;
  // write the header of pcd format file
  pcdbuf << "# .PCD v.7 - Point Cloud Data file format\n";
  pcdbuf << "VERSION 0.7\n";
  pcdbuf << "FIELDS x y z intensity label\n";
  pcdbuf << "SIZE 4 4 4 4 4\n";
  pcdbuf << "TYPE F F F F I\n";
  pcdbuf << "COUNT 1 1 1 1 1\n";
  pcdbuf << "WIDTH " << num << "\n";
  pcdbuf << "HEIGHT 1\n";
  pcdbuf << "VIEWPOINT 0 0 0 1 0 0 0\n";
  pcdbuf << "POINTS " << num << "\n";
  pcdbuf << "DATA binary\n";

  // write point raw
  if (lidar.point_lists().size() > 0) {
    std::cout << "use bytes" << std::endl;
    pcdbuf.write(lidar.point_lists().data(), lidar.point_lists().size());
  } else {
    // or write x y z i by order
#pragma pack(push, 4)
    struct TMP {
      float x = 0;
      float y = 0;
      float z = 0;
      float i = 0;          // intensity
      std::uint32_t t = 0;  // type
    };
#pragma pack(pop)

    std::vector<TMP> datas;
    datas.reserve(num);
    for (const auto &p : lidar.points()) {
      TMP tmp;
      tmp.x = p.x();
      tmp.y = p.y();
      tmp.z = p.z();
      tmp.i = p.i();
      tmp.t = p.t();
      datas.push_back(tmp);
    }
    pcdbuf.write(reinterpret_cast<const char *>(datas.data()), 5 * num * sizeof(float));
  }

  // the info of point
  info.id = lidar.id();
  info.fpath = ffnm;
  info.size = pcdbuf.str().length();
  info.timestamp = timestamp;
  info.fpath = ffnm;
  info.count = lidar.count();
  info.buffer = pcdbuf.str();
  info.coord_type = lidar.coord_type();
  info.pose.x = lidar.pose_last().longitude();
  info.pose.y = lidar.pose_last().latitude();
  info.pose.z = lidar.pose_last().altitude();
  info.pose.roll = lidar.pose_last().roll();
  info.pose.pitch = lidar.pose_last().pitch();
  info.pose.yaw = lidar.pose_last().yaw();
  info.timestamp_bg = lidar.timestamp_begin();
  info.timestamp_ed = lidar.timestamp_end();

  std::cout << "add lidar " << info.id << " pose first " << lidar.pose_first().longitude() << " "
            << lidar.pose_first().latitude() << " " << lidar.pose_first().altitude() << " "
            << " roll " << lidar.pose_first().roll() << " pitch " << lidar.pose_first().pitch() << " yaw "
            << lidar.pose_first().yaw() << " pose last " << lidar.pose_last().longitude() << " "
            << lidar.pose_last().latitude() << " " << lidar.pose_last().altitude() << " roll "
            << lidar.pose_last().roll() << " pitch " << lidar.pose_last().pitch() << " yaw " << lidar.pose_last().yaw()
            << " timestamp begin " << lidar.timestamp_begin() << " timestamp end " << lidar.timestamp_end()
            << std::endl;

  // " pose last "
  // << info.pose_last().longitude() << " " << info.pose_last().latitude() << " " << info.pose_last().altitude()
  // << " roll " << info.pose.roll << " pitch " << info.pose.pitch << " yaw " << info.pose.yaw << " timestamp "
  // << info.timestamp << " timestamp begin " << info.timestamp_bg << " timestamp end " << info.timestamp_ed
  // << std::endl;

  return true;
}

/**
 * @brief Save file method implementation for SimLabel class
 *
 * @param fname The name of the file to be saved
 * @param buf The binary data to be saved
 * @return true
 * @return false
 */
bool sim_label::saveFile(const std::string &fname, const std::string &buf) {
  // {
  //   // Lock save mutex before checking if filename already exists in saved files
  //   // set
  //   std::unique_lock<std::mutex> lock(save_mutex);
  //   if (saved_files.find(fname) != saved_files.end()) {
  //     return true;
  //   }
  //   // Update saved files set after successfully saving file
  //   saved_files.insert(fname);
  // }

  // Open output stream for writing binary data to disk
  std::fstream of(fname, std::ios::out | std::ios::binary);
  if (of.is_open()) {
    of.write(buf.c_str(), buf.size());
    of.close();
    //std::cout << "save " << fname << " done." << std::endl;
  } else {
    std::cout << "save " << fname << " failed." << std::endl;
  }

  // Log successful save operation
  std::cout << std::endl;
  return true;
}

/**
 * @brief Save opencv image method implementation for SimLabel class
 *
 * @param fname The name of the file to be saved
 * @param buf The binary data of cv::Mat image to be saved
 * @return true
 * @return false
 */
bool sim_label::saveOpenCVImage(const std::string &fname, const std::string &buf, int width, int height,
                                sim_msg::SensorRaw_Type type) {
  int cv_type = 0;
  if (type == sim_msg::SensorRaw_Type_TYPE_DEPTH || type == sim_msg::SensorRaw_Type_TYPE_FISHEYE_DEPTH) {
    cv_type = CV_16UC1;
  }

  cv::Mat mat(height, width, cv_type, const_cast<void *>(reinterpret_cast<const void *>(buf.data())));
  static std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 4};
  if (!cv::imwrite(fname, mat, params)) {
    std::cout << "save depth image error " << fname << std::endl;
    return false;
  }
  //std::cout << "save depth image " << fname << std::endl;
  return true;
}

/**
 * @brief This code snippet demonstrates how to use C++ Boost libraries to
 * extract relevant information from an image package, generate necessary
 * metadata, and enqueue tasks to save both JPEG and PNG files alongside their
 * respective JSON files. The main steps include extracting timestamps, getting
 * UTC dates, binding functions to save files, defining temporary structures,
 * checking input types, iterating over sensors, processing detected objects'
 * outlines, calculating UVs for visible pixels within bounding boxes,
 * initializing visible buffer with calculated UV coordinates, calling
 * `ReVisable` method to update visibility status, generating convex hull
 * polygons using Boost Geometry library, creating JSON metadata, enqueuing
 * tasks to save JPEG and PNG files, constructing JSON source object, appending
 * related files to dataset, writing JSON output streams, and finally testing
 * the functionality by exporting GeoJSON format of extracted objects.
 *
 * @param info The image package to be processed
 */
void sim_label::saveImageLabel(const ImagePackage &info) {
  // Get timestamp string and UTC date/time string from image package
  std::string tss = timeStarmString(info.image.timestamp);
  std::string utc = getUTC();
  // Bind a function that saves file with given key-value pair
  auto sfun = std::bind(&sim_label::saveFile, this, std::placeholders::_1, std::placeholders::_2);

  // Define temporary object structure used in saving data

  // ImageLabel label(info);
  // label.init(minArea, maxDistance, completeness, fullBox);

  // std::string dir0, dir1;
  // // Construct JSON source object
  // Json::Value source = label.label(dir0, dir1);

  const auto &jpginfo = info.image;
  threads->enqueue(sfun, savePath + "/" + jpginfo.fpath, jpginfo.buffer);

  // save json object as string
  // Json::StreamWriterBuilder builder;
  // builder["commentStyle"] = "None";
  // builder["indentation"] = "";
  // const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  // std::stringstream oss;
  // writer->write(source, &oss);
  // // save the file in queue
  // threads->enqueue(sfun, dir0 + "/" + tss + "_" + std::to_string(jpginfo.id) + ".json", oss.str());

  // save geojson for debug
  // you can drop geojson in QGIS for a look
  // if (debugFiles) {
  //   Json::Value geojson;
  //   geojson["type"] = "FeatureCollection";
  //   for (const auto &key : source["openlabel"]["objects"].getMemberNames()) {
  //     const Json::Value &obj = source["openlabel"]["objects"][key];
  //     const Json::Value &odata = obj["object_data"]["poly2d"][0]["val"];
  //     Json::Value feature;
  //     feature["type"] = "Feature";
  //     feature["geometry"]["type"] = "LineString";
  //     auto pn = odata.size() / 2;
  //     for (std::uint32_t i = 0; i < pn; i++) {
  //       Json::Value jp;
  //       jp.append(odata[i * 2].asDouble());
  //       jp.append(-(odata[i * 2 + 1].asDouble()));
  //       feature["geometry"]["coordinates"].append(jp);
  //     }
  //     geojson["features"].append(feature);
  //   }
  //   Json::StreamWriterBuilder builder;
  //   const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  //   std::stringstream oss;
  //   writer->write(geojson, &oss);
  //   threads->enqueue(sfun, dir1 + "/" + jpginfo.fpath + ".json", oss.str());
  // }
}

void sim_label::saveDepthImageLabel(const ImagePackage &info) {
  // Get timestamp string and UTC date/time string from image package
  std::string tss = timeStarmString(info.image.timestamp);
  std::string utc = getUTC();
  auto save_opencv_fun = std::bind(&sim_label::saveOpenCVImage, this, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
  const auto &pnginfo = info.image;
  threads->enqueue(save_opencv_fun, savePath + "/" + pnginfo.fpath, pnginfo.buffer, pnginfo.width, pnginfo.height,
                   info.type);
}

/**
 * @brief save pcd label
 *
 * @param info pcd info
 */
void sim_label::savePcdLabel(const PcdPackage &info) {
  std::cout << "savePcdLabel obj " << info.obj.timestamp() << " lidar " << info.lidar.timestamp << std::endl;
  // Get timestamp string and UTC date/time string from lidar package
  std::string tss = timeStarmString(info.lidar.timestamp);
  std::string utc = getUTC();

  // Define temporary object structure used in saving data
  auto sfun = std::bind(&sim_label::saveFile, this, std::placeholders::_1, std::placeholders::_2);

  // find out these object in lidar view
  struct TempObject {
    int t;
    sim_msg::DisplayPose::Object obj;
    double x, y, z;
    double roll, pitch, yaw;
  };
  std::vector<TempObject> detect_objects;

  // read object
  const auto &obj_ops = info.obj;
  // hand object from display
  if (lidars.find(info.lidar.id) == lidars.end()) {
    std::cout << "cannot find lidar in sensor config. id = " << info.lidar.id << std::endl;
  } else {
    // found out these visable object
    auto &lidar = lidars[info.lidar.id];
    lidar->setCarPosition(Eigen::Vector3d::Zero());
    lidar->setCarRotation(info.lidar.pose.roll, info.lidar.pose.pitch, info.lidar.pose.yaw);
    std::cout << " lidar roll " << info.lidar.pose.roll << " pitch " << info.lidar.pose.pitch << " yaw "
              << info.lidar.pose.yaw << std::endl;
    std::cout << " lidar x " << info.lidar.pose.x << " y " << info.lidar.pose.y << " z " << info.lidar.pose.z
              << std::endl;
    std::vector<std::pair<int, sim_msg::DisplayPose::Object>> dobjects;
    for (const auto &car : obj_ops.egos()) {
      dobjects.push_back(std::make_pair(-1, car));
    }
    for (const auto &car : obj_ops.cars()) {
      dobjects.push_back(std::make_pair(0, car));
    }
    for (const auto &sta : obj_ops.staticobstacles()) {
      dobjects.push_back(std::make_pair(1, sta));
    }
    for (const auto &dyn : obj_ops.dynamicobstacles()) {
      dobjects.push_back(std::make_pair(2, dyn));
    }

    for (const auto &dobj : dobjects) {
      Eigen::Vector3d pos(dobj.second.pose().longitude(), dobj.second.pose().latitude(), dobj.second.pose().altitude());
      std::cout << " obj coor " << dobj.second.id() << " type " << dobj.second.type() << " raw type "
                << dobj.second.raw_type() << " x " << pos.x() << " y " << pos.y() << " z " << pos.z() << std::endl;
      coord_trans_api::lonlat2enu(pos.x(), pos.y(), pos.z(), info.lidar.pose.x, info.lidar.pose.y, info.lidar.pose.z);
      std::cout << " local coor " << dobj.second.id() << " type " << dobj.second.type() << " x " << pos.x() << " y "
                << pos.y() << " z " << pos.z() << std::endl;
      double distance = pos.norm();
      // filter by distance
      if (distance > maxDistance) {
        continue;
      }
      // filter by fov
      // if (!lidar->inFov(Catalog::getInstance().getBboxPts(
      //         std::make_pair(dobj.first, dobj.first == -1 ? dobj.second.id() : dobj.second.raw_type()), pos,
      //         dobj.second.pose().roll(), dobj.second.pose().pitch(), dobj.second.pose().yaw()))) {
      //   continue;
      // }
      pos = lidar->FovVectorOnlyCar(pos);
      TempObject tobj;
      auto center = Catalog::getInstance().getCenterOffset(
          std::make_pair(dobj.first, dobj.first == -1 ? dobj.second.id() : dobj.second.raw_type()));
      std::cout << " obj in lidar coor " << dobj.second.id() << " type " << dobj.second.type() << " x " << pos.x()
                << " y " << pos.y() << " z " << pos.z() << std::endl;
      std::cout << " obj " << dobj.second.raw_type() << " bbox center " << dobj.second.center_x() << " "
                << dobj.second.center_y() << " " << dobj.second.center_z() << " center offset x " << center.x() << " y "
                << center.y() << " z " << center.z() << std::endl;
      tobj.t = dobj.first;
      tobj.obj = dobj.second;

      tobj.roll = dobj.second.pose().roll();
      tobj.pitch = dobj.second.pose().pitch();
      tobj.yaw = dobj.second.pose().yaw();

      lidar->FovRotator(tobj.roll, tobj.pitch, tobj.yaw);

      Eigen::Quaterniond q(Eigen::AngleAxisd(tobj.yaw, Eigen::Vector3d::UnitZ()));
      Eigen::Vector3d offset_local(center.x() + dobj.second.center_x(), center.y() + dobj.second.center_y(),
                                   center.z() + dobj.second.center_z());

      auto offset = q * offset_local;
      tobj.x = pos.x() + offset.x();
      tobj.y = pos.y() + offset.y();
      tobj.z = pos.z() + offset.z();

      std::cout << "OFFSET " << offset.x() << " " << offset.y() << " " << offset.z() << " " << tobj.roll << " "
                << tobj.pitch << " " << tobj.yaw << std::endl;

      detect_objects.push_back(tobj);
    }
  }

  // save pcd file
  const auto &pcdinfo = info.lidar;
  threads->enqueue(sfun, "lidar/pcd/" + pcdinfo.fpath, pcdinfo.buffer);
  auto pcdname = boost::filesystem::path(pcdinfo.fpath).filename().string();

  // write base info
  Json::Value source;
  Json::Value &label = source["openlabel"];
  // metadata
  label["metadata"]["schema_version"] = "1.0.0";
  // coordinate_systems
  std::string sensor_cs = "lidar";
  sensor_cs += std::to_string(pcdinfo.id);
  label["coordinate_systems"]["geospatial-wgs84"]["type"] = "geo";
  label["coordinate_systems"]["geospatial-wgs84"]["parent"] = "";
  label["coordinate_systems"]["geospatial-wgs84"]["children"].append(sensor_cs);
  Json::Value &cs = label["coordinate_systems"][sensor_cs];
  cs["type"] = "geo";
  cs["parent"] = "geospatial-wgs84";
  cs["children"].resize(0);
  // euler_angles
  cs["pose_wrt_parent"]["euler_angles"].append(pcdinfo.pose.roll);
  cs["pose_wrt_parent"]["euler_angles"].append(pcdinfo.pose.pitch);
  cs["pose_wrt_parent"]["euler_angles"].append(pcdinfo.pose.yaw);
  // translation
  cs["pose_wrt_parent"]["translation"].append(pcdinfo.pose.x);
  cs["pose_wrt_parent"]["translation"].append(pcdinfo.pose.y);
  cs["pose_wrt_parent"]["translation"].append(pcdinfo.pose.z);
  // order
  cs["pose_wrt_parent"]["sequence"] = "zyx";
  // streams
  Json::Value &stream = label["streams"][sensor_cs];
  stream["type"] = "lidar";
  stream["description"] = sensor_cs;
  stream["uri"] = "lidar/pcd/" + pcdinfo.fpath;

  // objects
  Json::Value &objects = label["objects"];
  for (const auto &detection : detect_objects) {
    int id = detection.obj.id();
    std::string name = "car";
    if (detection.t == 1) {
      id += 10000;
      name = "static obstacle";
    } else if (detection.t == 2) {
      id = 20000 - id;
      name = "dynamic obstacle";
    }
    Json::Value &obj = objects[std::to_string(id)];
    obj["name"] = findTypeFromUE(detection.obj.type());
    if (obj["name"] == "truck" && detection.obj.raw_type() == -1) {
      obj["type"] = "trailer";
    } else {
      obj["type"] = name;
    }
    obj["coordinate_system"] = sensor_cs;
    obj["object_data"]["cuboid"].resize(1);
    Json::Value &odata = obj["object_data"]["cuboid"][0];
    odata["name"] = "bbox3d";
    // box
    odata["val"].append(detection.x);
    odata["val"].append(detection.y);
    odata["val"].append(detection.z);
    odata["val"].append(detection.yaw);
    odata["val"].append(detection.pitch);
    odata["val"].append(detection.roll);
    odata["val"].append(detection.obj.length());
    odata["val"].append(detection.obj.width());
    odata["val"].append(detection.obj.height());
  }

  Json::StreamWriterBuilder builder;
  builder["commentStyle"] = "None";
  builder["indentation"] = "";
  const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  std::stringstream oss;
  writer->write(source, &oss);
  threads->enqueue(sfun, "lidar/" + tss + "_" + std::to_string(pcdinfo.id) + ".json", oss.str());

  // test
  // write obj file
  // you can drop it into cloudcampare and watch it
  if (debugFiles) {
    std::stringstream test;
    int nn = 0;
    for (const auto &detection : detect_objects) {
      auto &info = detection;
      Eigen::Quaterniond q(Eigen::AngleAxisd(info.yaw, Eigen::Vector3d::UnitZ()));
      Eigen::Affine3d tfs = Eigen::Translation3d(info.x, info.y, info.z) * q.toRotationMatrix();

      std::cout << "debug " << info.obj.raw_type() << " " << info.x << " " << info.y << " " << info.z << " "
                << " " << info.yaw << " " << info.obj.length() << " " << info.obj.width() << " " << info.obj.height()
                << std::endl;

      const double offx[8] = {-0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5};
      const double offy[8] = {-0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5};
      const double offz[8] = {-0.5, -0.5, -0.5, -0.5, 0.5, 0.5, 0.5, 0.5};
      for (int i = 0; i < 8; i++) {
        double x = offx[i] * info.obj.length();
        double y = offy[i] * info.obj.width();
        double z = offz[i] * info.obj.height();

        Eigen::Vector3d ep = tfs * Eigen::Vector3d(x, y, z);
        test << "v " << ep.x() << " " << ep.y() << " " << ep.z() << "\n";
      }

      test << "l " << nn + 1 << " " << nn + 2 << " " << nn + 3 << " " << nn + 4 << " " << nn + 1 << "\n";
      test << "l " << nn + 5 << " " << nn + 6 << " " << nn + 7 << " " << nn + 8 << " " << nn + 5 << "\n";
      test << "l " << nn + 1 << " " << nn + 5 << "\n";
      test << "l " << nn + 2 << " " << nn + 6 << "\n";
      test << "l " << nn + 3 << " " << nn + 7 << "\n";
      test << "l " << nn + 4 << " " << nn + 8 << "\n";
      nn += 8;
    }

    threads->enqueue(sfun, "lidar/pcd/" + pcdinfo.fpath + ".obj", test.str());
  }
}

/**
 * @brief This code snippet demonstrates a simple helper function that returns
 * the current UTC timestamp represented as a string. It uses the Chrono library
 * provided by C++ standard library to obtain the current system clock time,
 * converts it to Unix Epoch time, multiplies by 1000 to make it milliseconds
 * accurate, and then formats it as a string. Helper function to get current UTC
 * timestamp as a string
 *
 * @return std::string of UTC timestamp
 */
std::string sim_label::getUTC() {
  auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  return std::to_string(now * 1000);
}

/**
 * @brief get config from protobuf message
 *
 * @param buffer txsim protobuf message
 * @return true on success
 * @return false on faild
 */
bool sim_label::getSensorConfig(const std::string &buffer, const std::string &groupname) {
  if (buffer.empty()) {
    return false;
  }
  // Deserialize protobuf message
  sim_msg::Scene scene;
  if (!scene.ParseFromString(buffer)) {
    std::cout << "parse scene faild.";
    return false;
  }
  // Clear existing camera/LiDAR lists
  cameras.clear();
  lidars.clear();
  semantics.clear();
  // Iterate through egos and process sensors
  for (const auto &ego : scene.egos()) {
    if (ego.group() != groupname) continue;
    for (const auto &sensor : ego.sensor_group().sensors()) {
      // Handle camera case
      if (sensor.type() == sim_msg::SENSOR_TYPE_CAMERA || sensor.type() == sim_msg::SENSOR_TYPE_SEMANTIC) {
        LoadCamera(sensor, device);
      } else if (sensor.type() == sim_msg::SENSOR_TYPE_FISHEYE) {
        // Handle FISHEYE case
        LoadFisheye(sensor, device);
      } else if (sensor.type() == sim_msg::SENSOR_TYPE_TRADITIONAL_LIDAR) {
        // Handle LiDAR case
        LoadLidar(sensor, device);
      }
    }
  }
  return true;
}

/**
 * @brief SAVE mask json file
 *
 * @param fpath
 * @return true
 * @return false
 */
bool sim_label::saveMaskJson(const std::string &fpath) {
  Json::Value root;
  root["maskMapping"]["ground"] = 16;
  root["maskMapping"]["road"] = 12;
  root["maskMapping"]["tree"] = 150;
  root["maskMapping"]["man"] = 90;
  root["maskMapping"]["car"] = 100;
  root["maskMapping"]["build"] = 50;
  root["maskMapping"]["pole"] = 27;
  root["maskMapping"]["trafficlight"] = 28;
  root["maskMapping"]["trafficsign"] = 20;
  Json::StreamWriterBuilder builder;
  builder["commentStyle"] = "None";
  builder["indentation"] = "";
  const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
  std::stringstream oss;
  writer->write(root, &oss);

  std::fstream of(savePath + "/" + fpath, std::ios::out | std::ios::binary);
  of.write(oss.str().c_str(), oss.str().size());
  of.close();
  return true;
}

TXSIM_MODULE(sim_label)
