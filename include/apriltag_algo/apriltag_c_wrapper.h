#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 用不透明指针(Opaque Pointer)代替 C++ 的 class 实例
typedef void* apriltag_align_handle_t;

// C 风格的输入图像结构体
typedef struct {
    uint8_t* data;  // 图像数据指针
    int width;      // 图像宽度
    int height;     // 图像高度
    int stride;     // 图像步长（每行字节数）
} apriltag_img_u8_t;

// C 风格的裁剪框结构体
typedef struct {
    int x;      // 裁剪框左上角 x 坐标
    int y;      // 裁剪框左上角 y 坐标
    int width;  // 裁剪框宽度
    int height; // 裁剪框高度
} apriltag_crop_rect_t;

// 因为 C 语言没有 unordered_map，我们用结构体数组来传递初始化参数
typedef struct {
    int id;
    double length;
    double shiftX;
    double shiftY;
} apriltag_param_t;

// 1. 创建实例 (对应 C++ 的构造函数)
apriltag_align_handle_t apriltag_align_create(
    double fx, double fy, double cx, double cy,
    const apriltag_param_t* params, int param_count,
    bool debug);

// 2. 销毁实例 (对应 C++ 的析构函数)
void apriltag_align_destroy(apriltag_align_handle_t handle);

// 3. 执行对齐算法 (对应 C++ 的 align 成员函数)
bool apriltag_align_process(void* handle, 
                            const apriltag_img_u8_t* small_img, 
                            float scale_x, 
                            float scale_y, 
                            int offset_x, 
                            int offset_y);

// 4. 获取结果接口
bool apriltag_align_is_found(apriltag_align_handle_t handle);
int apriltag_align_get_tag_id(apriltag_align_handle_t handle);
double apriltag_align_get_shift_x(apriltag_align_handle_t handle);
double apriltag_align_get_shift_y(apriltag_align_handle_t handle);
double apriltag_align_get_shift_z(apriltag_align_handle_t handle);
double apriltag_align_get_yaw(apriltag_align_handle_t handle);

#ifdef __cplusplus
}
#endif