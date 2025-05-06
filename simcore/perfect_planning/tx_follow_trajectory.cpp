#include "tx_follow_trajectory.h"
#include <boost/algorithm/string/join.hpp>
#include <chrono>
#include "common/coord_trans.h"
#include "sensor_meta.pb.h"
#include "tx_time_utils.h"

TX_NAMESPACE_OPEN(TrafficFlow)

void TAD_FollowTrajectory::Init(tx_sim::InitHelper& helper) {
  std::string trajectory_path = helper.GetParameter("Trajectory_Path");
  LOG(INFO) << "[Trajectory_Path] " << trajectory_path;

  if (!helper.GetParameter("FramesToSkip").empty())
    first_frames_to_skip_ = std::atoi(helper.GetParameter("FramesToSkip").c_str());
  LOG(INFO) << "first_frames_to_skip_ " << first_frames_to_skip_;

  std::ifstream ifs(trajectory_path);
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(ifs, root)) {
    const std::string errorMsg = reader.getFormattedErrorMessages();
    LOG(ERROR) << "Failed to parse trajectory file: " << trajectory_path << "\nError details: " << errorMsg;
  }

  site_.name = root["site_name"].asString();
  std::string scene_type = root["Scene type"].asString();
  std::string lighting = root["Lighting brightness"].asString();
  std::string groud_material = root["Ground material"].asString();
  std::string mechanical = std::to_string(root["Mechanical"].asInt());
  std::string tags = scene_type + "," + lighting + "," + groud_material + "," + mechanical;

  site_.tags = tags;

  int total_frames = 0;

  auto clips = root["clips"];
  site_.num_clips = clips.size();
  for (int i = 0; i < clips.size(); i++) {
    auto clip_json = clips[i];
    Clip clip;
    clip.id = clip_json["clip_id"].asInt();
    auto frames = clip_json["transforms"];
    // remove the first frame
    int num_frames = frames.size();
    total_frames += num_frames;
    clip.num_frames = num_frames;
    for (int j = 0; j < frames.size(); j++) {
      auto frame = ConvertFrame(j, frames[j]);
      if (i == 0 && j == 0) {
        for (int k = 0; k < first_frames_to_skip_; k++) {
          // add identical frames at the beginning to avoid the camera scene capture artifacts
          clip.frames.push_back(frame);
        }
      }
      clip.frames.push_back(frame);
    }
    site_.clips.push_back(clip);
  }
  LOG(INFO) << "total_frames " << total_frames;
  LOG(INFO) << "site_name " << site_.name << " num_clips " << site_.num_clips;
  helper.Publish(location_topic_);
  helper.Publish(highlight_ego_topic_);
  helper.Publish(sensor_meta_topic_);
}

void TAD_FollowTrajectory::Reset(tx_sim::ResetHelper& helper) {
  cur_frame_ = 0;
  cur_clip_ = 0;
  frames_so_far_ = 0;
  clip_frame_offset_ms_ = 0;
}

std::string TAD_FollowTrajectory::getFormattedTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto now_c = std::chrono::system_clock::to_time_t(now);
  std::tm tm = *std::localtime(&now_c);

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y%m%d-%H%M%S");
  return oss.str();
}

void TAD_FollowTrajectory::Step(tx_sim::StepHelper& helper) {
  auto clip = site_.clips[cur_clip_];
  auto clip_id = clip.id;
  auto clip_frames = clip.frames;
  auto clip_num_frames = clip_frames.size();
  if (cur_frame_ >= clip_num_frames) {
    LOG(INFO) << " clip " << clip_id << " end";
    clip_frame_offset_ms_ += clip.num_frames * 100;
    cur_clip_++;
    if (cur_clip_ < site_.num_clips) {
      cur_frame_ = 0;
      clip = site_.clips[cur_clip_];
      clip_id = clip.id;
      clip_frames = clip.frames;
      clip_num_frames = clip_frames.size();
    } else {
      LOG(INFO) << " traj end";
      helper.StopScenario("reaching maximum trajectory clips");
      return;
    }
  }

  LOG(INFO) << " clip " << clip_id << " frame " << cur_frame_ << " num frames " << clip_num_frames;

  auto frame = clip_frames[cur_frame_];

  PubHighlightEgo(helper);
  PubCGLocation(helper, frame);
  PubSensorMeta(helper, frame);
  cur_frame_++;
  frames_so_far_++;
}

void TAD_FollowTrajectory::Stop(tx_sim::StopHelper& helper) {}

TAD_FollowTrajectory::Frame TAD_FollowTrajectory::ConvertFrame(int index, const Json::Value& frame) {
  Frame f;
  f.id = index;
  Eigen::Vector3d enu(frame["tx"].asDouble(), frame["ty"].asDouble(), frame["tz"].asDouble());
  enu /= 100.0;
  f.ue_position = enu;
  coord_trans_api::enu2lonlat(enu.x(), enu.y(), enu.z(), 0.0, 0.0, 0.0);
  Eigen::Quaterniond q1(frame["ow"].asDouble(), frame["ox"].asDouble(), frame["oy"].asDouble(), frame["oz"].asDouble());
  Eigen::Matrix3d m = q1.toRotationMatrix();
  Eigen::Vector3d rpy = m.eulerAngles(0, 1, 2);
  f.position = enu;
  f.rpy = rpy;
  f.quat = q1;
  return f;
}

