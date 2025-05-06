      
import time
import os
import json
import cv2
from os.path import join as pjoin
from scipy.spatial.transform import Rotation
import numpy as np
from plyfile import PlyData

# from file_io import RapidPointCloudSaver
from argparse import ArgumentParser
parser = ArgumentParser()


def reproject_img_to_3d(intrinsics, depth_image):
    assert len(depth_image.shape) == 2
    H, W = depth_image.shape
    cx, cy = intrinsics[0, 2], intrinsics[1, 2]
    fx, fy = intrinsics[0, 0], intrinsics[1, 1]
    x = np.tile(np.arange(W), (H, 1))
    y = np.tile(np.arange(H)[:, np.newaxis], (1, W))
    x3d = (x - cx) * depth_image / fx
    y3d = (y - cy) * depth_image / fy
    z3d = depth_image
    return np.stack([x3d, y3d, z3d], axis=-1)

def create_raw_point_cloud(intrinsics, c2w, image, depth_pred, ply_filename):
    # intrinsics: 4x4
    # image: [H, W, 3]
    # depth_pred: [H, W], in image colors
    # depth_gt: [H, W] (sparse image), in red

    # print(intrinsics, c2w, image.shape, depth_pred.shape)
    with RapidPointCloudSaver(ply_filename) as saver:
        # pred
        pred_points = reproject_img_to_3d(intrinsics, depth_pred)
        pred_colors = image
        pred_colors = pred_colors.reshape(-1, 3)
        pred_points = pred_points.reshape(-1, 3)
        pred_points = np.dot(c2w[:3, :3], pred_points.T).T + c2w[:3, 3]
        pred_points = pred_points.reshape(-1, 3)
        saver.add_points_colors(pred_points, pred_colors)

def merge_point_cloud(ply_files, output_ply):
    with RapidPointCloudSaver(output_ply) as ply_saver:
        for plyfilename in ply_files:
            plydata = PlyData.read(plyfilename)
            vertex = plydata['vertex'].data
            ply_saver.add_vertex(vertex)

def quat_to_transform(q, t):
    rot = Rotation.from_quat(q).as_matrix()
    transform_matrix = np.eye(4)
    transform_matrix[:3, :3] = rot
    transform_matrix[:3, 3] = t
    return transform_matrix

def process_extrinsic(work_path, site_name):
    # convert attribute
    
    # parser.add_argument("--work_path", type=str, required=False, default="/horizon-bucket/saturn_v_dev/01_users/qingfeng.li/simulated_train_dataset/MVS_Sim_V3")
    # parser.add_argument("--site_name", type=str, required=False, default="Site_130_134150_50_73420")

    # args = parser.parse_args()
    import json

    prefix = "_ue"

    clips = os.listdir(pjoin(work_path, site_name))
    for clip_name in clips:
        if not clip_name.startswith("DZ"):
            continue
        print(f"clip_name: {clip_name}")
        attribute_file = pjoin(work_path, site_name, clip_name, "attribute.json")

        if os.path.exists(attribute_file):
            with open(attribute_file, 'r') as file:
                attribute = json.load(file)

        # back origin attribute
        curtime = time.time()
        attribute_file_bk = pjoin(work_path, site_name, clip_name, f"attribute_bk_{curtime}.json")
        with open(attribute_file_bk, 'w') as file:
            json.dump(attribute, file, indent=4, ensure_ascii=False)

        for camera_name in ["camera_front", "camera_front_left", "camera_front_right", "camera_rear", "camera_rear_left", "camera_rear_right"]:
            # ue origin pose
            lidar_top_2_camera_raw = np.array(attribute["calibration"][f"lidar_top_2_{camera_name}{prefix}"])
            lidar_top_2_camera_raw[:3, 3] = lidar_top_2_camera_raw[:3, 3] * 1e-2
            # print(f"lidar_top_2_{camera_name}{prefix}: {lidar_top_2_camera_raw}")

            lidar_top_2_camera = lidar_top_2_camera_raw
            camera_rdf_2_fru = np.zeros((3, 3))
            camera_rdf_2_fru[0, 2] = 1
            camera_rdf_2_fru[1, 0] = 1
            camera_rdf_2_fru[2, 1] = -1
            # determinant = np.linalg.det(camera_rdf_2_fru)
            # print(f"camera_rdf_2_fru determinant: {determinant}")
            lidar_top_2_camera[:3, :3] = lidar_top_2_camera_raw[:3, :3] @ camera_rdf_2_fru
            fru_2_flu = np.eye(4)
            fru_2_flu[1, 1] = -1
            lidar_top_2_camera = fru_2_flu @ lidar_top_2_camera
            # determinant = np.linalg.det(lidar_top_2_camera[:3, :3])
            # print(f"{camera_name}2_chassis determinant: {determinant}")


            print(f"{camera_name}2_chassis: {lidar_top_2_camera}")

            euler_lidar_top_2_camera = Rotation.from_matrix(lidar_top_2_camera[:3, :3]).as_euler("xyz", degrees=True)  # lidar_top_2_camera is same as camera_2_chassis in origin sim data
            # print(f"lidar_top_2_{camera_name}{prefix} euler: {euler_lidar_top_2_camera}, trans: {lidar_top_2_camera[:3, 3]}")
            print(f"lidar_top_2_{camera_name}{prefix}: {[f'{x:.3f}' for x in euler_lidar_top_2_camera]}, trans: {[f'{x:.3f}' for x in lidar_top_2_camera[:3, 3]]}")

            # save to new attribute file
            attribute["calibration"][f"lidar_top_2_{camera_name}"] = np.linalg.inv(lidar_top_2_camera).tolist()

            attribute_file_recalib = pjoin(work_path, site_name, clip_name, "attribute.json")
            with open(attribute_file_recalib, 'w') as file:
                json.dump(attribute, file, indent=4, ensure_ascii=False)

