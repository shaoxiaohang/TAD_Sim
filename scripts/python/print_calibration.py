import json
import math
import numpy as np
from scipy.spatial.transform import Rotation

def matrix_to_euler(R):
    """将3x3旋转矩阵转换为欧拉角(Roll, Pitch, Yaw)"""
    # 确保输入是3x3矩阵
    assert R.shape == (3, 3), "输入必须是3x3旋转矩阵"
    
    sy = math.sqrt(R[0,0] * R[0,0] + R[1,0] * R[1,0])
    
    singular = sy < 1e-6
    if not singular:
        roll = math.atan2(R[2,1], R[2,2])  # X轴旋转
        pitch = math.atan2(-R[2,0], sy)    # Y轴旋转
        yaw = math.atan2(R[1,0], R[0,0])   # Z轴旋转
    else:
        roll = math.atan2(-R[1,2], R[1,1])
        pitch = math.atan2(-R[2,0], sy)
        yaw = 0
    
    return np.degrees([roll, pitch, yaw])  # 转换为角度

def print_camera_extrinsics(json_file):
    """读取JSON文件并打印相机外参"""
    with open(json_file) as f:
        data = json.load(f)
    

    camera_types = [
        "camera_front_2_chassis", "camera_front_30fov_2_chassis",
        "camera_front_left_2_chassis", "camera_front_right_2_chassis",
        "camera_rear_left_2_chassis", "camera_rear_right_2_chassis", 
        "camera_rear_2_chassis"
    ]

    for cam in camera_types:
        if cam in data:
            matrix = np.array(data[cam])
            # 提取平移向量（单位：米）
            xyz = matrix[:3, 3]  
            # 计算RPY角（ZYX顺序，单位：度）
            rpy = Rotation.from_matrix(matrix[:3, :3]).as_euler('zyx', degrees=True)
            print(f"\n{cam}:")
            print(f"  Translation (m): [{xyz[0]:.6f}, {xyz[1]:.6f}, {xyz[2]:.6f}]")
            print(f"  Rotation (deg):")
            print(f"    Roll (X): {rpy[0]:.6f}°")
            print(f"    Pitch (Y): {rpy[1]:.6f}°")
            print(f"    Yaw (Z): {rpy[2]:.6f}°")


    # for cam_name, matrix in data.items():
    #     if isinstance(matrix, list) and len(matrix) == 4 and all(len(row) == 4 for row in matrix):
    #         # 转换为numpy数组便于处理
    #         T = np.array(matrix)
            
    #         # 提取平移向量(X,Y,Z) - 矩阵最后一列的前三个元素
    #         x, y, z = T[0:3, 3]
            
    #         # 提取旋转矩阵(3x3)
    #         R = T[0:3, 0:3]
            
    #         # 计算欧拉角(RPY)
    #         roll, pitch, yaw = matrix_to_euler(R)
            
    #         print(f"\n{cam_name}:")
    #         print(f"  Translation (m): [{x:.6f}, {y:.6f}, {z:.6f}]")
    #         print(f"  Rotation (deg):")
    #         print(f"    Roll (X): {roll:.6f}°")
    #         print(f"    Pitch (Y): {pitch:.6f}°")
    #         print(f"    Yaw (Z): {yaw:.6f}°")
    #     else:
    #         print(f"\n警告：'{cam_name}'的矩阵格式无效，期望4x4矩阵")

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='解析相机外参JSON文件')
    parser.add_argument('json_file', help='包含相机外参的JSON文件路径')
    args = parser.parse_args()
    
    print_camera_extrinsics(args.json_file)
