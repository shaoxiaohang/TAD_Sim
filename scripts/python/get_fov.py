# import cv2
# import numpy as np

# # 内参矩阵和畸变系数示例
# K = np.array([[550, 0, 960], [0, 550, 768], [0, 0, 1]])
# D = np.zeros(4)  # 假设无畸变（实际需标定获取）


# # 计算FOV
# fov_x, fov_y, _, _, _ = cv2.calibrationMatrixValues(K, (1920, 1536), 0, 0)
# print(f"水平FOV: {fov_x:.2f}°", f"垂直FOV: {fov_y:.2f}°")

# import cv2
# import numpy as np

# K = np.array([
#     [550, 0, 960],
#     [0, 550, 768],
#     [0, 0, 1]
# ], dtype=np.float32)

# fovx, fovy, focalLength, principalPoint, aspectRatio = cv2.calibrationMatrixValues(
#     K, (1920, 1536), 0, 0
# )

# print("水平FOV:", fovx, "度")
# print("垂直FOV:", fovy, "度")
# print("焦距（像素）:", focalLength)
# print("主点坐标（像素）:", principalPoint)
# print("焦距纵横比:", aspectRatio)

import cv2
import numpy as np

# 鱼眼标定参数（需通过实际标定获取）
K = np.array([[550, 0, 960], [0, 550, 768], [0, 0, 1]])  # 等距投影下的内参
D = np.array([0.1, -0.2, 0.001, 0.001])  # 鱼眼畸变系数（示例）

# 计算鱼眼FOV（需手动验证）
fovx = 2 * np.arctan(960 / 2) * 180 / np.pi  # ≈200°

print(fovx)