def process_pose(work_path, site_name):
    # convert ue pose flu_2_fru
    clips = os.listdir(pjoin(work_path, site_name))
    
    for clip_name in clips:
        if not clip_name.startswith("DZ"):
            continue
        pose_ue_file = pjoin(work_path, site_name, clip_name, "pose_ue.txt")
        poses_ue = np.loadtxt(pose_ue_file)
        poses_fru = []
        for pose_ue in poses_ue:
            pose_ue[1:4] = pose_ue[1:4] * 1e-2
            chassis_2_w_ue = quat_to_transform(pose_ue[4:8], pose_ue[1:4])
            flu_2_fru = np.eye(4)
            flu_2_fru[1, 1] = -1
            chassis_2_w_ue = flu_2_fru @ chassis_2_w_ue @ flu_2_fru
            # print(f"det: {np.linalg.det(chassis_2_w_ue[:3, :3])}")
            pose_quat = Rotation.from_matrix(chassis_2_w_ue[:3, :3]).as_quat()
            # print(f"pose_quat: {pose_quat}")
            pose_fru = np.concatenate([pose_ue[:1], chassis_2_w_ue[:3, 3], pose_quat])
            poses_fru.append(pose_fru)
        poses_fru_file = pjoin(work_path, site_name, clip_name, "pose_fru.txt")
        #第一位时间戳不保留小数，后面的保留6位小数
        poses_fru = np.array(poses_fru)
        np.savetxt(poses_fru_file, poses_fru, fmt=["%.0f", "%.6f", "%.6f", "%.6f", "%.6f", "%.6f", "%.6f", "%.6f"])
        # np.savetxt(poses_fru_file, poses_fru, fmt="%.6f")
        print(f"poses_fru saved to {poses_fru_file}")


