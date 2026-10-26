#include "apriltag_algo.h"
#include "apriltag_c_wrapper.h"

extern "C" {

apriltag_align_handle_t apriltag_align_create(
    double fx, double fy, double cx, double cy,
    const apriltag_param_t* params, int param_count,
    bool debug)
{
    // 把 C 的结构体数组，转换成 C++ 需要的 unordered_map
    std::unordered_map<int, TagParam> param_map;
    for (int i = 0; i < param_count; ++i) {
        TagParam p;
        p.length = params[i].length;
        p.shiftX = params[i].shiftX;
        p.shiftY = params[i].shiftY;
        param_map[params[i].id] = p;
    }

    // new 出 C++ 对象，并强转为 void* 句柄返回给 C
    ApriltagAlignment* aligner = new ApriltagAlignment(fx, fy, cx, cy, param_map, debug);
    return static_cast<apriltag_align_handle_t>(aligner);
}

void apriltag_align_destroy(apriltag_align_handle_t handle) {
    if (handle) {
        delete static_cast<ApriltagAlignment*>(handle);
    }
}

bool apriltag_align_process(apriltag_align_handle_t handle, 
                            const apriltag_img_u8_t* img, 
                            const apriltag_crop_rect_t* bbox) 
{
    if (!handle || !img || !bbox) return false;

    ApriltagAlignment* aligner = static_cast<ApriltagAlignment*>(handle);
    
    // C struct 映射为 C++ struct
    ImageU8 cpp_img = {img->data, img->width, img->height, img->stride};
    CropRect cpp_bbox = {bbox->x, bbox->y, bbox->width, bbox->height};

    return aligner->align(cpp_img, cpp_bbox);
}

bool apriltag_align_is_found(apriltag_align_handle_t handle) {
    return handle ? static_cast<ApriltagAlignment*>(handle)->is_found() : false;
}

int apriltag_align_get_tag_id(apriltag_align_handle_t handle) {
    return handle ? static_cast<ApriltagAlignment*>(handle)->get_tag_id() : -1;
}

double apriltag_align_get_shift_x(apriltag_align_handle_t handle) {
    return handle ? static_cast<ApriltagAlignment*>(handle)->get_shift_x() : 0.0;
}

double apriltag_align_get_shift_y(apriltag_align_handle_t handle) {
    return handle ? static_cast<ApriltagAlignment*>(handle)->get_shift_y() : 0.0;
}

double apriltag_align_get_shift_z(apriltag_align_handle_t handle) {
    return handle ? static_cast<ApriltagAlignment*>(handle)->get_shift_z() : 0.0;
}

double apriltag_align_get_yaw(apriltag_align_handle_t handle) {
    return handle ? static_cast<ApriltagAlignment*>(handle)->get_yaw() : 0.0;
}

} // extern "C"