import collections
import json
import logging
import os
from collections import OrderedDict
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
from os.path import basename, dirname, exists, join
import pickle
import cv2
import numpy as np
from horizon_driving_dataset import DatasetReader, PoseTransformer
from scipy.spatial.transform import Rotation
from sklearn.neighbors import NearestNeighbors
from tqdm import tqdm
from hde_colmap.sfm_config import SfMConfig
from hde_colmap.sfm_utils.colmap_pose_tool import interpolate
from hde_colmap.sfm_utils.file import check_a_file, exec_command, get_keyframe
from hde_colmap.sfm_utils.io import load_json, save_json
from hde_colmap.sfm_utils.logger import timer

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)
logging.basicConfig(level=logging.INFO, format="%(asctime)-15s %(message)s")

import io

def undistort(
    image,
    intrinsics,
    d,
    new_cam_matrix,
    camera_name=None,
    new_cam_wh=None,
    interpolation=cv2.INTER_LINEAR,
    scale=None
):
    # TODO: init Map first to run faster
    if scale is not None:
        cv_shape = (int(image.shape[1] * scale), int(image.shape[0] * scale))
    elif new_cam_wh is not None:
        cv_shape = (new_cam_wh[0], new_cam_wh[1])
    else:
        raise ValueError("scale or new_cam_wh must be set")
    
    if "fisheye" in camera_name:
        new_K = cv2.fisheye.estimateNewCameraMatrixForUndistortRectify(intrinsics, d, cv_shape , None, None , None , cv_shape, 0.3)
        undistort_image = cv2.fisheye.undistortImage(image, intrinsics, d , None , new_K, cv_shape)
    else:
        map1, map2 = cv2.initUndistortRectifyMap(
            intrinsics, d, np.eye(3), new_cam_matrix, cv_shape, cv2.CV_32FC1
        )
        undistort_image = cv2.remap(
            image, map1, map2, interpolation=interpolation)  

    return undistort_image


def check_filelist_exist(filelist, check_prelabel=True):

    seg_filelist = [a.replace("/Raw_data/","/4DLabel_output/Prelabel/").replace(
        "/camera_", "/seg_camera_").replace("/fisheye_", "/seg_fisheye_").replace(".jpg", ".png") for a in filelist]
    
    logger.info("begin_worker_pool check_filelist_exist")
    worker_pool = ThreadPoolExecutor(max_workers=8)
    exist_list = worker_pool.map(
                        check_a_file,
                        filelist)
    logger.info("end_worker_pool")
    worker_pool.shutdown()
    if check_prelabel:
        logger.info("begin_worker_pool check_seg_filelist_exist")
        worker_pool = ThreadPoolExecutor(max_workers=8)
        seg_exist_list = worker_pool.map(
                            check_a_file,
                            seg_filelist)
        logger.info("end_worker_pool")
        worker_pool.shutdown()
        return [a and b for a, b in zip(exist_list, seg_exist_list)]
    else:
        return [a for a in exist_list]


def replace_timestamps(
    source_camera_paths, source_timestamps, target_timestamps
):
    assert len(source_camera_paths) == len(target_timestamps) and len(
        source_timestamps
    ) == len(
        target_timestamps
    ), "timestamps must have same length with camera_paths"
    target_camera_paths = []
    for i, camera_path in enumerate(source_camera_paths):
        target_camera_path = camera_path.replace(
            str(source_timestamps[i]), str(target_timestamps[i])
        )
        target_camera_paths.append(target_camera_path)
    return target_camera_paths


def axis_z(pos1, pos2):
    z_err = abs(pos1[2] - pos2[2])
    return z_err


def cal_angle_error(
    angle1, angle2, pos1, pos2
):  # angle:tum--c2w四元数，qx qy qz qw
    qx1 = angle1[0]
    qy1 = angle1[1]
    qz1 = angle1[2]
    qw1 = angle1[3]
    optical_axis = np.array([0, 0, 1])
    r1 = Rotation.from_quat([qx1, qy1, qz1, qw1])
    rotation1 = r1.as_matrix()
    o1o = np.dot(rotation1, optical_axis.T)
    # o1o=np.array([rotation1[0][2],rotation1[1][2],rotation1[2][2]])

    qx2 = angle2[0]
    qy2 = angle2[1]
    qz2 = angle2[2]
    qw2 = angle2[3]

    r2 = Rotation.from_quat([qx2, qy2, qz2, qw2])
    rotation2 = r2.as_matrix()
    o2o = np.dot(rotation2, optical_axis.T)
    # o2o=np.array([rotation2[0][2],rotation2[1][2],rotation2[2][2]])

    cos_theta = np.dot(o1o, o2o) / (
        np.linalg.norm(o1o) * np.linalg.norm(o2o)
    )
    if cos_theta > 1:
        # logger.info(cos_theta)
        cos_theta = 1
    if cos_theta < -1:
        # logger.info(cos_theta)
        cos_theta = -1
    theta = np.arccos(cos_theta)
    err1 = theta * 180 / np.pi

    # err1表示AB两张图的夹角，err2表示AB向量和A图方向的夹角
    vector_ab = np.array(pos2) - np.array(pos1)  # B-A

    cos_theta = np.dot(vector_ab, o1o) / (
        np.linalg.norm(o1o) * np.linalg.norm(vector_ab)
    )
    if cos_theta > 1:
        cos_theta = 1
    if cos_theta < -1:
        cos_theta = -1
    theta = np.arccos(cos_theta)
    err2 = theta * 180 / np.pi

    # err3表示BA向量和B图方向的夹角
    vector_ba = -vector_ab  # A-B
    cos_theta = np.dot(vector_ba, o2o) / (
        np.linalg.norm(o2o) * np.linalg.norm(vector_ba)
    )
    if cos_theta > 1:
        cos_theta = 1
    if cos_theta < -1:
        cos_theta = -1
    theta = np.arccos(cos_theta)
    err3 = theta * 180 / np.pi

    return err1, err2, err3

import sys
import pickle
import numpy as np

