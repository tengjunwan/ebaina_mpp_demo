#include "apriltag_algo.h"

#include <iostream>
#include <algorithm>
#include <chrono>
#include <cmath> // 必须引入，以使用 std::atan2

extern "C" {
#include "apriltag.h"
#include "tag36h11.h"
#include "apriltag_pose.h"
#include "common/matd.h" // AprilTag 自带的轻量级矩阵库
}



ApriltagAlignment::ApriltagAlignment(
    double fx, double fy, double cx, double cy,
    const std::unordered_map<int, TagParam>& tag_param_dict, 
    bool debug)
    : fx_(fx), fy_(fy), cx_(cx), cy_(cy), tag_params_(tag_param_dict), debug_(debug)
{
    tf_ = tag36h11_create();
    td_ = apriltag_detector_create();
    apriltag_detector_add_family(td_, tf_);

    td_->nthreads = 2;
    td_->refine_edges = true;
    // td_->debug = debug_;
}

ApriltagAlignment::~ApriltagAlignment() {
    apriltag_detector_destroy(td_);
    tag36h11_destroy(tf_);
}

bool ApriltagAlignment::align(const ImageU8& full_gray_img, const CropRect& bbox) {
    found_36h11_ = false;
    tag_id_ = -1;
    debug_det_id_ = -1;
    debug_rotation_.fill(0.0);
    debug_translation_.fill(0.0);
    
    int img_h = full_gray_img.height;
    int img_w = full_gray_img.width;

    // 1. 边界外扩保护
    int x_min = std::max(0, bbox.x);
    int y_min = std::max(0, bbox.y);
    int x_max = std::min(img_w, bbox.x + bbox.width);
    int y_max = std::min(img_h, bbox.y + bbox.height);

    int crop_w = x_max - x_min;
    int crop_h = y_max - y_min;

    if (crop_w <= 0 || crop_h <= 0) return false;    
    
    // ==========================================
    // 强制使用整数降采样，绝对避免小数插值导致的边缘模糊
    // ==========================================
    int max_dim = std::max(crop_w, crop_h);

    if (max_dim < 120) {
        td_->quad_decimate = 1.0;
    } else if (max_dim < 240) {
        td_->quad_decimate = 2.0;
    } else if (max_dim < 360) {
        td_->quad_decimate = 3.0;
    } else {
        td_->quad_decimate = 4.0;
    }

    std::cout << "max_dim: " << max_dim << std::endl;
    std::cout << "quad_decimate: " << td_->quad_decimate << std::endl;

    // 3. 补偿光心坐标
    double cx_crop = cx_ - x_min;
    double cy_crop = cy_ - y_min;

    // 4. 计算裁剪区域首地址 (Zero-Copy)
    // 假设原始图像按 stride 连续对齐
    uint8_t* crop_start_addr = full_gray_img.data + y_min * full_gray_img.stride + x_min;

    // image_u8_t has const width/height/stride members, so initialize it as
    // an aggregate (field order: width, height, stride, buffer).
    image_u8_t im = {
        crop_w,
        crop_h,
        full_gray_img.stride,  // 步长必须保留原图的步长
        crop_start_addr
    };

    // 5. 执行检测
    auto start_time = std::chrono::high_resolution_clock::now();
    zarray_t* detections = apriltag_detector_detect(td_, &im);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    if (debug_) {
        double debug_time_cost_ = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        std::cout << "[time cost] apriltag detector detect: " << debug_time_cost_ << " ms\n";
    }

    // 6. 遍历检测结果匹配 ID
    apriltag_detection_t* valid_det = nullptr;
    TagParam valid_param;

    for (int i = 0; i < zarray_size(detections); i++) {
        apriltag_detection_t* det;
        zarray_get(detections, i, &det);
        
        auto it = tag_params_.find(det->id);
        if (it != tag_params_.end()) {
            valid_det = det;
            valid_param = it->second;
            found_36h11_ = true;
            break;
        }
    }

    if (!found_36h11_) {
        apriltag_detections_destroy(detections);
        return false;
    }

    // 7. 保存必要状态
    tag_id_ = valid_det->id;
    debug_det_id_ = valid_det->id;
    debug_det_margin_ = valid_det->decision_margin;
    debug_det_hamming_ = valid_det->hamming;
    for (int i = 0; i < 4; ++i) {
        debug_corners_full_[i] = {valid_det->p[i][0] + x_min, valid_det->p[i][1] + y_min};
    }

    // 8. 位姿解算 (AprilTag 官方接口)
    apriltag_detection_info_t info;
    info.det = valid_det;
    info.tagsize = valid_param.length; 
    info.fx = fx_;
    info.fy = fy_;
    info.cx = cx_crop;
    info.cy = cy_crop;

    
    start_time = std::chrono::high_resolution_clock::now();
    // ---------------- 原来的代码 ----------------
    apriltag_pose_t pose;
    estimate_tag_pose(&info, &pose);
    
    // ---------------- 修改后的代码 ----------------
    // apriltag_pose_t pose;
    // apriltag_pose_t pose1, pose2;
    // double err1, err2;

    // // 直接调用底层的迭代函数，将原本写死的 50 次迭代改为 5 次（无人机降落 5~10 次精度完全足够）
    // estimate_tag_pose_orthogonal_iteration(&info, &err1, &pose1, &err2, &pose2, 5);

    // // AprilTag PnP 会有两个解，我们选择重投影误差更小的那一个
    // if (err1 <= err2) {
    //     pose.R = pose1.R;
    //     pose.t = pose1.t;
    //     // 【关键】必须销毁被抛弃的解的矩阵内存，否则会导致内存泄漏！
    //     if (pose2.R) matd_destroy(pose2.R);
    //     if (pose2.t) matd_destroy(pose2.t);
    // } else {
    //     pose.R = pose2.R;
    //     pose.t = pose2.t;
    //     if (pose1.R) matd_destroy(pose1.R);
    //     if (pose1.t) matd_destroy(pose1.t);
    // }

    
    end_time = std::chrono::high_resolution_clock::now();
    if (debug_) {
        double debug_time_cost_ = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        std::cout << "[time cost] apriltag pose estimate: " << debug_time_cost_ << " ms\n";
    }

    // Keep a plain C++ copy for application-side visualization. This header
    // and library API remains independent of OpenCV.
    for (int r = 0; r < 3; ++r) {
        debug_translation_[r] = MATD_EL(pose.t, r, 0);
        for (int c = 0; c < 3; ++c) {
            debug_rotation_[r * 3 + c] = MATD_EL(pose.R, r, c);
        }
    }

    // 9. 坐标转换：P_camera = R * P_local + t
    // 使用 AprilTag 库自带的 matd_t 矩阵运算，彻底抛弃 cv::Mat
    
    // 构造 P_local 向量 (3x1)
    matd_t* P_local = matd_create(3, 1);
    MATD_EL(P_local, 0, 0) = -valid_param.shiftX;
    MATD_EL(P_local, 1, 0) =  valid_param.shiftY;
    MATD_EL(P_local, 2, 0) =  0.0;

    // R * P_localA
    matd_t* R_times_Plocal = matd_multiply(pose.R, P_local);
    
    // P_camera = R * P_local + t
    matd_t* P_camera = matd_add(R_times_Plocal, pose.t);

    // 提取最终结果
    tagShift_x_ = MATD_EL(P_camera, 0, 0);
    tagShift_y_ = MATD_EL(P_camera, 1, 0);
    tagShift_z_ = MATD_EL(P_camera, 2, 0);
    tagYaw_ = std::atan2(MATD_EL(pose.R, 1, 0), MATD_EL(pose.R, 0, 0));

    // 10. 销毁局部 C 对象，防止内存泄漏
    matd_destroy(P_local);
    matd_destroy(R_times_Plocal);
    matd_destroy(P_camera);
    matd_destroy(pose.R);
    matd_destroy(pose.t);
    apriltag_detections_destroy(detections);

    return true;
}
