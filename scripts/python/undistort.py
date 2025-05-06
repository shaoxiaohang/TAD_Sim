import re
import cv2
import numpy as np

import re
class FisheyeCameraConfig:

    def __init__(self, config_text):
        self.config_text = config_text
        self.intrinsic_matrix = None
        self.distortion_params = None
        self.image_size = None
        self._parse_config()
    
    def _parse_config(self):
        """解析配置文件文本"""
        # 改进后的正则表达式，更精确地匹配值部分
        def extract_param_value(key):
            pattern = rf'{key}.*?value:\s*"([^"]+)"'
            match = re.search(pattern, self.config_text, re.DOTALL)
            if not match:
                raise ValueError(f"找不到参数: {key}")
            return match.group(1).strip()
        
        # 获取各参数值
        distortion_str = extract_param_value("Distortion_Parameters")
        intrinsic_str = extract_param_value("Intrinsic_Matrix")
        res_h_str = extract_param_value("Res_Horizontal")
        res_v_str = extract_param_value("Res_Vertical")

        print(f"Distortion Parameters: {distortion_str}")
        print(f"Intrinsic Matrix: {intrinsic_str}")
        print(f"Res Horizontal: {res_v_str}")
        print(f"Res Vertical: {res_h_str}")
        
        # 解析畸变参数 (k1, k2, k3, k4)
        self.distortion_params = np.array(
            [float(x.strip()) for x in distortion_str.split(",") if x.strip()],
            dtype=np.float64
        )
        
        # 解析内参矩阵 (3x3)
        intrinsic_values = [float(x.strip()) for x in intrinsic_str.split(",") if x.strip()]
        if len(intrinsic_values) != 9:
            raise ValueError("内参矩阵需要9个值")
        self.intrinsic_matrix = np.array(intrinsic_values, dtype=np.float64).reshape(3, 3)
        
        # 解析图像分辨率
        self.image_size = (int(res_h_str), int(res_v_str))



def undistort_fisheye_image(image_path, config_path, output_path):
    """对鱼眼图像进行去畸变处理"""
    # 读取配置文件
    with open(config_path, 'r') as f:
        config_text = f.read()
    
    # 解析配置
    config = FisheyeCameraConfig(config_text)
    
    # 读取图像
    image = cv2.imread(image_path)
    if image is None:
        raise ValueError(f"无法读取图像: {image_path}")
    
    # 检查图像尺寸是否匹配配置
    if (image.shape, image.shape) != config.image_size:
        print(f"警告: 图像尺寸 {image.shape[:2]} 与配置尺寸 {config.image_size} 不匹配")
    
    # 估计新的相机矩阵
    new_K = cv2.fisheye.estimateNewCameraMatrixForUndistortRectify(
        config.intrinsic_matrix, 
        config.distortion_params, 
        config.image_size, 
        None, None, None, 
        config.image_size, 
        0.3
    )
    
    # 执行去畸变
    undistorted_img = cv2.fisheye.undistortImage(
        image, 
        config.intrinsic_matrix, 
        config.distortion_params, 
        None, 
        new_K, 
        config.image_size
    )
    
    # 保存结果
    cv2.imwrite(output_path, undistorted_img)
    print(f"去畸变图像已保存到: {output_path}")

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="鱼眼图像去畸变工具")
    parser.add_argument("image_path", help="输入鱼眼图像路径")
    parser.add_argument("config_path", help="相机内参配置文件路径")
    parser.add_argument("output_path", help="输出图像保存路径")
    
    args = parser.parse_args()

    print(f"输入图像: {args.image_path}")
    print(f"配置文件: {args.config_path}")
    print(f"输出图像: {args.output_path}")
    
    undistort_fisheye_image(args.image_path, args.config_path, args.output_path)

