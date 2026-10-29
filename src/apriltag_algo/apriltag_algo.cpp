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
    td_->debug = false;  // never
}

ApriltagAlignment::~ApriltagAlignment() {
    apriltag_detector_destroy(td_);
    tag36h11_destroy(tf_);
}

bool ApriltagAlignment::align(const ImageU8& tag_small_img, 
                              float scale_x, float scale_y, 
                              int offset_x, int offset_y) {
    found_36h11_ = false;
    tag_id_ = -1;
    debug_det_id_ = -1;
    debug_rotation_.fill(0.0);
    debug_translation_.fill(0.0);

    // 1. 锁死最高精度：因为输入已经是极速小图 (64x64)
    td_->quad_decimate = 1.0;

    // 2. 构造 image_u8_t
    image_u8_t im = {
        tag_small_img.width,
        tag_small_img.height,
        tag_small_img.stride,
        tag_small_img.data
    };

    // 3. 执行检测 (此时耗时极短)
    auto start_time = std::chrono::high_resolution_clock::now();
    zarray_t* detections = apriltag_detector_detect(td_, &im);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    if (debug_) {
        std::cout << "width: " << tag_small_img.width << ", height: " << tag_small_img.height << std::endl;
        double debug_time_cost_ = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        std::cout << "[time cost] apriltag detector detect (64x64): " << debug_time_cost_ << " ms\n";
    }

    // 4. 遍历检测结果匹配 ID，并进行坐标逆向投影！
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
            
            // ============== 核心修改：坐标逆向投影 ==============
            // 将 64x64 局部坐标系下的中心点还原到 1080p 全局坐标系
            det->c[0] = det->c[0] * scale_x + offset_x;
            det->c[1] = det->c[1] * scale_y + offset_y;
            
            // 还原四个角点
            for (int k = 0; k < 4; k++) {
                det->p[k][0] = det->p[k][0] * scale_x + offset_x;
                det->p[k][1] = det->p[k][1] * scale_y + offset_y;
            }
            break; // 找到就退出循环
        }
    }

    if (!found_36h11_) {
        apriltag_detections_destroy(detections);
        return false;
    }

    // 5. 保存全局状态 (用于 debug 可视化)
    tag_id_ = valid_det->id;
    debug_det_id_ = valid_det->id;
    debug_det_margin_ = valid_det->decision_margin;
    debug_det_hamming_ = valid_det->hamming;
    for (int i = 0; i < 4; ++i) {
        debug_corners_full_[i] = {valid_det->p[i][0], valid_det->p[i][1]}; // 已经是全局坐标
    }

    // 6. 位姿解算 (此时 valid_det 里装的都是 1080p 的像素坐标了)
    apriltag_detection_info_t info;
    info.det = valid_det;
    info.tagsize = valid_param.length; 
    info.fx = fx_;
    info.fy = fy_;
    info.cx = cx_; // 直接用原始光心 cx，不需要减去偏移了，因为像素已经被拉回全局了！
    info.cy = cy_; 

    apriltag_pose_t pose;
    start_time = std::chrono::high_resolution_clock::now();
    estimate_tag_pose(&info, &pose);
    end_time = std::chrono::high_resolution_clock::now();
    if (debug_) {
        double debug_time_cost_ = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        std::cout << "[time cost] apriltag pose estimate: " << debug_time_cost_ << " ms\n";
    }

    for (int r = 0; r < 3; ++r) {
        debug_translation_[r] = MATD_EL(pose.t, r, 0);
        for (int c = 0; c < 3; ++c) {
            debug_rotation_[r * 3 + c] = MATD_EL(pose.R, r, c);
        }
    }

    // 7. 坐标转换：P_camera = R * P_local + t
    matd_t* P_local = matd_create(3, 1);
    MATD_EL(P_local, 0, 0) = -valid_param.shiftX;
    MATD_EL(P_local, 1, 0) =  valid_param.shiftY;
    MATD_EL(P_local, 2, 0) =  0.0;

    matd_t* R_times_Plocal = matd_multiply(pose.R, P_local);
    matd_t* P_camera = matd_add(R_times_Plocal, pose.t);

    tagShift_x_ = MATD_EL(P_camera, 0, 0);
    tagShift_y_ = MATD_EL(P_camera, 1, 0);
    tagShift_z_ = MATD_EL(P_camera, 2, 0);
    tagYaw_ = std::atan2(MATD_EL(pose.R, 1, 0), MATD_EL(pose.R, 0, 0));

    matd_destroy(P_local);
    matd_destroy(R_times_Plocal);
    matd_destroy(P_camera);
    matd_destroy(pose.R);
    matd_destroy(pose.t);
    apriltag_detections_destroy(detections);

    return true;
}