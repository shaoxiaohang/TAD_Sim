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

class RapidPointCloudSaver:
    def __init__(self, filename, binary=True, color=True):
        self.filename = filename
        self.binary = binary
        self.color = color
        self.f = open(filename, "wb" if binary else "w")

        header_lines = [
            "ply",
            "format binary_little_endian 1.0",
            "element vertex              ",  # will be filled later
            "property float x",
            "property float y",
            "property float z",
            "property uchar red" if color else None,
            "property uchar green" if color else None,
            "property uchar blue" if color else None,
            "end_header",
        ]
        header = [line for line in header_lines if line is not None]
        header = "\n".join(header) + "\n"
        self.f.write(header.encode('ascii') if binary else header)

        self.num_points = 0
        self.vertex_seek = 0
        self.vertex_seek += len(header_lines[0]) + len(header_lines[1]) + 2
        self.vertex_seek += len("element vertex ")

    def __enter__(self):
        return self

    def add_vertex(self, vertex):
        if len(vertex) <= 0:
            return
        self.num_points += len(vertex)
        vertex.tofile(self.f)

    def add_points_colors(self, points, colors):
        if len(points) <= 0:
            return

        points = points.copy()
        colors = colors.copy()
        print("points", points.shape, points.dtype, points.min(0), points.max(0))
        self.num_points += points.shape[0]
        print("add points", points.shape[0], "total", self.num_points)
        points = points.astype(np.float32)
        colors = colors.astype(np.uint8)
        
        if self.color:
            dtype = np.dtype([("x", np.float32), ("y", np.float32), ("z", np.float32), ("red", np.uint8), ("green", np.uint8), ("blue", np.uint8)])
        else:
            dtype = np.dtype([("x", np.float32), ("y", np.float32), ("z", np.float32)])

        vertices = np.zeros(points.shape[0], dtype=dtype)
        vertices['x'] = points[:, 0]
        vertices['y'] = points[:, 1]
        vertices['z'] = points[:, 2]
        
        if self.color:
            vertices['red'] = colors[:, 2]
            vertices['green'] = colors[:, 1]
            vertices['blue'] = colors[:, 0]

        if self.binary:
            vertices.tofile(self.f)
        else:
            if self.color:
                for vertex in vertices:
                    self.f.write(f"{vertex['x']} {vertex['y']} {vertex['z']} {vertex['red']} {vertex['green']} {vertex['blue']}\n")
            else:
                for vertex in vertices:
                    text = f"{vertex['x']:.2f} {vertex['y']:.2f} {vertex['z']:.2f}\n"
                    self.f.write(text)

    def __exit__(self, exc_type, exc_value, traceback):
        print('set points to ', self.num_points)
        self.f.seek(self.vertex_seek)
        self.f.write(f"{self.num_points}".encode() if self.binary else f"{self.num_points}")
        self.f.close()
        print("saved to", self.filename)


def reproject_img_to_3d(intrinsics, depth_image):
    assert len(depth_image.shape) == 2
    H, W = depth_image.shape
    cx, cy = intrinsics[0, 2], intrinsics[1, 2]
    fx, fy = intrinsics[0, 0], intrinsics[1, 1]
    print(f"fx: {fx}, fy: {fy}, cx: {cx}, cy: {cy}")
    print(f"H: {H}, W: {W}")
    x = np.tile(np.arange(W), (H, 1))
    y = np.tile(np.arange(H)[:, np.newaxis], (1, W))
    print(f"x: {x.shape}, y: {y.shape}")
    x3d = (x - cx) * depth_image / fx
    y3d = (y - cy) * depth_image / fy
    z3d = depth_image
    return np.stack([x3d, y3d, z3d], axis=-1)



# 输入：pred_points (N×3数组)，c2w (4×4矩阵)
def cam2world(pred_points, c2w):
    # 转换为齐次坐标 (N×4)
    homo_points = np.hstack([pred_points, np.ones((len(pred_points),1))])
    
    # 应用变换矩阵
    world_homo = (c2w @ homo_points.T).T
    
    # 返回世界坐标 (N×3)
    return world_homo[:,:3]


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

        #rdf to flu
        pred_points = pred_points[:, [2, 0, 1]] * np.array([1, 1, -1])

        #pred_points = np.dot(c2w[:3, :3], pred_points.T).T + c2w[:3, 3]
        #print(f"c2w[:3, 3]: {c2w[:3, 3]}")
        #pred_points = pred_points.reshape(-1, 3)

        pred_points = cam2world(pred_points, c2w)

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