void TAD_FollowTrajectory::PubCGLocation(tx_sim::StepHelper& helper, const Frame& frame) {
  auto time_stamp = static_cast<std::int64_t>(helper.timestamp());

  // Eigen::Vector3d enu(frame["tx"].asDouble(), frame["ty"].asDouble(), frame["tz"].asDouble());
  // enu /= 100.0;
  // coord_trans_api::enu2lonlat(enu.x(), enu.y(), enu.z(), 0.0, 0.0, 0.0);

  // Eigen::Quaterniond q1(frame["ow"].asDouble(), frame["ox"].asDouble(), frame["oy"].asDouble(),
  // frame["oz"].asDouble()); Eigen::Matrix3d m = q1.toRotationMatrix(); Eigen::Vector3d rpy = m.eulerAngles(0, 1, 2);

  sim_msg::Location location;
  location.set_t(time_stamp * 1e-3);

  auto position = location.mutable_position();
  position->set_x(frame.position.x());
  position->set_y(frame.position.y());
  position->set_z(frame.position.z());
  // auto rpy_loc = location.mutable_rpy();
  // rpy_loc->set_x(frame.rpy.x());
  // rpy_loc->set_y(frame.rpy.y());
  // rpy_loc->set_z(frame.rpy.z());
  auto quat = location.mutable_quat();
  quat->set_x(frame.quat.x());
  quat->set_y(frame.quat.y());
  quat->set_z(frame.quat.z());
  quat->set_w(frame.quat.w());
  location.set_use_quat(true);
  std::string loc_payload;
  if (location.SerializeToString(&loc_payload) && loc_payload.size()) {
    helper.PublishMessage(location_topic_, loc_payload);
  }
}

void TAD_FollowTrajectory::PubHighlightEgo(tx_sim::StepHelper& helper) {
  sim_msg::HighlightGroup highlight_group;
  highlight_group.set_groupname("Ego_001");

  std::string payload;
  if (highlight_group.SerializeToString(&payload) && payload.size()) {
    helper.PublishMessage(highlight_ego_topic_, payload);
  }
}

void TAD_FollowTrajectory::PubSensorMeta(tx_sim::StepHelper& helper, const Frame& frame) {
  sim_msg::SensorMeta sensor_meta;
  auto site = sensor_meta.mutable_site();
  auto clip = sensor_meta.mutable_clip();
  sensor_meta.set_cur_clip(cur_clip_);
  sensor_meta.set_cur_frame(cur_frame_);
  sensor_meta.set_timestamp(helper.timestamp());

  auto cur_clip = site_.clips[cur_clip_];

  site->set_name(site_.name);
  site->set_num_clips(site_.num_clips);
  site->set_start_timestamp(cur_clip.frames[0].id);

  clip->set_id(cur_clip.id);
  clip->set_num_frames(cur_clip.num_frames);
  int start_timestamp = clip_frame_offset_ms_ + first_frames_to_skip_ * 100;
  clip->set_start_timestamp(start_timestamp);
  clip->set_end_timestamp(start_timestamp + (cur_clip.num_frames - 1) * 100);
  clip->set_plate(plate_);
  clip->set_status("init");
  clip->set_tags(site_.tags);
  clip->set_mapping_type("reconstruct");
  std::string record_time = plate_ + "_" + getFormattedTimestamp();
  clip->set_record_time(record_time);

  auto pose = sensor_meta.mutable_pose();

  auto frame_flu = FRU2FLU(frame);

  pose->set_x(frame_flu.ue_position.x());
  pose->set_y(frame_flu.ue_position.y());
  pose->set_z(frame_flu.ue_position.z());
  pose->set_qx(frame_flu.quat.x());
  pose->set_qy(frame_flu.quat.y());
  pose->set_qz(frame_flu.quat.z());
  pose->set_qw(frame_flu.quat.w());

  std::string payload;
  if (sensor_meta.SerializeToString(&payload) && payload.size()) {
    helper.PublishMessage(sensor_meta_topic_, payload);
  }
}

TAD_FollowTrajectory::Frame TAD_FollowTrajectory::FRU2FLU(const Frame& frame) {
  return frame;
  Frame frame_flu = frame;
  frame_flu.ue_position.y() *= -1;
  Eigen::Quaterniond mirror_y(0, 0, 1, 0);  // Pure Y-mirror quaternion
  frame_flu.quat = frame_flu.quat * mirror_y;
  return frame_flu;
}

TX_NAMESPACE_CLOSE(TrafficFlow)

TXSIM_MODULE(TrafficFlow::TAD_FollowTrajectory)