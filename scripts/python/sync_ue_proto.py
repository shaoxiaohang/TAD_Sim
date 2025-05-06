import os
import shutil
import argparse

def sync_folders(folder_a, folder_b):
    """
    用文件夹B中的文件覆盖文件夹A中同名文件
    
    参数:
        folder_a (str): 文件夹A路径
        folder_b (str): 文件夹B路径
    """
    # 获取两个文件夹中的文件列表
    files_a = set(os.listdir(folder_a))
    files_b = set(os.listdir(folder_b))
    
    # 找出两个文件夹中都存在的文件
    common_files = files_a & files_b
    
    # 统计覆盖的文件数量
    count = 0
    
    for file_name in common_files:
        src_path = os.path.join(folder_b, file_name)
        dst_path = os.path.join(folder_a, file_name)
        
        # 检查是否是文件(不是目录)
        if os.path.isfile(src_path):
            shutil.copy2(src_path, dst_path)  # 复制并保留元数据
            count += 1
            print(f"已覆盖: {file_name}")
    
    print(f"\n操作完成! 共覆盖了 {count} 个文件")

def main():
    
    folder_a = "/saturnv/simcore/sensors/displayue5/Source/Display/SimMsg"
    folder_b = "/saturnv/common/message/build"
    
    # 验证路径是否存在
    if not os.path.isdir(folder_a):
        print(f"错误: 文件夹A路径不存在 - {folder_a}")
    elif not os.path.isdir(folder_b):
        print(f"错误: 文件夹B路径不存在 - {folder_b}")
    else:
        sync_folders(folder_a, folder_b)

if __name__ == "__main__":
    main()