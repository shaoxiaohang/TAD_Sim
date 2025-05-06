import argparse
import cv2
import numpy as np
import matplotlib.pyplot as plt

def main():
    parser = argparse.ArgumentParser(description='Interactive EXR Pixel Viewer with OpenCV')
    parser.add_argument('image_path', help='Path to the EXR image file')
    args = parser.parse_args()

    try:
        # 使用OpenCV读取EXR图像（注意：OpenCV默认读取为BGR格式）
        img = cv2.imread(args.image_path, cv2.IMREAD_UNCHANGED)
        if img is None:
            raise ValueError("Failed to read image. Check if the file exists and is a valid EXR.")
    except Exception as e:
        print(f"Error reading EXR file: {e}")
        return

    # 确保图像是三维数组 (height, width, channels)
    if img.ndim == 2:
        img = img[:, :, np.newaxis]
    elif img.ndim == 3:
        pass
    else:
        print("Unsupported image format: must be 2D or 3D array.")
        return
    
    print(img.shape)

    #print image size
    print(img.shape[0], img.shape[1], img.shape[2])
    print(img.dtype)
    print(img.min(), img.max())
    print(img.size)

    # 预处理用于显示的数据（处理NaN/Inf并归一化到0-1）
    img_display = np.nan_to_num(img).astype(np.float32)
    for c in range(img_display.shape[2]):  # 遍历通道数
        channel = img_display[:, :, c]
        min_val, max_val = np.min(channel), np.max(channel)
        if max_val > min_val:
            img_display[:, :, c] = (channel - min_val) / (max_val - min_val)
        else:
            img_display[:, :, c] = 0

    # 应用Gamma校正（可选）
    img_display = img_display ** (1 / 2.2)

    # 准备显示数据（单通道使用灰度图）
    if img_display.shape == 1:
        display_data = img_display[:, :, 0]
    else:
        # OpenCV默认读取为BGR，转换为RGB以正确显示
        display_data = cv2.cvtColor(img_display, cv2.COLOR_BGR2RGB)

    # 创建图像窗口
    fig, ax = plt.subplots()
    if img_display.shape == 1:
        img_plot = ax.imshow(display_data, cmap='gray')
    else:
        img_plot = ax.imshow(display_data)
    plt.title(args.image_path)

    def onclick(event):
        if event.inaxes == ax:
            x = int(round(event.xdata))
            y = int(round(event.ydata))
            if 0 <= x < img.shape and 0 <= y < img.shape:
                pixel = img[y, x]
                # 格式化输出所有通道的值
                values = ', '.join([f"{val:.6f}" for val in pixel])
                print(f"Pixel at ({x}, {y}): [{values}]")

    # 绑定点击事件并显示
    fig.canvas.mpl_connect('button_press_event', onclick)
    plt.show()

if __name__ == '__main__':
    main()