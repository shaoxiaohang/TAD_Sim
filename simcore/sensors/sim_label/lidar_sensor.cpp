/**
 * @file LidarSensor.cpp
 * @author kekesong (kekesong@tencent.com)
 * @brief
 * @version 0.1
 * @date 2024-03-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "lidar_sensor.h"
#include <float.h>
#include <Eigen/Geometry>
#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <iostream>

/// lidar sensor by id
LidarSensor::LidarSensor(int _id) : FovFilterSensor(_id) {}

void LidarSensor::SetVerAngles(const std::vector<double>& angles){
  verAngle = angles;
}

/**
 * @brief initialize lidar sensor
 *
 * @return true on success
 * @return false
 */
bool LidarSensor::Init() {
  if (rayNum < 1) return false;
  if (leftFOV > rightFOV) return false;
  if (verAngle.empty()) {
    verAngle.resize(rayNum);
    verAngle[0] = downFOV;
    for (int i = 1; i < rayNum; ++i) {
      verAngle[i] = downFOV + i * (upFOV - downFOV) / (rayNum - 1);
    }
  } else {
    if (verAngle.size() != rayNum) return false;
    downFOV = DBL_MAX;
    upFOV = -DBL_MAX;
    for (auto a : verAngle) {
      //std::cout << a  << std::endl;
      downFOV = std::min(a, downFOV);
      upFOV = std::max(a, upFOV);
    }
    std::sort(verAngle.begin(), verAngle.end());
  }
  downFOV = std::min(0.0, std::max(-1.5707963267948966192313216916398, downFOV));
  upFOV = std::max(0.0, std::min(1.5707963267948966192313216916398, upFOV));
  // leftFOV = std::min(0.0,std::max(-3.1415926535897932384626433832795,
  // leftFOV)); rightFOV =
  // std::max(0.0,std::min(3.1415926535897932384626433832795, rightFOV));

  verMap.resize(std::ceil((upFOV - downFOV) / resVerial));
  if (verMap.empty()) return false;
  int cur = 0;
  for (size_t i = 0; i < verMap.size(); ++i) {
    double a = downFOV + i * resVerial;
    while (cur < rayNum - 1 && a > verAngle[cur + 1]) cur++;
    verMap[i] = cur;
  }

  return FovFilterSensor::Init();
}

/**
 * @brief convert world coordinate to pixel coordinate
 *
 * @param wp : world coordinate
 * @param uv : pixel coordinate
 * @return true on success
 * @return false on failure
 */
bool LidarSensor::world2pixel(const Eigen::Vector3d &wp, Eigen::Vector2d &uv) const {
  Eigen::Vector3d c = FovVector(wp);
  if (c.norm() >= dRange) {
    return false;
  }
  double h = -std::atan2(c.y(), c.x());
  double v = std::atan2(c.z(), std::sqrt(c.x() * c.x() + c.y() * c.y()));
  if (h >= leftFOV && h <= rightFOV && v >= downFOV && v <= upFOV) {
    uv.x() = (h - leftFOV) / resHorizonal;
    uv.y() = (v - downFOV) / resVerial;
    if (uv.y() >= 0 && uv.y() < verMap.size()) {
      uv.y() = verMap.at(std::floor(uv.y()));
      return true;
    }
  }

  return false;
}

/// @brief all lidars
std::map<int, std::shared_ptr<LidarSensor>> lidars;

/**
 * @brief load lidar data from sensor message
 *
 * @param sensor sensor message
 * @param device device id
 * @return true on success
 * @return false on failure
 */
