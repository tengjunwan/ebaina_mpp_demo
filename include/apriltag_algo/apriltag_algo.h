#pragma once

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <array>

struct apriltag_family;
typedef struct apriltag_family apriltag_family_t;

struct apriltag_detector;
typedef struct apriltag_detector apriltag_detector_t;

// 定义数据结构
struct TagParam {
    double length;  // tag的长度（单位m）
    double shiftX;  // tag坐标x（单位m，相对于板子上的原点）
    double shiftY;  // tag坐标y（单位m，相对于板子上的原点）
};

// 替代 cv::Rect
struct CropRect {
    int x;
    int y;
    int width;
    int height;
};

// 替代 cv::Mat 的极简图像封装 (对应海思的 Y 内存)
struct ImageU8 {
    uint8_t* data;
    int width;
    int height;
    int stride;
};

// Pixel coordinates for optional visualization by the application layer.
struct ImagePoint {
    double x;
    double y;
};

// 算法类定义
class ApriltagAlignment {
public:
    ApriltagAlignment(double fx, double fy, double cx, double cy,
                      const std::unordered_map<int, TagParam>& tag_param_dict,
                      bool debug = false);
    ~ApriltagAlignment();

    ApriltagAlignment(const ApriltagAlignment&) = delete;
    ApriltagAlignment& operator=(const ApriltagAlignment&) = delete;

    // ----核心业务接口----
    // 传入硬件提取的 ImageU8 结构体和裁剪框信息
    bool align(const ImageU8& tag_small_img, 
               float scale_x, float scale_y, 
               int offset_x, int offset_y);

    // ----状态获取接口----
    bool is_found() const { return found_36h11_; }
    int get_tag_id() const { return tag_id_; }
    double get_shift_x() const { return tagShift_x_; }
    double get_shift_y() const { return tagShift_y_; }
    double get_shift_z() const { return tagShift_z_; }
    double get_yaw() const { return tagYaw_; }
    
    int get_debug_hamming() const { return debug_det_hamming_; }
    double get_debug_margin() const { return debug_det_margin_; }
    const std::array<ImagePoint, 4>& get_debug_corners() const { return debug_corners_full_; }
    // Row-major rotation matrix mapping tag-local vectors into camera coordinates.
    const std::array<double, 9>& get_debug_rotation() const { return debug_rotation_; }
    // AprilTag pose translation, in camera coordinates, in meters.
    const std::array<double, 3>& get_debug_translation() const { return debug_translation_; }

private:
    // =========members=========
    double fx_;
    double fy_;
    double cx_;
    double cy_;

    std::unordered_map<int, TagParam> tag_params_;
    bool debug_;

    apriltag_family_t* tf_;
    apriltag_detector_t* td_;
    
    bool found_36h11_ = false;
    int tag_id_ = -1;
    double tagShift_x_ = 0.0;
    double tagShift_y_ = 0.0;
    double tagShift_z_ = 0.0;
    double tagYaw_ = 0.0;

    int debug_det_id_ = -1;
    double debug_det_margin_ = 0.0;
    int debug_det_hamming_ = 0;
    std::array<ImagePoint, 4> debug_corners_full_{};
    std::array<double, 9> debug_rotation_{};
    std::array<double, 3> debug_translation_{};
};
