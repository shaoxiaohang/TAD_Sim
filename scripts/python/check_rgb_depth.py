import os
import cv2
import re
import numpy as np
import argparse

def extract_basename(filename):
    """提取文件名基础部分（忽略后缀）"""
    match = re.match(r"^(.+?)(\.jpg|\.png)?$", filename)
    return match.group(1) if match else None

def find_matching_pairs(rgb_dir, depth_dir):
    """匹配两个文件夹中基础名相同的.jpg/.png文件"""
    # 获取符合后缀要求的文件列表
    rgb_files = [f for f in os.listdir(rgb_dir) 
                if f.lower().endswith(('.jpg', '.png'))]
    depth_files = [f for f in os.listdir(depth_dir) 
                  if f.lower().endswith(('.jpg', '.png'))]

    # 构建基础名到全名的映射
    rgb_map = {extract_basename(f): f for f in rgb_files}
    depth_map = {extract_basename(f): f for f in depth_files}

    print(f"RGB文件数: {len(rgb_map)}, 深度图文件数: {len(depth_map)}")
    print(f"RGB文件示例: {list(rgb_map.values())[:5]}")
    print(f"深度图文件示例: {list(depth_map.values())[:5]}")

    # 交集匹配
    common_basenames = set(rgb_map) & set(depth_map)
    
    return [
        (
            os.path.join(rgb_dir, rgb_map[base]),
            os.path.join(depth_dir, depth_map[base])
        )
        for base in common_basenames
    ]

def calculate_overlap(rgb_path, depth_path, save_edges=False, output_dir=None):
    """计算边缘重合度"""
    # 读取图像
    rgb = cv2.imread(rgb_path, cv2.IMREAD_GRAYSCALE)
    depth = cv2.imread(depth_path, cv2.IMREAD_ANYDEPTH)
    
    if rgb is None or depth is None:
        print(f"无法读取图像: {rgb_path} 或 {depth_path}")
        return -1
    
    # 归一化深度图到0-255范围
    depth_normalized = cv2.normalize(depth, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    
    # Canny边缘检测
    rgb_edges = cv2.Canny(rgb, 100, 200)
    depth_edges = cv2.Canny(depth_normalized, 100, 200)
    
    # 保存中间结果
    if save_edges and output_dir:
        base = os.path.splitext(os.path.basename(rgb_path))
        cv2.imwrite(f"{output_dir}/{base}_rgb_edge.png", rgb_edges)
        cv2.imwrite(f"{output_dir}/{base}_depth_edge.png", depth_edges)
    
    # 计算边缘重合度
    intersection = np.sum(cv2.bitwise_and(rgb_edges, depth_edges))
    union = np.sum(cv2.bitwise_or(rgb_edges, depth_edges))
    return intersection / union if union != 0 else 0.0

def main(rgb_dir, depth_dir, threshold, save_edges, output_dir):
    """主处理流程"""
    matched_pairs = find_matching_pairs(rgb_dir, depth_dir)

    print(f"找到 {len(matched_pairs)} 对匹配的图像")
    
    # 处理匹配结果
    total = len(matched_pairs)
    mismatched = []
    overlap_scores = []
    
    for rgb_path, depth_path in matched_pairs:
        overlap = calculate_overlap(rgb_path, depth_path, save_edges, output_dir)
        if overlap < threshold:
            mismatched.append(f"{os.path.basename(rgb_path)} - {os.path.basename(depth_path)}")
        overlap_scores.append(overlap)
    
    # 生成报告
    avg_overlap = np.mean(overlap_scores) if overlap_scores else 0
    print(f"\n=== 匹配报告 ===")
    print(f"扫描文件数: RGB({len(os.listdir(rgb_dir))}), Depth({len(os.listdir(depth_dir))})")
    print(f"有效匹配对: {total}")
    print(f"平均边缘重合度: {avg_overlap:.2%}")
    print(f"不匹配图像对 (阈值={threshold:.0%}): {len(mismatched)}")
    print("\n不匹配列表:")
    print("\n".join(mismatched) if mismatched else "无异常")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="跨格式RGB-深度图同步检测工具")
    parser.add_argument("--rgb_dir", required=True, help="RGB图像文件夹路径（支持.jpg/.png）")
    parser.add_argument("--depth_dir", required=True, help="深度图文件夹路径（支持.jpg/.png）")
    parser.add_argument("--threshold", type=float, default=0.3, 
                       help="判定不匹配的重合度阈值 (默认: 0.3)")
    parser.add_argument("--save_edges", action="store_true",
                       help="保存Canny边缘检测中间结果")
    parser.add_argument("--output_dir", default="./edges",
                       help="中间结果保存路径 (默认: ./edges)")
    
    args = parser.parse_args()
    
    if args.save_edges and not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)
    
    main(args.rgb_dir, args.depth_dir, 
        args.threshold, args.save_edges, args.output_dir)