bool LoadLidar(const sim_msg::Sensor &sensor, const std::string &device) {
  if (!device.empty() && device != sensor.extrinsic().device()) {
    return true;
  }
  const double a2r = EIGEN_PI / 180.;
  auto lid = std::make_shared<LidarSensor>(sensor.extrinsic().id());
  // read lidar extrinsic
  lid->setPosition(Eigen::Vector3d(sensor.extrinsic().locationx() * 0.01, sensor.extrinsic().locationy() * 0.01,
                                   sensor.extrinsic().locationz() * 0.01));
  lid->setRotation(a2r * sensor.extrinsic().rotationx(), a2r * sensor.extrinsic().rotationy(),
                   a2r * sensor.extrinsic().rotationz());
  // parse angle from string
  auto angle_fromstring = [&](const std::string &str) {
    std::vector<double> angle;
    std::stringstream sss(str);
    std::string line;
    while (std::getline(sss, line)) {
      std::string s;
      std::stringstream ss(line);
      std::vector<std::string> data;
      while (std::getline(ss, s, ',')) data.push_back(s);
      if (data.empty()) {
        std::cout << "lidar %d: Error in angle.str.";
        return angle;
      }
      angle.push_back(a2r * std::atof(data[0].c_str()));
    }
    return angle;
  };
  auto angle_fromstring_m1 = [&](const std::string &str) {
    std::vector<double> angle;

    return angle;
  };

  // read frequency
  GetPropValue(sensor.intrinsic().params(), "Frequency", lid->fps);
  std::string sbuf;
  GetPropValue(sensor.intrinsic().params(), "Type", sbuf);
  GetPropValue(sensor.intrinsic().params(), "Model", sbuf);

  std::cout << "lidar type " << sbuf << std::endl;

  // handle on hs128at
  if (sbuf == "HS128AT") {
    std::string AngleDefinition;
    GetPropValue(sensor.intrinsic().params(), "AngleDefinition", AngleDefinition);

    lid->upFOV = a2r * 13;
    lid->downFOV = -a2r * 12.5;
    lid->leftFOV = -a2r * 60;
    lid->rightFOV = a2r * 60;
    lid->rayNum = 128;
    lid->dRange = 200;
    lid->resHorizonal = a2r * 0.01 * lid->fps;
    lid->resVerial = a2r * 0.02;
  } else if (sbuf == "HS128") {
    // handle on hs128
    std::string angle_file = "/home/aaa/workspace/hsim/simcore/sensors/displayue5/XMLFiles/LidarConfig/HS128/angle.csv";
    std::ifstream ifs(angle_file);
    std::vector<double> vertical_angles;
    std::vector<double> horizontal_angles;
    vertical_angles.resize(128);
    horizontal_angles.resize(128);
    if (ifs.good()) {
      uint32_t loop_num = 128;
      std::string str;
      std::getline(ifs, str);
      while (std::getline(ifs, str)) {
        std::stringstream ss(str);
        std::vector<std::string> data;
        while (std::getline(ss, str, ',')) data.push_back(str);
        if (data.size() < 3) {
          std::cout <<  "Error in angle.csv." << std::endl;
          return false;
        }
        uint32_t loopi = std::atoi(data[0].c_str());
        if (loopi < 1 || loopi > loop_num) {
          std::cout <<  "Error in angle.csv." << std::endl;
          return false;
        }
        vertical_angles[loopi - 1] = std::atof(data[1].c_str());
        horizontal_angles[loopi - 1] = std::atof(data[2].c_str());
      }
      lid->SetVerAngles(vertical_angles);
    }
    else{
      std::cout <<  "Failed to read angle.csv" << std::endl;
      return false;
    }

    lid->upFOV = a2r * 14.4;
    lid->downFOV = -a2r * 25.0;
    lid->leftFOV = -a2r * 180;
    lid->rightFOV = a2r * 180;
    lid->rayNum = 128;
    lid->dRange = 200;
    lid->resHorizonal = a2r * 0.02 * lid->fps;
    lid->resVerial = a2r * 0.5;
  } else if (sbuf == "RSM1") {
    // handle on rsm1
    std::string AngleDefinition;
    GetPropValue(sensor.intrinsic().params(), "AngleDefinition", AngleDefinition);

    lid->upFOV = a2r * 12.5;
    lid->downFOV = -a2r * 12.5;
    lid->leftFOV = -a2r * 60;
    lid->rightFOV = a2r * 60;
    lid->rayNum = 128;
    lid->dRange = 200;
    lid->fps = 10;
    lid->resHorizonal = a2r * 0.2;
    lid->resVerial = a2r * 0.02;
  } else if (sbuf == "User") {
    // handle on user

    GetPropValue(sensor.intrinsic().params(), "FovUp", lid->upFOV);
    GetPropValue(sensor.intrinsic().params(), "FovDown", lid->downFOV);
    GetPropValue(sensor.intrinsic().params(), "FovStart", lid->leftFOV);
    GetPropValue(sensor.intrinsic().params(), "FovEnd", lid->rightFOV);
    GetPropValue(sensor.intrinsic().params(), "Radius", lid->dRange);
    GetPropValue(sensor.intrinsic().params(), "HorzionalRes", lid->resHorizonal);
    GetPropValue(sensor.intrinsic().params(), "RayNum", lid->rayNum);

    GetPropValue(sensor.intrinsic().params(), "uUpperFov", lid->upFOV);
    GetPropValue(sensor.intrinsic().params(), "uLowerFov", lid->downFOV);
    GetPropValue(sensor.intrinsic().params(), "FovStart", lid->leftFOV);
    GetPropValue(sensor.intrinsic().params(), "FovEnd", lid->rightFOV);
    GetPropValue(sensor.intrinsic().params(), "uRange", lid->dRange);
    GetPropValue(sensor.intrinsic().params(), "uHorizontalResolution", lid->resHorizonal);
    GetPropValue(sensor.intrinsic().params(), "uChannels", lid->rayNum);
  } else {
    std::cout << "not support lidar type: " << sbuf << std::endl;
    return false;
  }

  // init
  if (lid->Init()) {
    lidars[lid->id] = lid;
    return true;
  }
  return false;
}