class RestrictedUnpickler(pickle.Unpickler):
    def find_class(self, module, name):
        # 仅允许特定模块和类的反序列化
        allowed_modules_and_classes = {
            "collections": {"OrderedDict", "defaultdict"},
            "builtins": {"list", "dict", "tuple", "set", "frozenset", "str", "int", "float", "bool", "NoneType"},
            "numpy.core.multiarray": {"_reconstruct"},
            "numpy": {"ndarray", "dtype"}  # 添加对 numpy.dtype 的支持
        }
        
        if module in allowed_modules_and_classes and name in allowed_modules_and_classes[module]:
            return getattr(sys.modules[module], name)
        # 默认情况下，禁止所有其他类的反序列化
        raise pickle.UnpicklingError(f"Restricted unpickler: ({module}, {name}) is not allowed")
    
class KeyframeSelection(SfMConfig):
    def __init__(self, configs):
        super(KeyframeSelection, self).__init__(configs)
        self.front_keyframe_yaw = self.configs["keyframe_yaw_camera_front"]
        self.front_keyframe_meter = self.configs["keyframe_meter_camera_front"]
        self.t_max_diff = self.configs["keyframe_t_max_diff"]
        self.clip_names = [basename(clip_dir) for clip_dir in self.clip_dirs]

        self.num_key_frames_th = self.configs.get("num_key_frames_th", 20)
        self.parallel_worker_type = self.configs.get(
            "parallel_worker_type", "thread")
        self.max_workers = 8
        # keyframe num less than num_key_frames_th
        self.dump_clip_list = []
        self.thr_dis = self.configs.get("thr_dis", 10)
        self.max_image_pair = self.configs.get("max_image_pair", 2000)
        self.sync = self.configs.get("sync", True)

        self.mask_dynamic = self.configs.get("mask_dynamic", False)
        self.mask_ego = self.configs.get("mask_ego", True)
        self.mask_vegetation = self.configs.get("mask_vegetation", False)

        self.input_mask_path = {}
        for camera_name in self.camera_names:
            self.input_mask_path[camera_name] =\
                self.configs["input_mask_path"].get(camera_name, "")
        self.pairs_mode = self.configs.get("pairs_mode", "sp+sp")
        self.fisheye_keyframe_step = self.configs.get("fisheye_keyframe_step", 1)

    def get_params(self, input_clip_dir, camera_name, scale, output_param_dir):
        dr = DatasetReader(str(input_clip_dir))
        intrinsic = dr.get_intrinsics(camera_name)
        cam_matrix = intrinsic["K"]
        dist_coeffs = intrinsic["d"]
        width, height = intrinsic["width"], intrinsic["height"]
        logger.info("camera matrix: ")
        logger.info(cam_matrix)
        logger.info("dist coeffs: ")
        logger.info(dist_coeffs)
        new_cam_matrix = cam_matrix.copy()
        new_cam_matrix[:2, :] *= scale
        
        if "fisheye" in camera_name:
            cv_shape = (int(width * scale), int(height * scale))
            new_cam_matrix = cv2.fisheye.estimateNewCameraMatrixForUndistortRectify(cam_matrix, dist_coeffs, cv_shape , None, None , None , cv_shape, 0.3)
            logger.info(f"new_cam_matrix: {new_cam_matrix}")
            
        camera_matrix_file_dir = str(
            join(output_param_dir, camera_name + ".params")
        )
        logger.info(f"Writing {camera_matrix_file_dir}")
        with open(camera_matrix_file_dir, "w") as f:
            f.write(
                str(new_cam_matrix[0, 0])
                + " "
                + str(new_cam_matrix[0, 2])
                + " "
                + str(new_cam_matrix[1, 2])
                + " "
                + str(width)
                + " "
                + str(height)
            )
        return cam_matrix, dist_coeffs, new_cam_matrix

    def get_keyframe_list(self):
        """
            get keyframe list
        """
        keyframe_list = []
        for input_clip_dir in self.clip_dirs:
            clip_name = basename(input_clip_dir)
            for camera_name in self.camera_names:
                keyframe_list = []
                keyframe = join(
                    self.workspace_site_dir,
                    clip_name, camera_name,  "keyframe"
                )
                image_list = get_keyframe(keyframe)
                image_list.sort()
                for image_name in image_list:
                    keyframe_list.append(
                        join(
                            clip_name, camera_name, "keyframe", image_name)
                    )

                image_list_path = join(
                    self.workspace_site_dir,
                    clip_name,
                    camera_name,
                    camera_name + "_list.txt",
                )

                with open(image_list_path, "w") as f:
                    for i in range(len(keyframe_list)):
                        f.write(str(keyframe_list[i]) + "\n")
        return keyframe_list

    def pose_graph2camera(self):
        """
        transform pose graph to camera pose
        """
        # use first clip start as offset reference
        for input_clip_dir in self.clip_dirs:
            for camera_name in self.camera_names:
                self.transform_camera2gnss(
                    clip_dir=input_clip_dir,
                    output_site_dir=self.workspace_site_dir,
                    camera_name=camera_name)

    def transform_camera2gnss(
        self,
        clip_dir,
        output_site_dir,
        camera_name,
    ):
        """
        transform odo_init from vcs to camera
        """
        clip_name = basename(clip_dir)

        if self.init_odo_type.startswith("_"):
            wigo_path = join(
                self.workspace_site_dir, clip_name, "odometry",
                self.init_odo_type[1:] + ".txt")
        else:
            wigo_path = join(
                self.workspace_site_dir, clip_name, "odometry",
                self.init_odo_type + ".txt")

        if not exists(wigo_path):
            logger.error(
                f"odometry file {wigo_path} not exist, "
                f"please check your input data"
            )
            return

        # transform gnss pose to camera_front
        dr = DatasetReader()
        dr.read_pack(clip_dir)

        # transform odo to camera
        wigo = np.loadtxt(wigo_path)
        pt = PoseTransformer()
        pt.loadarray(wigo)

        camera2chassis = dr.get_extrinsic(camera_name, "chassis", jump_sensor='chassis')
        pt.right_rotate(camera2chassis)
        wigo_transformed = pt.dumparray()

        # output
        wigo_transformed_path = join(
            output_site_dir,
            clip_name,
            "camera_odo_init",
            f"{camera_name}_all{self.init_odo_type}.txt"
        )
        os.makedirs(dirname(wigo_transformed_path), exist_ok=True)
        np.savetxt(wigo_transformed_path, wigo_transformed)

        # interpolate
        # get keyframe pose from pose_graph odo
        image_path = join(
            self.workspace_site_dir, clip_name, camera_name, "keyframe"
        )
        image_names = get_keyframe(image_path)
        image_names.sort()
        ts_ind = np.argsort(wigo_transformed[:, 0])
        wigo_transformed = wigo_transformed[ts_ind]
        lines = interpolate(image_names, wigo_transformed)

        # Write camera_pose from pose_graph
        camera_pose_path = join(
            output_site_dir,
            clip_name,
            camera_name,
            f"{camera_name}{self.init_odo_type}.txt"
        )
        os.makedirs(dirname(camera_pose_path), exist_ok=True)

        with open(camera_pose_path, "w") as f:
            f.writelines(lines)

    def del_dump_clip(self):
        """
            delete clip with less keyframe
        """
        for clip_name in self.dump_clip_list:
            if clip_name in self.clip_names:
                self.clip_names.remove(clip_name)
                self.clip_dirs.remove(join(self.site_raw_data_dir, clip_name))
                logger.info(
                    f"Keyframes < {self.num_key_frames_th}, del {clip_name}"
                )
                exec_command(
                    f"rm -rf {join(self.workspace_site_dir, clip_name)}"
                )
        
    def spatial_pairs(self):
        """
            spatial pairs
        """
        logger.info(
            "#### Start Running spatial_pairs ####"
        )

        match_pairs_path = join(self.workspace_site_dir, "match_pairs.txt")
        thr_dis = self.thr_dis
        self.create_pairs(thr_dis, match_pairs_path)
        self.check_merge(thr_dis, self.workspace_site_dir, self.clip_names)

    def check_input_mask(self):
        # check exists, width and height

        for input_clip_dir in self.clip_dirs:
            for camera_name in self.camera_names:
                input_mask_path = self.input_mask_path[camera_name]
                if input_mask_path != "":
                    if not exists(input_mask_path):
                        logger.error(f"ERROR: {input_mask_path} not exists")
                        exit()
                    else:
                        dr = DatasetReader(str(input_clip_dir))
                        intrinsic = dr.get_intrinsics(camera_name)
                        width, height = intrinsic["width"], intrinsic["height"]
                        input_mask =\
                            cv2.imread(input_mask_path, cv2.IMREAD_GRAYSCALE)
                        if input_mask.shape != (height, width):
                            logger.error(f"ERROR: input_mask{input_mask_path} has different size with {input_clip_dir}/{camera_name} image")# noqa : E501
                            exit()

    def make_foreground_mask(self, label_img, camera_name):
        """
            make foreground mask for colmap
        """
        h, w = label_img.shape
        input_mask_path = self.input_mask_path[camera_name]

        if input_mask_path == "":
            foreground = np.ones([h, w]) * 255
        else:
            foreground = cv2.imread(input_mask_path, cv2.IMREAD_GRAYSCALE)

        # mask_sky
        foreground[label_img == 20] = 0

        if self.mask_dynamic:
            foreground[
                (label_img >= 9) * (label_img <= 17)] = 0

        if self.mask_ego:
            foreground[
                (label_img == 38)] = 0

        if self.mask_vegetation:
            foreground[
                (label_img == 2) * (label_img == 3)] = 0

        foreground_mvs = foreground.copy()

        return foreground, foreground_mvs

    def write_one_frame(self, argument_list):
        (
            camera_name,
            camera_path,
            intrinsics,
            d,
            scale,
            new_cam_matrix,
            output_image_dir,
            output_mask_dir,
            output_label_dir,
            output_mask_mvs_dir,
            output_lane_label_dir,
        ) = argument_list

        timestamp = camera_path.split("/")[-1].split(".")[0]
        image = cv2.imread(camera_path)
        undistort_img = undistort(
            image=image,
            intrinsics=intrinsics,
            d=d,
            scale=scale,
            new_cam_matrix=new_cam_matrix,
            camera_name=camera_name,
            interpolation=cv2.INTER_LINEAR
        )

        label_path = camera_path.replace("/Raw_data/","/4DLabel_output/Prelabel/"
                                ).replace(camera_name, f"seg_{camera_name}"
                                ).replace(".jpg", ".png")


        label_img = cv2.imread(label_path, -1)
        undistort_label = undistort(
            image=label_img,
            intrinsics=intrinsics,
            d=d,
            scale=scale,
            new_cam_matrix=new_cam_matrix,
            camera_name=camera_name,
            interpolation=cv2.INTER_NEAREST
        )
        foreground, foreground_mvs = self.make_foreground_mask(
            undistort_label, camera_name
        )

        lane_label_path = camera_path.replace("/Raw_data/","/4DLabel_output/Prelabel/"
                                ).replace(camera_name, f"lane_{camera_name}").replace(".jpg", ".png")

        lane_label_img = cv2.imread(lane_label_path, -1)
        undistort_lane_label = undistort(
            image=lane_label_img,
            intrinsics=intrinsics,
            d=d,
            scale=scale,
            new_cam_matrix=new_cam_matrix,
            camera_name=camera_name,
            interpolation=cv2.INTER_NEAREST
        )

        image_output_path = str(
            join(output_image_dir, f"{timestamp}.jpg")
        )
        mask_output_path = str(
            join(output_mask_dir, f"{timestamp}.jpg.png")
        )
        mask_mvs_output_path = str(
            join(output_mask_mvs_dir, f"{timestamp}.jpg.png")
        )
        label_output_path = str(
            join(output_label_dir, f"{timestamp}.png")
        )
        lane_label_output_path = str(
            join(output_lane_label_dir, f"{timestamp}.png")
        )
        # logger.info(f"Writing image: {image_output_path}")

        cv2.imwrite(image_output_path, undistort_img)
        cv2.imwrite(mask_output_path, foreground)
        cv2.imwrite(mask_mvs_output_path, foreground_mvs)
        cv2.imwrite(label_output_path, undistort_label)
        cv2.imwrite(lane_label_output_path, undistort_lane_label)

    def check_sync(self, input_clip_dir, all_camera_front_paths):
        # read attribute.json
        attribute_path = join(input_clip_dir, "attribute.json")
        if exists(attribute_path):
            with open(attribute_path, "r") as file:
                attribute = json.load(file)
        else:
            raise FileNotFoundError(f"can not find {attribute_path}")

        sync = attribute["sync"]
        sync_list = OrderedDict()
        for camera_name in self.camera_names:
            sync_list[f"{camera_name}"] = np.array(
                sorted(sync[f"{camera_name}"]))

        sync_idx = []
        real_sync_list = OrderedDict()
        for camera_name in self.camera_names:
            real_sync_list[f"{camera_name}"] = []

        all_camera_front_timestamps = [
            int(p.split("/")[-1].split(".")[0]) for p in all_camera_front_paths
        ]

        for idx, timestamp in enumerate(all_camera_front_timestamps):
            flag = True
            for camera_name in self.camera_names:
                cur_timestamps = sync_list[f"{camera_name}"]
                right_idx = np.searchsorted(
                    cur_timestamps, timestamp, side="left"
                )
                left_idx = right_idx - 1
                left_time_diff = (
                    timestamp - cur_timestamps[left_idx]
                    if left_idx >= 0 else float("inf")
                )
                right_time_diff = (
                    cur_timestamps[right_idx] - timestamp
                    if right_idx < cur_timestamps.shape[0] else float("inf")
                )
                time_diff = min(left_time_diff, right_time_diff)
                if time_diff > self.t_max_diff:
                    flag = False
                query_index = (
                    left_idx
                    if left_time_diff < right_time_diff else right_idx
                )
                real_sync_list[f"{camera_name}"].append(
                    int(cur_timestamps[query_index])
                )

            if not flag:
                for camera_name in self.camera_names:
                    if len(real_sync_list[f"{camera_name}"]) > 0:
                        # pop back
                        real_sync_list[f"{camera_name}"] = \
                            real_sync_list[f"{camera_name}"][0:-1]
            else:
                sync_idx.append(idx)

        return sync_idx, real_sync_list

    def make_savedir(self, clip_name, camera_name):
        output_image_dir = join(
            self.workspace_site_dir, clip_name, camera_name, "keyframe"
        )
        os.makedirs(output_image_dir, exist_ok=True)

        output_param_dir = join(
            self.workspace_site_dir, clip_name, camera_name
        )
        os.makedirs(output_param_dir, exist_ok=True)
        output_mask_dir = join(
            self.workspace_site_dir, clip_name, camera_name, "mask"
        )
        os.makedirs(output_mask_dir, exist_ok=True)

        output_label_dir = join(
            self.workspace_site_dir, clip_name, camera_name, "seg"
        )
        os.makedirs(output_label_dir, exist_ok=True)

        output_mask_mvs_dir = join(
            self.workspace_site_dir, clip_name, camera_name, "mask_mvs"
        )
        os.makedirs(output_mask_mvs_dir, exist_ok=True)

        output_lane_label_dir = join(
            self.workspace_site_dir, clip_name, camera_name, "lane"
        )
        os.makedirs(output_lane_label_dir, exist_ok=True)

        return (output_image_dir, output_mask_dir,
                output_label_dir, output_mask_mvs_dir, output_lane_label_dir, output_param_dir)

    def generate_other_exist_file(self, camera_front_paths, sync_list, check_prelabel=True):
        other_file_exsit_mask = np.ones_like(camera_front_paths).astype(bool)
        for camera_name in self.camera_names:
            if camera_name == "camera_front":
                continue
            else:
                cur_camera_paths = [
                    a.replace("camera_front", f"{camera_name}")
                    for a in camera_front_paths
                ]
                cur_camera_paths = replace_timestamps(
                    cur_camera_paths,
                    sync_list["camera_front"],
                    sync_list[f"{camera_name}"],
                )
                cur_file_exist_mask = check_filelist_exist(
                    cur_camera_paths, check_prelabel
                )
                other_file_exsit_mask &= cur_file_exist_mask

        # save valid real sync timestamps
        other_file_exsit_idx = np.where(other_file_exsit_mask)[0]
        other_valid_camera_paths = []
        valid_sync_list = OrderedDict()
        for camera_name in self.camera_names:
            valid_sync_list[f"{camera_name}"] = []

        for idx in other_file_exsit_idx:
            other_valid_camera_paths.append(camera_front_paths[idx])
            for camera_name in self.camera_names:
                valid_sync_list[f"{camera_name}"].append(
                    sync_list[f"{camera_name}"][idx]
                )

        other_camera_path = OrderedDict()
        for camera_name in self.camera_names:
            cur_camera_paths = [
                a.replace("camera_front", f"{camera_name}")
                for a in other_valid_camera_paths
            ]
            cur_camera_paths = replace_timestamps(
                cur_camera_paths,
                valid_sync_list["camera_front"],
                valid_sync_list[f"{camera_name}"],
            )
            if check_prelabel and ("fisheye" in camera_name):
                other_camera_path[f"{camera_name}"] = cur_camera_paths[::self.fisheye_keyframe_step]
                if len(other_camera_path[f"{camera_name}"]) == 1:
                    logger.info(f"{camera_name} just have one keyframe, skip downsample")
                    other_camera_path[f"{camera_name}"] = cur_camera_paths
                    
            else:
                other_camera_path[f"{camera_name}"] = cur_camera_paths
            
        return other_camera_path

    def detect_keyframe(
        self,
        camera_name,
        input_clip_base_path,
        output_image_dir,
        output_mask_dir,
        output_label_dir,
        output_mask_mvs_dir,
        output_lane_label_dir,
        output_param_dir,
        scale=1
    ):
        """
            detect keyframe
        """
        clip_name = basename(input_clip_base_path)
        if self.init_odo_type.startswith("_"):
            odo_path = join(
                self.workspace_site_dir, clip_name,
                "odometry",
                self.init_odo_type[1:]+".txt"
            )
        else:
            odo_path = join(
                self.workspace_site_dir, clip_name,
                "odometry",
                self.init_odo_type+".txt"
            )

        odo_tum = np.loadtxt(odo_path)
        logger.info(f"Get odo from {odo_path} odo_tum: {odo_tum.shape}")

        cam_matrix, dist_coeffs, new_cam_matrix = \
                        self.get_params(input_clip_base_path, camera_name, scale, output_param_dir) # noqa : E501

        ts_ind = np.argsort(odo_tum[:, 0])
        odo_tum = odo_tum[ts_ind]

        pt = PoseTransformer()
        pt.loadarray(odo_tum)
        pt.normalize2origin()

        dr = DatasetReader(str(input_clip_base_path))
        last_transform = np.eye(4, dtype=np.float32)
        argument_list = []
        for camera_path in dr.yield_sensor_filepath(camera_name, ext="jpg", sync=self.sync):# noqa : E501
            timestamp = (
                float(camera_path.split("/")[-1].split(".")[0]) / 1000.0
            )
            try:
                current_pose = pt.seek_by_timestamp(
                    timestamp, t_max_diff=0.5, interpolate=True)
            except RuntimeError as e:
                logger.info(
                    f"Failed seek pose {e}, skip this frame {camera_path}"
                )
                continue

            relative_transform = np.linalg.inv(last_transform) @ current_pose
            distance = np.linalg.norm(relative_transform[:3, 3], ord=2)
            yaw_degree = Rotation.from_matrix(
                relative_transform[:3, :3]).as_euler("xyz", degrees=True)[2]

            if distance < 0:
                logger.error(f"distance is negative: {distance}")
                argument_list.append(
                    (
                        camera_name,
                        camera_path,
                        cam_matrix,
                        dist_coeffs,
                        scale,
                        new_cam_matrix,
                        output_image_dir,
                        output_mask_dir,
                        output_label_dir,
                        output_mask_mvs_dir,
                        output_lane_label_dir,
                    )
                )
                last_transform = current_pose.copy()
                continue

            if (
                distance > self.front_keyframe_meter
                or abs(yaw_degree) > self.front_keyframe_yaw
            ):
                # logger.info(f"adding keyframe: {camera_path}")
                argument_list.append(
                    (
                        camera_name,
                        camera_path,
                        cam_matrix,
                        dist_coeffs,
                        scale,
                        new_cam_matrix,
                        output_image_dir,
                        output_mask_dir,
                        output_label_dir,
                        output_mask_mvs_dir,
                        output_lane_label_dir,
                    )
                )
                last_transform = current_pose.copy()
                continue

        logger.info(f"Detect {len(argument_list)} keyframes")

        return len(argument_list), argument_list

    @timer
    def keyframe_selection_unsync(self):
        scale = 1
        for input_clip_dir in self.clip_dirs:
            clip_name = input_clip_dir.split("/")[-1]

            clip_base_path = join(self.workspace_site_dir, clip_name)
            if not self.rewrite_keyframe and exists(clip_base_path):
                logger.info(
                    f"Skip {clip_name} because keyframe already exists")
                continue

            num_key_frames = 0

            for camera_name in self.camera_names:
                # generate output paths
                (
                    output_image_dir, output_mask_dir,
                    output_label_dir, output_mask_mvs_dir, output_lane_label_dir, output_param_dir
                ) = self.make_savedir(clip_name, camera_name)

                _, argument_list = self.detect_keyframe(
                    camera_name,
                    input_clip_dir,
                    output_image_dir,
                    output_mask_dir,
                    output_label_dir,
                    output_mask_mvs_dir,
                    output_lane_label_dir,
                    output_param_dir,
                    scale,
                )

                if len(argument_list) == 0:
                    logger.info(
                        f"{camera_name} detect {len(argument_list)} keyframes"
                    )
                    continue

                camera_paths = [argument[1] for argument in argument_list]

                file_exist_mask = check_filelist_exist(camera_paths)
                file_exist_idx = np.where(file_exist_mask)[0]

                argument_valid_list = []
                for idx in file_exist_idx:
                    argument_valid_list.append(argument_list[idx])

                logger.info(f"begin_worker_pool unsync_{camera_name}")
                worker_pool = (
                    ThreadPoolExecutor(max_workers=8)
                    if self.parallel_worker_type == "thread"
                    else ProcessPoolExecutor(max_workers=8)
                )
                worker_pool.map(self.write_one_frame, argument_valid_list)
                logger.info("end_worker_pool")
                worker_pool.shutdown()
                num_key_frames += len(argument_valid_list)

            logger.info(
                f"Totally, {clip_name} select {num_key_frames} key frames"
            )

            if num_key_frames < self.num_key_frames_th:
                self.dump_clip_list.append(clip_name)
                logger.info(
                    f"Keyframes less{self.num_key_frames_th}, del {clip_name}!"
                )
            logger.info(f"keyframe_selection {clip_name} done!")
    @timer
    def keyframe_selection_sync(self):
        scale = 1
        for input_clip_dir in self.clip_dirs:
            clip_name = input_clip_dir.split("/")[-1]

            clip_base_path = join(self.workspace_site_dir, clip_name)
            if not self.rewrite_keyframe and exists(clip_base_path):
                logger.info(
                    f"Skip {clip_name} because keyframe already exists")
                continue

            # generate camera front keyframes
            camera_name = "camera_front"
            num_key_frames = 0
            # generate output paths
            (
                output_image_dir, output_mask_dir,
                output_label_dir, output_mask_mvs_dir, output_lane_label_dir, output_param_dir
            ) = self.make_savedir(clip_name, camera_name)

            camera_front_keyframe_list_path = join(
                self.site_output_dir,
                "Prelabel",
                f"{clip_name}_camera_front_keyframe_list.pkl",
            )

            if not exists(camera_front_keyframe_list_path):
                _, argument_camera_front_list = self.detect_keyframe(
                    camera_name,
                    input_clip_dir,
                    output_image_dir,
                    output_mask_dir,
                    output_label_dir,
                    output_mask_mvs_dir,
                    output_lane_label_dir,
                    output_param_dir,
                    scale,
                )
            else:
                try:
                    with open(camera_front_keyframe_list_path, "rb") as f:
                        argument_camera_front_list = RestrictedUnpickler(f).load()
                except FileNotFoundError:
                    print(f"File not found: {camera_front_keyframe_list_path}")
                    return None
                except pickle.UnpicklingError as e:
                    print(f"Pickle decode error: {e}")
                    return None
                
            if len(argument_camera_front_list) == 0:
                self.dump_clip_list.append(clip_name)
                logger.info(
                    f"{camera_name} detect {len(argument_camera_front_list)} keyframes"
                )
                continue

            all_camera_front_paths = [
                argument[1] for argument in argument_camera_front_list
            ]

            # check file exist
            file_exist_mask = check_filelist_exist(all_camera_front_paths)
            file_exist_idx = np.where(file_exist_mask)[0]
            # check sync
            sync_idx, sync_list = self.check_sync(input_clip_dir, all_camera_front_paths)  # noqa : E501

            valid_sync_list = OrderedDict()
            for camera_name in self.camera_names:
                valid_sync_list[camera_name] = [v for v, m in zip(
                    sync_list[camera_name], file_exist_mask) if m]

            valid_idx = list(set(file_exist_idx) & set(sync_idx))
            all_valid_sync_camera_front_paths = []
            for idx in valid_idx:
                all_valid_sync_camera_front_paths.append(
                    all_camera_front_paths[idx]
                )

            # generate other camera keyframes
            other_camera_path = \
                self.generate_other_exist_file(all_valid_sync_camera_front_paths, valid_sync_list) # noqa : E501

            # write keyframes
            for camera_name in self.camera_names:
                (
                    output_image_dir, output_mask_dir,
                    output_label_dir, output_mask_mvs_dir, output_lane_label_dir, output_param_dir
                ) = self.make_savedir(clip_name, camera_name)

                cur_camera_paths = other_camera_path[camera_name]
                cam_matrix, dist_coeffs, new_cam_matrix = \
                    self.get_params(input_clip_dir, camera_name, scale, output_param_dir) # noqa : E501

                argument_list = []
                for camera_path in cur_camera_paths:
                    argument_list.append(
                        (
                            camera_name,
                            camera_path,
                            cam_matrix,
                            dist_coeffs,
                            scale,
                            new_cam_matrix,
                            output_image_dir,
                            output_mask_dir,
                            output_label_dir,
                            output_mask_mvs_dir,
                            output_lane_label_dir,
                        )
                    )

                logger.info(
                    f"{camera_name} detect {len(argument_list)} keyframes after sync and file exist check"  # noqa : E501
                )

                logger.info("begin_worker_pool sync_camera_other")
                worker_pool = (
                    ThreadPoolExecutor(max_workers=8)
                    if self.parallel_worker_type == "thread"
                    else ProcessPoolExecutor(max_workers=8)
                )
                worker_pool.map(self.write_one_frame, argument_list)
                logger.info("end_worker_pool")
                worker_pool.shutdown()

                num_key_frames += len(argument_list)

            logger.info(
                f"Totally, {clip_name} select {num_key_frames} key frames"
            )

            if num_key_frames < self.num_key_frames_th:
                self.dump_clip_list.append(clip_name)
                logger.info(
                    f"Keyframes less{self.num_key_frames_th}, del {clip_name}!"
                )
            logger.info(f"keyframe_selection {clip_name} done!")
    
    def find_candidates(self, distances, indices, thr_dis, candidates_neighbor_num):
        """
        根据距离将候选对分段，均匀选取每段的候选对。

        Parameters:
            distances (list or np.ndarray): 当前点与候选点的距离数组。
            indices (list or np.ndarray): 对应的候选点索引。
            thr_dis (float): 距离阈值，定义分段的上限。
            candidates_neighbor_num (int): 最多允许选取的总候选对数。

        Returns:
            list: 筛选后的候选点索引。
        """
        # 定义距离分段
        bins = np.linspace(0, thr_dis, int(candidates_neighbor_num)+1)  # 分为 [0, thr_dis] 的 3 段
        max_per_bin = 1  # 每段最多选取的候选数
        selected_indices = []

        # 遍历每个区间，均匀选取候选点
        for i in range(len(bins) - 1):
            # 找到距离在当前区间的候选点索引
            in_bin = np.where((distances >= bins[i]) & (distances < bins[i + 1]))[0]
            
            # 如果区间内有候选点，从中取最多 max_per_bin 个点
            if len(in_bin) > 0:
                sample_size = min(len(in_bin), max_per_bin)
                # sampled = np.random.choice(in_bin, sample_size, replace=False)
                sampled = in_bin[:sample_size]  # 直接取前 sample_size 个点
                selected_indices.extend(sampled)

        return selected_indices
    
    
    def create_spatial_pairs(self, thr_dis):
        logger.info("#### Create pairs ####")
        keyframe = []
        gnss = []
        angle = []
        for clip_name in self.clip_names:
            for camera_name in self.camera_names:
                keyframe_dir = join(
                    self.workspace_site_dir, clip_name, camera_name, "keyframe"
                )
                gnss_dir = join(
                    self.workspace_site_dir,
                    clip_name,
                    camera_name,
                    f"{camera_name}{self.init_odo_type}.txt"
                )
                logger.info(f"Get pair of camera_name path: {gnss_dir}")
                clip_keyframe = get_keyframe(keyframe_dir)
                gnss_list = list(open(gnss_dir))
                for frame in clip_keyframe:
                    for i in range(len(gnss_list)):
                        ts, tx, ty, tz, qx, qy, qz, qw = [
                            float(i) for i in gnss_list[i].split()
                        ]
                        ts = "{:0.3f}".format(ts)
                        if frame[:11] == ts.replace(".", "")[:11]:
                            gnss.append([tx, ty, tz])
                            angle.append([qx, qy, qz, qw])
                            keyframe.append(
                                join(
                                    clip_name, camera_name, "keyframe", frame
                                )
                            )
                            break

        assert len(gnss) != 0, \
            "No keyframe selected, please check your input data"

        knn = NearestNeighbors()
        knn.fit(gnss)
        distances, positive = knn.radius_neighbors(gnss, radius=thr_dis, sort_results=True)
        spatial_pairs = set()
        
        for i in tqdm(range(len(positive))):
            candidates_neighbor_num = len(positive[i])
            candidates_neighbor_num = self.max_image_pair if candidates_neighbor_num > self.max_image_pair else candidates_neighbor_num
            candidates_idx = self.find_candidates(distances[i], positive[i], thr_dis, candidates_neighbor_num)
            candidates_pairs = set()
            for j in candidates_idx:
                # if self.pairs_mode == "seq+sp":
                #     clip_name1 = keyframe[i].split("/")[0]
                #     clip_name2 = keyframe[positive[i][j]].split("/")[0]
                #     if clip_name1 == clip_name2:
                #         continue

                if (
                    keyframe[i],
                    keyframe[positive[i][j]],
                ) in spatial_pairs or (
                    keyframe[positive[i][j]],
                    keyframe[i],
                ) in spatial_pairs:
                    continue
                else:
                    img1_path = keyframe[i]
                    img2_path = keyframe[positive[i][j]]
                    if img1_path >= img2_path:
                        img1_path, img2_path = img2_path, img1_path
                    angle1 = angle[i]
                    angle2 = angle[positive[i][j]]
                    pos1 = gnss[i]
                    pos2 = gnss[positive[i][j]]
                    clip_name1 = keyframe[i].split("/")[0]
                    clip_name2 = keyframe[positive[i][j]].split("/")[0]
                    if clip_name1 == clip_name2 and axis_z(pos1, pos2) > 3:
                        continue

                    camera_name1 = keyframe[i].split("/")[1]
                    camera_name2 = keyframe[positive[i][j]].split("/")[1]

                    if not (camera_name1 == camera_name2 and camera_name1 in ["camera_front", "camera_rear"]):
                        if distances[i][j] > 20:
                            continue

                    # angle1表示AB两张图的夹角，angle2表示AB向量和A图方向的夹角, angle3表示BA向量和B图方向的夹角
                    angle1, angle2, angle3 = cal_angle_error(
                        angle1, angle2, pos1, pos2
                    )
 
                    if angle1 > 90:
                        if angle1>170: #排除类似同clip front-rear的情况，虽有共视，但匹配不佳
                            continue
                        if angle1+angle2+angle3>181:#三个角不能AB和A向量B向量不能构成三角形，说明A和B向量方向相反
                            continue
                        elif distances[i][j] < thr_dis/2:# 距离太近
                            continue
                        else:
                            candidates_pairs.add(
                                (img1_path, img2_path)
                            )
                    else:
                        # if angle1+angle2+angle3>181:#三个角不能AB和A向量B向量不能构成三角形，说明A和B向量方向相反
                        #     continue
                        if angle2>90 and angle3>90:
                            continue
                        elif angle2+angle3>200:
                            continue
                        else:
                            candidates_pairs.add(
                                (img1_path, img2_path)
                            )
            # print(keyframe[i], len(candidates_pairs))
            spatial_pairs.update(candidates_pairs)

        return spatial_pairs

    def create_sequence_pairs(self, forward_frame_front=20):
        sequence_pairs = set()

        forward_frame_side = forward_frame_front//2
        matchable_cameras = {
            'camera_front': [('camera_front', forward_frame_front), ('camera_front_left', forward_frame_side), ('camera_front_right', forward_frame_side)],
            'camera_front_left':[('camera_front_left', forward_frame_side), ('camera_rear_left', forward_frame_side)],
            'camera_front_right':[('camera_front_right', forward_frame_side), ('camera_rear_right', forward_frame_side)],
            'camera_rear': [('camera_rear', forward_frame_front), ('camera_rear_left', forward_frame_side), ('camera_rear_right', forward_frame_side)],
            'camera_rear_left':[('camera_rear_left', forward_frame_side)],
            'camera_rear_right':[('camera_rear_right', forward_frame_side)]
        }

        for clip_name in self.clip_names:
            for camera_name in self.camera_names:
                if camera_name not in matchable_cameras:
                    continue
                for match_cam, forward_frame in matchable_cameras[camera_name]:
                    keyframe_dir = join(
                        self.workspace_site_dir, clip_name, camera_name, "keyframe"
                    )
                    clip_keyframe = get_keyframe(keyframe_dir)
                    match_keyframe_dir = join(
                        self.workspace_site_dir, clip_name, match_cam, "keyframe"
                    )
                    match_clip_keyframe = get_keyframe(match_keyframe_dir)

                    # Match with each frame from 1 to forward_frame ahead, if they exist.
                    for i in range(len(clip_keyframe)):
                        for fwd in range(1, min(forward_frame, len(match_clip_keyframe) - i) + 1):
                            if i + fwd < len(match_clip_keyframe):
                                img1_path = (join(clip_name, camera_name, "keyframe", clip_keyframe[i]))
                                img2_path = (join(clip_name, match_cam, "keyframe", match_clip_keyframe[i + fwd]))
                                if img1_path < img2_path:
                                    sequence_pairs.add((img1_path, img2_path))
                                else:
                                    sequence_pairs.add((img2_path, img1_path))
        return sequence_pairs

    def create_pairs(self, thr_dis, match_pairs_path):
        # TODO add sequence pairs
        sequence_pairs = set()
        if self.pairs_mode == "seq+sp":
            sequence_pairs = self.create_sequence_pairs(forward_frame_front=10)
            logger.info(f"The length of sequence pairs: {len(sequence_pairs)}")

        spatial_pairs = self.create_spatial_pairs(thr_dis)
        logger.info(f"The length of spatial pairs: {len(spatial_pairs)}")
        # spatial_pairs = set()

        all_pairs = spatial_pairs.union(sequence_pairs)
        logger.info(f"The length of all pairs: {len(all_pairs)}")
        
        if exists(match_pairs_path):
            os.remove(match_pairs_path)
        with open(match_pairs_path, "w") as f:
            for pair in iter(all_pairs):
                if pair[0] == pair[1]:
                    continue
                f.write(str(pair[0]) + " " + str(pair[1]) + "\n")
        return all_pairs

    def check_merge(self, thr_dis, output_site_dir, clip_names):
        match_pairs_path = join(output_site_dir, "match_pairs.txt")
        match_pairs = list(open(match_pairs_path))
        clip_name2id = {}
        clip_id2name = {}
        for i, clip in enumerate(clip_names):
            clip_name2id[clip] = i
            clip_id2name[i] = clip
        clip_nums = len(clip_names)
        clip_graph = np.zeros((clip_nums, clip_nums))

        # build clip_graph by img matched or not
        for pair in match_pairs:
            image_name1, image_name2 = pair.split()
            clip_name1 = image_name1.split("/")[0]
            clip_name2 = image_name2.split("/")[0]
            if clip_name1 != clip_name2:
                id1 = clip_name2id[clip_name1]
                id2 = clip_name2id[clip_name2]
                clip_graph[id1][id2] = 1
                clip_graph[id2][id1] = 1

        # brute force search max_merge_set
        visited = set()
        max_merge_set = []
        for i in range(clip_nums):
            if i not in visited:
                cur_merge_set = [i]
                queue = collections.deque([i])
                visited.add(i)
                while queue:
                    j = queue.popleft()
                    for intrinsics in range(clip_nums):
                        if clip_graph[j][intrinsics] == 1 \
                                and intrinsics not in visited:
                            cur_merge_set.append(intrinsics)
                            visited.add(intrinsics)
                            queue.append(intrinsics)

                if len(max_merge_set) < len(cur_merge_set):
                    max_merge_set = cur_merge_set.copy()

        # rm the clip_dir cann't be merged, rewrite match_pairs.txt;
        if len(max_merge_set) < clip_nums:
            self.clip_names = []
            for i in range(clip_nums):
                if i in max_merge_set:
                    self.clip_names.append(clip_id2name[i])
                else:
                    # rm the clip_dir without overlap with other clips
                    logger.info(f"Cann't merge, dump {clip_id2name[i]}")
                    remove_clip = join(
                        self.workspace_site_dir, clip_id2name[i])
                    exec_command(f"rm -rf {remove_clip}")

            match_pairs_path = join(
                self.workspace_site_dir, "match_pairs.txt"
            )
            self.create_pairs(thr_dis, match_pairs_path)

    def run(self):
        logger.info("Run keyframe selection")
        # Run keyframe selection in tmp dir
        self.check_input_mask()
        if self.sync:
            _, keyframe_selection_time_cost = self.keyframe_selection_sync()
        else:
            _, keyframe_selection_time_cost = self.keyframe_selection_unsync()
        timer_path = join(self.workspace_site_dir, "timer_res.json")
        if exists(timer_path):
            times_result = load_json(timer_path)
        else:
            times_result = {}
        times_result.update(
            {"keyframe_selection_time_cost": keyframe_selection_time_cost})
        save_json(timer_path, times_result)

        # del dump clips
        self.del_dump_clip()

        # get keyframe list
        self.get_keyframe_list()

        # transform pose graph to 6v camera
        self.pose_graph2camera()
        init_odo_type_orig = self.init_odo_type
        self.init_odo_type = "_3ddr"
        self.pose_graph2camera()
        self.init_odo_type = init_odo_type_orig

        # run spatial pairs, get match pairs list
        self.spatial_pairs()

    def keyframe_prepare(self):
        logger.info("Run keyframe preparation")
        # Run keyframe selection in tmp dir
        self.check_input_mask()
        if self.sync:
            scale = 1
            other_camera_path_allclips = []
            for input_clip_dir in self.clip_dirs:
                clip_name = input_clip_dir.split("/")[-1]

                clip_base_path = join(self.workspace_site_dir, clip_name)
                if not self.rewrite_keyframe and exists(clip_base_path):
                    logger.info(
                        f"Skip {clip_name} because keyframe already exists")
                    continue

                # generate camera front keyframes
                camera_name = "camera_front"
                num_key_frames = 0
                # generate output paths
                (
                    output_image_dir, output_mask_dir,
                    output_label_dir, output_mask_mvs_dir, output_lane_label_dir, output_param_dir
                ) = self.make_savedir(clip_name, camera_name)

                _, argument_camera_front_list = self.detect_keyframe(
                    camera_name,
                    input_clip_dir,
                    output_image_dir,
                    output_mask_dir,
                    output_label_dir,
                    output_mask_mvs_dir,
                    output_lane_label_dir,
                    output_param_dir,
                    scale,
                )
                
                if len(argument_camera_front_list) == 0:
                    self.dump_clip_list.append(clip_name)
                    logger.info(
                        f"{camera_name} detect {len(argument_camera_front_list)} keyframes"
                    )
                    continue
                # write all camera front keyframes paths
                camera_front_keyframe_list_path = join(
                    self.site_output_dir,
                    "Prelabel",
                    f"{clip_name}_camera_front_keyframe_list.pkl",
                )
                with open(camera_front_keyframe_list_path, "wb") as f:
                    pickle.dump(
                        argument_camera_front_list,
                        f)
                    
                all_camera_front_paths = [
                    argument[1] for argument in argument_camera_front_list
                ]

                # check sync
                sync_idx, sync_list = self.check_sync(input_clip_dir, all_camera_front_paths)  # noqa : E501

                valid_sync_list = OrderedDict()
                for camera_name in self.camera_names:
                    valid_sync_list[camera_name] = sync_list[camera_name]

                valid_idx = list(set(sync_idx))
                all_valid_sync_camera_front_paths = []
                for idx in valid_idx:
                    all_valid_sync_camera_front_paths.append(
                        all_camera_front_paths[idx]
                    )

                # generate other camera keyframes
                other_camera_path = \
                    self.generate_other_exist_file(all_valid_sync_camera_front_paths, valid_sync_list, check_prelabel=False) # noqa : E501
                other_camera_path_allclips.append(other_camera_path)
            # write all keyframes paths
            keyframe_list_path = join(
                self.site_output_dir,
                "Prelabel",
                "keyframe_list.txt",
            )
            with open(keyframe_list_path, "w") as f:
                for other_camera_path in other_camera_path_allclips:
                    for camera_name, camera_images in other_camera_path.items():
                        for camera_image in camera_images:
                            f.write(camera_image + "\n")
                
        else:
            logger.error(f"ERROR: unsync not supported")# noqa : E501
            exit()

if __name__ == "__main__":
    import yaml

    def read_config(config_file):
        with open(config_file, "r") as f:
            content = yaml.safe_load(f)
        return content

    config_file = join(dirname(dirname(__file__)), "sfm_config_default.yaml")
    sfm_config = read_config(config_file)

    # run keyframe selection
    KS = KeyframeSelection(sfm_config)
    KS.run()
