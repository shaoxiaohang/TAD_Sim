import os
import cv2
import numpy as np
from tqdm import tqdm  # 进度条（可选）

# 定义语义类别到RGB颜色的映射（根据需求修改）
semantic_colors = {
    0: (128, 64, 128),    # 路面: 灰色
    1: (244, 35, 232),    # 人行道: 粉色
    2: (107, 142, 35),    # 植被: 绿色
    3: (152, 251, 152),   # 地形: 浅绿色
    4: (153, 153, 153),   # 杆子: 灰色
    5: (220, 220, 0),     # 交通标志牌: 黄色
    6: (250, 170, 30),    # 交通灯: 橙色
    7: (70, 70, 70),      # 标志线: 深灰色
    8: (255, 255, 255),   # 车道线: 白色
    9: (220, 20, 60),     # 人: 红色
    10: (255, 0, 0),      # 骑行者: 亮红色
    11: (0, 0, 142),      # 自行车: 深蓝色
    12: (0, 0, 230),      # 摩托车: 蓝色
    13: (119, 11, 32),    # 三轮车: 棕色
    14: (0, 0, 255),      # 小汽车: 蓝色
    15: (0, 60, 100),     # 卡车: 深蓝绿色
    16: (0, 80, 100),     # 公交车: 蓝绿色
    17: (0, 0, 70),       # 火车: 深蓝色
    18: (70, 70, 70),     # 建筑物: 深灰色
    19: (190, 153, 153),  # 围栏: 浅棕色
    20: (70, 130, 180),   # 天空: 天蓝色
    21: (255, 165, 0),    # 路锥: 橙色
    22: (255, 215, 0),    # 防护柱: 金色
    23: (0, 191, 255),    # 指路牌: 亮蓝色
    24: (255, 255, 0),    # 斑马线: 黄色
    25: (255, 0, 255),    # 交通箭头: 紫色
    26: (0, 255, 255),    # 导流线: 青色
    27: (255, 0, 0),      # 停止线: 红色
    28: (255, 69, 0),     # 三角路标: 橙红色
    29: (255, 99, 71),    # 限速路标: 番茄色
    30: (218, 112, 214),  # 菱形路标: 紫色
    31: (50, 205, 50),    # 自行车路标: 亮绿色
    32: (139, 0, 0),      # 减速带: 深红色
    33: (178, 34, 34),    # 禁止通行路牌: 砖红色
    34: (210, 105, 30),   # 停车杆: 巧克力色
    35: (139, 69, 19),    # 停车锁: 棕色
    36: (255, 228, 196),  # 可跨越障碍物: 米色
    37: (205, 133, 63),   # 不可跨越障碍物: 黄褐色
    38: (0, 0, 0),        # 掩膜: 黑色
    39: (0, 0, 0),  # 其他: 灰色
    254: (105, 105, 105),  # 机械车位架子: 深灰色（类似金属色）
    255: (169, 169, 169)   # 机械车位: 浅灰色（区分于普通车位）
}

def semantic_to_rgb(semantic_img):
    """将单通道语义图转换为RGB彩色图"""
    h, w = semantic_img.shape
    rgb_img = np.zeros((h, w, 3), dtype=np.uint8)
    for class_id, color in semantic_colors.items():
        rgb_img[semantic_img == class_id] = color
    return rgb_img

def process_folder(input_dir, output_dir):
    """处理整个文件夹的语义图"""
    os.makedirs(output_dir, exist_ok=True)
    img_files = [f for f in os.listdir(input_dir) if f.endswith(('.png', '.jpg', '.tif', '.tiff'))]
    
    for img_file in tqdm(img_files, desc="Processing"):
        # 读取语义图（确保是单通道uint8）
        semantic_path = os.path.join(input_dir, img_file)
        semantic_img = cv2.imread(semantic_path, cv2.IMREAD_GRAYSCALE)
        
        # 转换为RGB
        rgb_img = semantic_to_rgb(semantic_img)
        
        # 保存RGB图
        output_path = os.path.join(output_dir, img_file)
        cv2.imwrite(output_path, rgb_img)

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--input_dir", type=str, required=True, help="输入语义图文件夹路径")
    parser.add_argument("--output_dir", type=str, required=True, help="输出RGB图文件夹路径")
    args = parser.parse_args()
    
    process_folder(args.input_dir, args.output_dir)
    print(f"转换完成！RGB图已保存到 {args.output_dir}")