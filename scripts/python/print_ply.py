from plyfile import PlyData
import numpy as np

# Read PLY file
ply_data = PlyData.read('/saturnv/datas/DZ115_20250411-200026_camera_front_0000001700_ue.ply')

# Extract vertices (assuming standard PLY format)
vertices = ply_data['vertex'].data

# Convert to numpy array (if needed)
points = np.array([[v['x'], v['y'], v['z']] for v in vertices])

# Print first 10 points
print("First 10 points:")
for i, point in enumerate(points[:10]):
    print(f"Point {i+1}: {point}")