def check_pose_and_extrinsic(work_path, site_name, clip_name, save_path):
    # parser.add_argument("--work_path", type=str, required=False, default="/home/yuansendu/horizon-bucket/saturn_v_dev/01_users/qingfeng.li/simulated_train_dataset/MVS_Test_8.29_1/Temp_Data/")
    # parser.add_argument("--site_name", type=str, required=False, default="Site_130_25630_50_14910")
    # parser.add_argument("--clip_name", type=str, required=False, default="DZ115_20240829_152306")
    # parser.add_argument("--save_path", type=str, required=False, default="/home/yuansendu/data/vision_pipeline/home/users/yuansen.du/data/MetrixCity/Site_130_25750_50_14760_0/ue_pose_merged_0829_1")

    # args = parser.parse_args()
    
    attribute_file = pjoin(work_path, site_name, clip_name, "attribute.json")

    if os.path.exists(attribute_file):
        with open(attribute_file, 'r') as file:
            attribute = json.load(file)

    posefile_ue = pjoin(work_path, site_name, clip_name, "pose_ue.txt")
    pose_data_ue = np.loadtxt(posefile_ue)

    os.makedirs(save_path,exist_ok=True)

    timestamps = attribute["sync"]["camera_front"][1:50:10]

    prefix = "_ue"

    save_global = True

    ply_files = []
    for timestamp in timestamps:
        print(f"timestamp: {timestamp}")
        curr_pose_ue = pose_data_ue[0]
        for pose in pose_data_ue:
            if pose[0] == timestamp:
                curr_pose_ue = pose
        curr_pose_ue[1:4] = curr_pose_ue[1:4] * 1e-2
        chassis_2_w_ue_raw = quat_to_transform(curr_pose_ue[4:8], curr_pose_ue[1:4])

        flu_2_fru = np.eye(4)
        flu_2_fru[1, 1] = -1
        chassis_2_w_ue =  chassis_2_w_ue_raw @ flu_2_fru
        euler_chassis_2_w_ue = Rotation.from_matrix(chassis_2_w_ue[:3, :3]).as_euler("xyz", degrees=True)
        
        # print(f"timestamp: {timestamp} - {str(curr_pose_ue[0])} curr_pose_ue: {curr_pose_ue}")
        # print(f"chassis_2_w_ue: {chassis_2_w_ue}")
        # print(f"chassis_2_w_ue: {[f'{x:.3f}' for x in euler_chassis_2_w_ue]}, trans: {[f'{x:.3f}' for x in chassis_2_w_ue[:3, 3]]}")


        for camera_name in ["camera_front", "camera_front_left", "camera_front_right", "camera_rear", "camera_rear_left", "camera_rear_right"]:
            img_path = pjoin(work_path, site_name, clip_name, f"{camera_name}", f"{timestamp}.jpg")
            depth_path = pjoin(work_path, site_name, clip_name, f"depth_{camera_name}", f"{timestamp}.png")
            ply_file = f"{save_path}/{clip_name}_{camera_name}_{timestamp}{prefix}.ply"
            img = cv2.imread(img_path)
            depth = cv2.imread(depth_path, cv2.IMREAD_UNCHANGED).astype(np.float32) / 256.0

            # ue origin pose
            K = np.array(attribute["calibration"][camera_name]["K"])
            # print(f"{camera_name} K", K)
            # lidar_top_2_camera_update = np.array(attribute["calibration"][f"lidar_top_2_{camera_name}"])
            # camera_2_chassis_update = np.linalg.inv(lidar_top_2_camera_update)
            # print(f"{camera_name}_2_chassis_update: {camera_2_chassis_update}")
            # euler_camera_2_chassis_update = Rotation.from_matrix(camera_2_chassis_update[:3, :3]).as_euler("xyz", degrees=True)
            # print(f"{camera_name}_2_chassis_update: {[f'{x:.3f}' for x in euler_camera_2_chassis_update]}, trans: {[f'{x:.3f}' for x in camera_2_chassis_update[:3, 3]]}")
            
            lidar_top_2_camera_raw = np.array(attribute["calibration"][f"lidar_top_2_{camera_name}{prefix}"])
            lidar_top_2_camera_raw[:3, 3] = lidar_top_2_camera_raw[:3, 3] * 1e-2
            # print(f"lidar_top_2_{camera_name}{prefix}: {lidar_top_2_camera_raw}")

            lidar_top_2_camera = lidar_top_2_camera_raw
            camera_rdf_2_fru = np.zeros((3, 3))
            camera_rdf_2_fru[0, 2] = 1
            camera_rdf_2_fru[1, 0] = 1
            camera_rdf_2_fru[2, 1] = -1
            lidar_top_2_camera[:3, :3] = lidar_top_2_camera_raw[:3, :3] @ camera_rdf_2_fru

            fru_2_flu = np.eye(4)
            fru_2_flu[1, 1] = -1
            lidar_top_2_camera = fru_2_flu @ lidar_top_2_camera
            # print(f"{camera_name}2_chassis: {lidar_top_2_camera}")

            euler_lidar_top_2_camera = Rotation.from_matrix(lidar_top_2_camera[:3, :3]).as_euler("xyz", degrees=True)
            # print(f"lidar_top_2_{camera_name}{prefix} euler: {euler_lidar_top_2_camera}, trans: {lidar_top_2_camera[:3, 3]}")
            # print(f"lidar_top_2_{camera_name}{prefix}: {[f'{x:.3f}' for x in euler_lidar_top_2_camera]}, trans: {[f'{x:.3f}' for x in lidar_top_2_camera[:3, 3]]}")

            if save_global:
                # depth_tool.create_raw_point_cloud(K, chassis_2_w_ue @ lidar_top_2_camera, img, depth, ply_file)  # lidar_top_2_camera is same as camera_2_chassis in origin sim data
                create_raw_point_cloud(K, chassis_2_w_ue_raw @ lidar_top_2_camera_raw, img, depth, ply_file)  # lidar_top_2_camera is same as camera_2_chassis in origin sim data
            else:
                create_raw_point_cloud(K, lidar_top_2_camera, img, depth, ply_file)

            ply_files.append(ply_file)



    # merged_ply_files = f"{save_path}/merged{prefix}.ply"
    # merge_point_cloud(ply_files, merged_ply_files)


if __name__ == "__main__":
    work_path = "/horizon-bucket/saturn_v_dev/01_users/qingfeng.li/simulated_train_dataset/MVS_Rolling"
    site_name = "Site_130_42080_50_21270"
    process_extrinsic(work_path, site_name)
    process_pose(work_path, site_name)
    # clip_name = "DZ115_20241010_191552"
    # save_path = "/home/users/yingfeng.cai/dev/isfm-colmap/data/mvs"
    # check_pose_and_extrinsic(work_path, site_name, clip_name, save_path)

    