def create_extrinsic_matrix(loc_x, loc_y, loc_z, rot_x_deg, rot_y_deg, rot_z_deg):
    """Create 4x4 extrinsic matrix from XYZ position and RPY angles in degrees"""
    # Convert degrees to radians for rotation calculations
    rot_x_rad = np.radians(rot_x_deg)
    rot_y_rad = np.radians(rot_y_deg)
    rot_z_rad = np.radians(rot_z_deg)
    
    # Create rotation matrix (using XYZ order)
    rotation = Rotation.from_euler('xyz', [rot_x_rad, rot_y_rad, rot_z_rad]).as_matrix()
    
    # Build 4x4 homogeneous transformation matrix
    extrinsic = np.eye(4)
    extrinsic[:3, :3] = rotation
    extrinsic[:3, 3] = [loc_x, loc_y, loc_z]
    
    return extrinsic


def matrix_to_xyz_rpy(matrix):
    # Extract translation
    x, y, z = matrix[0:3, 3]

    # Extract rotation matrix (3x3)
    rot_matrix = matrix[0:3, 0:3]

    # Convert to Euler angles: roll (X), pitch (Y), yaw (Z)
    r = Rotation.from_matrix(rot_matrix)
    roll, pitch, yaw = r.as_euler('xyz', degrees=True)  # RPY order

    return x, y, z, roll, pitch, yaw



def check_pose_and_extrinsic(work_path, site_name, clip_name, save_path):    
    attribute_file = pjoin(work_path, site_name, clip_name, "attribute.json")

    if os.path.exists(attribute_file):
        with open(attribute_file, 'r') as file:
            attribute = json.load(file)
            print(f"load attribute_file: {attribute_file}")

    posefile_ue = pjoin(work_path, site_name, clip_name, "pose_ue.txt")
    pose_data_ue = np.loadtxt(posefile_ue)

    os.makedirs(save_path,exist_ok=True)

    timestamps = attribute["sync"]["camera_front"][0:50:10]

    prefix = "_ue"


    save_global = True


    ply_files = []
    for timestamp in timestamps:
        #print(f"timestamp: {timestamp}")
        curr_pose_ue = pose_data_ue[0]
        for pose in pose_data_ue:
            match_timestamp = f"{int(pose[0]):010d}"
            #print(f"match_timestamp: {match_timestamp}")
            if match_timestamp == timestamp:
                curr_pose_ue = pose

        #print(f"curr_pose_ue: {curr_pose_ue} timestamp: {timestamp}")
        curr_pose_ue[1:4] = curr_pose_ue[1:4]
        chassis_2_w = quat_to_transform(curr_pose_ue[4:8], curr_pose_ue[1:4])

        print(f"chassis_2_w shape: {chassis_2_w.shape}")

        # fru_2_flu  = np.eye(4)
        fru_2_flu  = np.eye(4)
        fru_2_flu [1, 1] = -1
        chassis_2_w =  fru_2_flu @ chassis_2_w


        for camera_name in ["camera_rear"]:
            img_path = pjoin(work_path, site_name, clip_name, f"{camera_name}", f"{timestamp}.jpg")
            depth_path = pjoin(work_path, site_name, clip_name, f"depth_{camera_name}", f"{timestamp}.png")
            ply_file = f"{save_path}/{clip_name}_{camera_name}_{timestamp}{prefix}.ply"
            img = cv2.imread(img_path)
            depth = cv2.imread(depth_path, cv2.IMREAD_UNCHANGED).astype(np.float32) / 256.0

            K = np.array(attribute["calibration"][camera_name]["K"])

            camera_2_chassis = np.array(attribute["calibration"][f"{camera_name}_2_chassis"])

            print(f"camera_2_chassis: {camera_2_chassis}")

            x, y, z, roll, pitch, yaw = matrix_to_xyz_rpy(camera_2_chassis)
            print(f"camera_2_chassis xyz_rpy: {x}, {y}, {z}, {roll}, {pitch}, {yaw}")
            

            camera_2_w = chassis_2_w @ camera_2_chassis


            if save_global:
                # depth_tool.create_raw_point_cloud(K, chassis_2_w_ue @ lidar_top_2_camera, img, depth, ply_file)  # lidar_top_2_camera is same as camera_2_chassis in origin sim data
                create_raw_point_cloud(K, camera_2_w, img, depth, ply_file)  # lidar_top_2_camera is same as camera_2_chassis in origin sim data
            else:
                create_raw_point_cloud(K, camera_2_chassis, img, depth, ply_file)

            ply_files.append(ply_file)



    merged_ply_files = f"{save_path}/merged{prefix}.ply"
    merge_point_cloud(ply_files, merged_ply_files)


if __name__ == "__main__":
    work_path = "/saturnv/datas"
    site_name = "Site0"
    clip_name = "DZ115_20250415-112501"
    save_path = "/saturnv/datas"
    check_pose_and_extrinsic(work_path, site_name, clip_name, save_path)