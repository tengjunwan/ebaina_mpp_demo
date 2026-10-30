/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
#include "sample_svp_npu_process.h"
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>
#include "svp_acl_rt.h"
#include "svp_acl.h"
#include "svp_acl_ext.h"
#include "ss_mpi_sys.h"
#include "sample_common_svp.h"
#include "sample_common_svp_npu.h"
#include "sample_common_svp_npu_model.h"

#include "ot_common_video.h"
#include "ot_common_vgs.h"
#include "ss_mpi_vgs.h"
// #include "ss_mpi_sys.h"

#include <sys/time.h>
#include <math.h>       // 必须包含 math.h 以使用 sqrt()
#include <sys/param.h>  // 使用 MAX, MIN 宏

// for capture frame
// #include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// #include <unistd.h>
#include <string.h>

#include "rtsp_server_api.h"

#include "apriltag_c_wrapper.h" // wtjdebug: 这个头文件是我自己封装的 C 接口，方便在 C 语言中调用 C++ 的 AprilTag 算法类

#define SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM 1
#define SAMPLE_SVP_NPU_LSTM_INPUT_FILE_NUM     2
#define SAMPLE_SVP_NPU_SHAERD_WORK_BUF_NUM     1
#define SAMPLE_SVP_NPU_YOLO_TYPE_NUM           12
#define SAMPLE_SVP_NPU_YOLO_THRESHOLD_NUM      1
#define SAMPLE_SVP_NPU_PREEMPTION_SLEEP_SECOND    30
#define SAMPLE_SVP_NPU_PATH_LEN 0x100

#define SVP_NPU_VO_WIDTH    1920
#define SVP_NPU_VO_HEIGHT   1080

#define USE_NEW_RPN_MODEL 1
// 预设的固定 Resize 目标尺寸，正方形 64x64
#define TAG_VGS_SIZE 64

// ==============================================================================
// 全局/静态变量：用于存放 VGS 输出的 64x64 小图 (避免在循环里频繁 malloc/free)
// ==============================================================================
static ot_video_frame_info g_vgs_64x64_frame = {0};
static td_bool g_vgs_buf_inited = TD_FALSE;

// ======capture frame related(start)======
// 标定用的 PC 端 IP 和接收端口（后续在 PC 写个 Python 脚本监听该端口）
#define PC_CALIB_IP "192.168.1.100" 
#define PC_CALIB_PORT 8888
#define BOARD_TRIGGER_PORT 9999 // 板端监听抓图指令的 UDP 端口

// 异步抓图全局控制变量
static td_bool g_trigger_capture = TD_FALSE;
static td_u8 *g_capture_buf = TD_NULL;
static td_u32 g_capture_size = 0;

// 线程同步机制
static pthread_mutex_t g_capture_mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t g_capture_sem;
// ======capture frame related(end)======

static ot_vb_pool_info g_svp_npu_vb_pool_info;
static td_void *g_svp_npu_vb_virt_addr = TD_NULL;
static ot_sample_svp_coords_info g_svp_npu_coords_info = {0};
static td_bool g_svp_npu_is_thread_start = TD_FALSE;
static td_bool g_svp_npu_terminate_signal = TD_FALSE;
static sample_svp_npu_shared_work_buf g_svp_npu_shared_work_buf[SAMPLE_SVP_NPU_SHAERD_WORK_BUF_NUM] = {0};
static pthread_t g_svp_npu_thread = 0;
static sample_svp_npu_task_info g_svp_npu_task[SAMPLE_SVP_NPU_MAX_TASK_NUM] = {0};
// 局部全局变量，持有 AprilTag 算法实例
static apriltag_align_handle_t g_apriltag_handle = TD_NULL;

// 放在函数外部的辅助宏，用于快速计算两个 timeval 之间的毫秒差值
#define CALC_TIME_MS(start, end) \
    ((end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0)

// wtjdebug: defined in sample_common_svp_npu.h
// typedef struct {
//     td_float nms_threshold;
//     td_float score_threshold;
//     td_float min_height;
//     td_float min_width;
//     td_char *name;
// } sample_svp_npu_threshold;

// static sample_svp_npu_threshold g_svp_npu_yolo_threshold[SAMPLE_SVP_NPU_YOLO_TYPE_NUM] = {
//     {0.0, 0.0, 0.0, 0.0, "rpn_data"},  // reserve
//     {0.5, 0.2, 1.0, 1.0, "rpn_data"},  // yolov1
//     {0.3, 0.5, 1.0, 1.0, "rpn_data"},  // yolov2
//     {0.45, 0.5, 1.0, 1.0, "rpn_data"}, // yolov3
//     {0.45, 0.5, 1.0, 1.0, "rpn_data"}, // yolov4
//     {0.8, 0.8, 1.0, 1.0, "rpn_data"}, // yolov5
//     {0.0, 0.0, 0.0, 0.0, "rpn_data"},  // reserve
//     {0.45, 0.5, 1.0, 1.0, "rpn_data"}, // yolov7
//     {0.8, 0.8, 1.0, 1.0, "rpn_data"},  // yolov8
//     {0.0, 0.0, 0.0, 0.0, "rpn_data"}   // reserve
//     };

static sample_svp_npu_threshold g_svp_npu_yolo_threshold[SAMPLE_SVP_NPU_YOLO_TYPE_NUM] = {
    {0.0, 0.0, 0.0, 0.0, "rpn_data"},  // reserve
    {0.5, 0.2, 1.0, 1.0, "rpn_data"},  // yolov1
    {0.3, 0.5, 1.0, 1.0, "rpn_data"},  // yolov2
    {0.45, 0.5, 1.0, 1.0, "rpn_data"}, // yolov3
    {0.45, 0.5, 1.0, 1.0, "rpn_data"}, // yolov4
    {0.8, 0.8, 1.0, 1.0, "rpn_data"}, // yolov5
    {0.0, 0.0, 0.0, 0.0, "rpn_data"},  // reserve
    {0.45, 0.5, 1.0, 1.0, "rpn_data"}, // yolov7
    {0.75, 0.85, 1.0, 1.0, "rpn_data"},  // yolov8: nms_thresh, score_thresh, min_height, min_width
    {0.0, 0.0, 0.0, 0.0, "rpn_data"}   // reserve
    };

static sample_svp_npu_roi_info g_svp_npu_yolo_roi_info[SAMPLE_SVP_NPU_YOLO_TYPE_NUM] = {
    {"reserve", "reserve"},  // reserve
    {"yolov1_nms", "yolov1_nms_"},  // yolov1
    {"DetectionOut0_nms_3", "DetectionOut0_nms_3_"},  // yolov2
    {"detection_nms_2", "detection_nms_2_"},  // yolov3
    {"detection_nms_2", "detection_nms_2_"},  // yolov4
    {"output0_0", "output0_"},  // yolov5
    {"reserve", "reserve"},  // reserve
    {"output", "output_"},  // yolov7
    {"output0_", "output0_0"},  // yolov8
    {"reserve", "reserve"},  // reserve
    };

static td_s32 g_svp_npu_dev_id = 0;
// wtjdebug: defined in sample_comm.h
// typedef struct {
//     sample_sns_info     sns_info;
//     sample_mipi_info    mipi_info;
//     sample_vi_dev_info  dev_info;
//     ot_vi_bind_pipe     bind_pipe;
//     sample_vi_grp_info  grp_info;
//     sample_vi_pipe_info pipe_info[OT_VI_MAX_PHYS_PIPE_NUM];
// } sample_vi_cfg;
static sample_vi_cfg g_vi_config;
static ot_sample_svp_rect_info g_svp_npu_rect_info = {0};
static td_bool g_svp_npu_thread_stop = TD_FALSE;
static td_bool g_svp_npu_rtsp_enable = TD_FALSE;


// wtj debug: defined in sample_common_svp.c
// there are 2 channel setting for video input, OT_SVP_MAX_VPSS_CHN_NUM=2
// typedef struct {
//     ot_sample_svp_switch svp_switch;
//     ot_size pic_size[OT_SVP_MAX_VPSS_CHN_NUM];
//     ot_pic_size pic_type[OT_SVP_MAX_VPSS_CHN_NUM];
//     ot_vb_pool vb_pool[OT_SVP_MAX_VPSS_CHN_NUM];
//     td_u32 chn_num;
// } ot_sample_svp_media_cfg;

static ot_sample_svp_media_cfg g_svp_npu_media_cfg = {
    .svp_switch = {TD_TRUE},
    .pic_type = {PIC_2560X1440, PIC_CIF},  // wtjdebug: 1st channel is 2K res, 2nd channel is set to PIC_BUTT
    .chn_num = OT_SVP_MAX_VPSS_CHN_NUM,
};


// 在系统启动或第一次进入检测时调用
static void init_vgs_apriltag_buffer(const ot_video_frame_info *base_frame) {
    if (g_vgs_buf_inited) return;

    // 为 YUV420SP 格式预留足够的空间 (宽 * 高 * 1.5)
    td_u32 target_size = TAG_VGS_SIZE * TAG_VGS_SIZE * 3 / 2; 
    
    // 【修改点】：换成 ss_mpi_sys_mmz_alloc
    ss_mpi_sys_mmz_alloc(&g_vgs_64x64_frame.video_frame.phys_addr[0], 
                         (td_void**)&g_vgs_64x64_frame.video_frame.virt_addr[0], 
                         "vgs_tag_buf", TD_NULL, target_size);
    
    g_vgs_64x64_frame.video_frame.width = TAG_VGS_SIZE;
    g_vgs_64x64_frame.video_frame.height = TAG_VGS_SIZE;
    g_vgs_64x64_frame.video_frame.stride[0] = TAG_VGS_SIZE;
    g_vgs_64x64_frame.video_frame.stride[1] = TAG_VGS_SIZE;
    
    // 【绝杀技巧】：直接继承输入原图的像素格式，绕开未定义的宏！
    g_vgs_64x64_frame.video_frame.pixel_format = base_frame->video_frame.pixel_format; 
    
    // 计算 UV 分量的物理与虚拟地址偏移
    td_u32 y_size = TAG_VGS_SIZE * TAG_VGS_SIZE;
    g_vgs_64x64_frame.video_frame.phys_addr[1] = g_vgs_64x64_frame.video_frame.phys_addr[0] + y_size;
    g_vgs_64x64_frame.video_frame.virt_addr[1] = (td_void*)((td_u8*)g_vgs_64x64_frame.video_frame.virt_addr[0] + y_size);

    g_vgs_buf_inited = TD_TRUE;
    printf("[AprilTag] VGS 64x64 buffer allocated successfully.\n");
}

// ====frame capture related(start)====
static void* thread_tcp_sender(void* arg) {
    while (1) {
        // 阻塞等待 SVP 拦截器发来的唤醒信号
        sem_wait(&g_capture_sem); 

        pthread_mutex_lock(&g_capture_mutex);
        if (g_capture_buf != TD_NULL && g_capture_size > 0) {
            
            // 建立 TCP 客户端套接字
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock >= 0) {
                struct sockaddr_in serv_addr;
                memset(&serv_addr, 0, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(PC_CALIB_PORT);
                inet_pton(AF_INET, PC_CALIB_IP, &serv_addr.sin_addr);

                printf("[Capture] Connecting to PC %s:%d...\n", PC_CALIB_IP, PC_CALIB_PORT);
                if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
                    // 发送 YUV 二进制流
                    ssize_t sent = send(sock, g_capture_buf, g_capture_size, 0);
                    printf("[Capture] Success! Sent %zd bytes to PC.\n", sent);
                } else {
                    printf("[Capture] Failed to connect to PC. Is Python server running?\n");
                }
                close(sock);
            }
        }
        pthread_mutex_unlock(&g_capture_mutex);
    }
    return NULL;
}

static void* thread_udp_trigger(void* arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in board_addr, client_addr;
    memset(&board_addr, 0, sizeof(board_addr));
    board_addr.sin_family = AF_INET;
    board_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    board_addr.sin_port = htons(BOARD_TRIGGER_PORT);

    bind(sock, (struct sockaddr*)&board_addr, sizeof(board_addr));
    printf("[Capture] UDP Trigger listening on port %d. Send 'CAP' to capture.\n", BOARD_TRIGGER_PORT);

    char buffer[64];
    socklen_t len = sizeof(client_addr);
    while (1) {
        ssize_t n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client_addr, &len);
        if (n > 0) {
            buffer[n] = '\0';
            // 如果收到的字符串包含 "CAP" (忽略换行符等)
            if (strstr(buffer, "CAP") != NULL) {
                printf("[Capture] Received trigger command!\n");
                // 置起全局标志位，SVP 线程会在下一帧路过时自动拦截
                g_trigger_capture = TD_TRUE; 
            }
        }
    }
    close(sock);
    return NULL;
}

// 留在 sample_svp_npu_process.c 中
void sample_svp_capture_threads_init(void) {
    sem_init(&g_capture_sem, 0, 0);

    pthread_t tid_sender, tid_trigger;
    // 创建后台线程
    pthread_create(&tid_sender, NULL, thread_tcp_sender, NULL);
    pthread_create(&tid_trigger, NULL, thread_udp_trigger, NULL);
    
    // 设置线程分离（可选），防止主线程退出时产生僵尸线程
    pthread_detach(tid_sender);
    pthread_detach(tid_trigger);
    
    printf("[Capture] Capture threads initialized successfully.\n");
}

// ====frame capture related(end)====

// =========================================================================
// [修改] 你的主处理函数
// =========================================================================
static void sample_apriltag_proc(const ot_video_frame_info *base_frame, 
                                 ot_sample_svp_rect_info *rect_info) 
{
    if (g_apriltag_handle == TD_NULL || base_frame == TD_NULL || rect_info == TD_NULL || rect_info->num == 0) {
        return;
    }

    // 1. 初始化：进入推理前，将所有候选框的 AprilTag 状态清空为 TD_FALSE
    for (td_u32 i = 0; i < rect_info->num; i++) {
        rect_info->is_found_apriltag[i] = TD_FALSE;
    }

    // [删除点]：不需要对 base_frame 进行整图的 mmap 映射了！
    // 确保 VGS 目标内存已初始化
    init_vgs_apriltag_buffer(base_frame);

    // 2. 顺序遍历（上游 NMS 已按 Score 降序排好）
    for (td_u32 i = 0; i < rect_info->num; i++) {
        td_u32 x_min = rect_info->rect[i].point[0].x;
        td_u32 y_min = rect_info->rect[i].point[0].y;
        td_u32 x_max = rect_info->rect[i].point[2].x;
        td_u32 y_max = rect_info->rect[i].point[2].y;
        
        float w = (float)(x_max - x_min);
        float h = (float)(y_max - y_min);
        float cx = (float)x_min + w / 2.0f;
        float cy = (float)y_min + h / 2.0f;
        
        // 使用几何平均数保持完美正方形，并 1.2 倍膨胀
        float crop_size = sqrtf(w * h) * 1.2f;
        float rx = cx - crop_size / 2.0f;
        float ry = cy - crop_size / 2.0f;
        
        // VGS 硬件对齐：X向下取16倍数，Y向下取2倍数
        #define MAX_VAL(a, b) ((a) > (b) ? (a) : (b))
        
        int aligned_x = ((int)MAX_VAL(rx, 0.0f)) & (~15);
        int aligned_y = ((int)MAX_VAL(ry, 0.0f)) & (~1);
        
        int shift_x = (int)rx - aligned_x;
        int shift_y = (int)ry - aligned_y;
        
        int aligned_w = ((int)crop_size + MAX_VAL(0, shift_x) + 15) & (~15);
        int aligned_h = ((int)crop_size + MAX_VAL(0, shift_y) + 15) & (~15);
        
        #undef MAX_VAL
        
        // 越界保护（强制转换为 int 消除 signedness 警告）
        if (aligned_x + aligned_w > (int)base_frame->video_frame.width) {
            aligned_w = ((int)base_frame->video_frame.width - aligned_x) & (~15);
        }
        if (aligned_y + aligned_h > (int)base_frame->video_frame.height) {
            aligned_h = ((int)base_frame->video_frame.height - aligned_y) & (~1);
        }

        // ======================= 调度 VGS 硬件 =======================
        ot_vgs_handle vgs_handle = -1;
        ot_vgs_task_attr vgs_task = {0};

        vgs_task.img_in = *base_frame; 
        // 物理地址截流：计算截取起点的物理内存偏移
        td_u64 phys_offset_y = aligned_y * base_frame->video_frame.stride[0] + aligned_x;
        td_u64 phys_offset_uv = (aligned_y / 2) * base_frame->video_frame.stride[1] + aligned_x; 

        vgs_task.img_in.video_frame.phys_addr[0] += phys_offset_y;
        vgs_task.img_in.video_frame.phys_addr[1] += phys_offset_uv;

        // 欺骗 VGS 让其以为原图只有框这么大
        vgs_task.img_in.video_frame.width = aligned_w;
        vgs_task.img_in.video_frame.height = aligned_h;
        vgs_task.img_out = g_vgs_64x64_frame;

        // --- 开始计时 ---
        struct timeval tv_start, tv_end;
        gettimeofday(&tv_start, NULL);

        // 执行 VGS 硬件零拷贝裁切 + 缩放
        ss_mpi_vgs_begin_job(&vgs_handle);
        ss_mpi_vgs_add_scale_task(vgs_handle, &vgs_task, 0); 
        ss_mpi_vgs_end_job(vgs_handle);

        // 刷新 Cache，让 CPU 读取最新抠出的 64x64 小图
        ss_mpi_sys_flush_cache(g_vgs_64x64_frame.video_frame.phys_addr[0], 
                               g_vgs_64x64_frame.video_frame.virt_addr[0], 
                               TAG_VGS_SIZE * TAG_VGS_SIZE);

        // --- 结束计时 ---
        gettimeofday(&tv_end, NULL);
        double vgs_time_ms = (tv_end.tv_sec - tv_start.tv_sec) * 1000.0 + (tv_end.tv_usec - tv_start.tv_usec) / 1000.0;
        
        printf("[wtjdebug] VGS Hardware Crop+Resize (to 64x64) Cost: %.3f ms\n", vgs_time_ms);
        // =============================================================

        // ======================= 传递给算法 =======================
        apriltag_img_u8_t vgs_small_img = {
            .data   = (td_u8*)g_vgs_64x64_frame.video_frame.virt_addr[0],
            .width  = TAG_VGS_SIZE,
            .height = TAG_VGS_SIZE,
            .stride = TAG_VGS_SIZE
        };

        float scale_x = (float)aligned_w / (float)TAG_VGS_SIZE;
        float scale_y = (float)aligned_h / (float)TAG_VGS_SIZE;

        printf("[wtjdebug] Checking %d-th rect (VGS Hardware Size: %d x %d\n)...", i, aligned_w, aligned_h);
        
        // 调用 C Wrapper 接口 (注意参数已经变了)
        bool is_processed = apriltag_align_process(g_apriltag_handle, &vgs_small_img, scale_x, scale_y, aligned_x, aligned_y);

        // 3. Early Exit
        if (is_processed && apriltag_align_is_found(g_apriltag_handle)) {
            int tag_id = apriltag_align_get_tag_id(g_apriltag_handle);
            double tx = apriltag_align_get_shift_x(g_apriltag_handle);
            double ty = apriltag_align_get_shift_y(g_apriltag_handle);
            double tz = apriltag_align_get_shift_z(g_apriltag_handle);
            double yaw = apriltag_align_get_yaw(g_apriltag_handle);
            double yaw_degree = yaw * 180.0 / 3.1415926;   
            
            printf("\n[wtjdebug] Target Locked! Score: %.2f, Tag_ID: %d | Pose: %.3f, %.3f, %.3f | Yaw: %.3f deg\n", 
                   rect_info->score[i], tag_id, tx, ty, tz, yaw_degree);
            
            rect_info->is_found_apriltag[i] = TD_TRUE; 
            break; 
        }
    }
}

td_void sample_svp_npu_enable_rtsp(td_u32 enable)
{
    if (enable == 1) {
        g_svp_npu_rtsp_enable = TD_TRUE;
    } else {
        g_svp_npu_rtsp_enable = TD_FALSE;
    }
    printf("[wtjdebug][sample_svp_npu_process][sample_svp_npu_enable_rtsp]: g_svp_npu_rtsp_enable = %d\n", g_svp_npu_rtsp_enable);
}

ot_sample_svp_media_cfg *sample_svp_npu_get_media_cfg(td_void)
{
    return &g_svp_npu_media_cfg;
}

td_s32 *sample_svp_npu_get_device_id(td_void)
{
    return &g_svp_npu_dev_id;
}

ot_sample_svp_rect_info *sample_svp_npu_get_svp_rect_info(td_void)
{
    return &g_svp_npu_rect_info;
}

td_bool *sample_svp_npu_get_thread_stop(td_void)
{
    return &g_svp_npu_thread_stop;
}

sample_svp_npu_task_info *sample_svp_npu_get_task_info(int task_idx)
{
    return &g_svp_npu_task[task_idx];
}

static td_void sample_svp_npu_acl_terminate(td_void)
{
    if (g_svp_npu_rtsp_enable){
        rtsp_server_stop();
    }

    if (g_svp_npu_terminate_signal == TD_TRUE) {
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
}

/* function : svp npu signal handle */
td_void sample_svp_npu_acl_handle_sig(td_void)
{
    g_svp_npu_terminate_signal = TD_TRUE;
}

static td_void sample_svp_npu_acl_deinit(td_void)
{
    svp_acl_error ret;

    ret = svp_acl_rt_reset_device(g_svp_npu_dev_id);
    if (ret != SVP_ACL_SUCCESS) {
        sample_svp_trace_err("reset device fail\n");
    }
    sample_svp_trace_info("end to reset device is %d\n", g_svp_npu_dev_id);

    ret = svp_acl_finalize();
    if (ret != SVP_ACL_SUCCESS) {
        sample_svp_trace_err("finalize acl fail\n");
    }
    sample_svp_trace_info("end to finalize acl\n");
    (td_void)sample_common_svp_check_sys_exit();
}

static td_s32 sample_svp_npu_acl_init(const td_char *acl_config_path)
{
    /* svp acl init */
    svp_acl_rt_run_mode run_mode;
    svp_acl_error ret;
    td_bool is_mpi_init;

    is_mpi_init = sample_common_svp_check_sys_init();
    sample_svp_check_exps_return(is_mpi_init != TD_TRUE, TD_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR, "mpi init failed!\n");

    ret = svp_acl_init(acl_config_path);
    sample_svp_check_exps_return(ret != SVP_ACL_SUCCESS, TD_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR, "acl init failed!\n");

    sample_svp_trace_info("svp acl init success!\n");

    /* open device */
    ret = svp_acl_rt_set_device(g_svp_npu_dev_id);
    if (ret != SVP_ACL_SUCCESS) {
        (td_void)svp_acl_finalize();
        sample_svp_trace_err("svp acl open device %d failed!\n", g_svp_npu_dev_id);
        return TD_FAILURE;
    }
    sample_svp_trace_info("open device %d success!\n", g_svp_npu_dev_id);

    /* get run mode */
    ret = svp_acl_rt_get_run_mode(&run_mode);
    if ((ret != SVP_ACL_SUCCESS) || (run_mode != SVP_ACL_DEVICE)) {
        (td_void)svp_acl_rt_reset_device(g_svp_npu_dev_id);
        (td_void)svp_acl_finalize();
        sample_svp_trace_err("acl get run mode failed!\n");
        return TD_FAILURE;
    }
    sample_svp_trace_info("get run mode success!\n");

    printf("[wtjdebug][sample_svp_npu_process][sample_svp_npu_acl_init]: g_svp_npu_dev_id = %u\n",
        g_svp_npu_dev_id); // wtjdebug
    printf("[wtjdebug][sample_svp_npu_process][sample_svp_npu_acl_init]: run_mode = %u\n",
        run_mode); // wtjdebug: SVP_ACL_DEVICE=0, SVP_ACL_HOST=1

    return TD_SUCCESS;
}

static td_s32 sample_svp_npu_acl_dataset_init(td_u32 task_idx)
{
    td_s32 ret = sample_common_svp_npu_create_input(&g_svp_npu_task[task_idx]);
    sample_svp_check_exps_return(ret != TD_SUCCESS, TD_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR, "create input failed!\n");

    ret = sample_common_svp_npu_create_output(&g_svp_npu_task[task_idx]);
    if (ret != TD_SUCCESS) {
        sample_common_svp_npu_destroy_input(&g_svp_npu_task[task_idx]);
        sample_svp_trace_err("execute create output fail.\n");
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

static td_void sample_svp_npu_acl_dataset_deinit(td_u32 task_idx)
{
    (td_void)sample_common_svp_npu_destroy_input(&g_svp_npu_task[task_idx]);
    (td_void)sample_common_svp_npu_destroy_output(&g_svp_npu_task[task_idx]);
}

static td_void *sample_svp_npu_acl_thread_multi_model_process(td_void *args)
{
    td_s32 ret;
    td_u32 task_idx = *(td_u32 *)args;
    td_u32 proc_cnt = 0;
    svp_acl_rt_context context = {0};

    ret = svp_acl_rt_create_context(&context, g_svp_npu_dev_id);
    sample_svp_check_exps_return(ret != TD_SUCCESS, TD_NULL, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "create context failed!\n");

    ret = svp_acl_rt_set_current_context(context);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "set context failed!\n");

    while (g_svp_npu_is_thread_start == TD_TRUE && g_svp_npu_terminate_signal == TD_FALSE) {
        ret = sample_common_svp_npu_model_execute(&g_svp_npu_task[task_idx]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR, "proc sync failed!\n");
        sample_svp_trace_info("model_index:%u complete cnt:%u\n", g_svp_npu_task[task_idx].cfg.model_idx, ++proc_cnt);
    }

process_end0:
    ret = svp_acl_rt_destroy_context(context);
    if (ret != SVP_ACL_SUCCESS) {
        sample_svp_trace_err("task[%u] destroy context failed!\n", task_idx);
    }
    return TD_NULL;
}

static td_void *sample_svp_npu_acl_thread_execute(td_void *args)
{
    td_s32 ret;
    td_u32 task_idx = *(td_u32 *)args;

    ret = svp_acl_rt_set_device(g_svp_npu_dev_id);
    sample_svp_check_exps_return(ret != TD_SUCCESS, TD_NULL, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "open device %d failed!\n", g_svp_npu_dev_id);

    ret = sample_common_svp_npu_model_execute(&g_svp_npu_task[task_idx]);
    if (ret != SVP_ACL_SUCCESS) {
        sample_svp_trace_err("execute inference failed of task[%u]!\n", task_idx);
    }

    ret = svp_acl_rt_reset_device(g_svp_npu_dev_id);
    if (ret != SVP_ACL_SUCCESS) {
        sample_svp_trace_err("task[%u] reset device failed!\n", task_idx);
    }
    return TD_NULL;
}

static td_void sample_svp_npu_acl_model_execute_multithread(td_void)
{
    pthread_t execute_threads[SAMPLE_SVP_NPU_MAX_THREAD_NUM] = {0};
    td_u32 idx[SAMPLE_SVP_NPU_MAX_THREAD_NUM] = {0};
    td_u32 task_idx;

    for (task_idx = 0; task_idx < SAMPLE_SVP_NPU_MAX_THREAD_NUM; task_idx++) {
        idx[task_idx] = task_idx;
        pthread_create(&execute_threads[task_idx], NULL, sample_svp_npu_acl_thread_execute, &idx[task_idx]);
    }

    td_void *waitret[SAMPLE_SVP_NPU_MAX_THREAD_NUM];
    for (task_idx = 0; task_idx < SAMPLE_SVP_NPU_MAX_THREAD_NUM; task_idx++) {
        pthread_join(execute_threads[task_idx], &waitret[task_idx]);
    }

    for (task_idx = 0; task_idx < SAMPLE_SVP_NPU_MAX_THREAD_NUM; task_idx++) {
        sample_svp_trace_info("output %u-th task data\n", task_idx);
        sample_common_svp_npu_output_classification_result(&g_svp_npu_task[task_idx]);
    }
}

static td_void sample_svp_npu_acl_deinit_task(td_u32 task_num, td_u32 shared_work_buf_idx)
{
    td_u32 task_idx;

    for (task_idx = 0; task_idx < task_num; task_idx++) {
        (td_void)sample_common_svp_npu_destroy_work_buf(&g_svp_npu_task[task_idx]);
        (td_void)sample_common_svp_npu_destroy_task_buf(&g_svp_npu_task[task_idx]);
        (td_void)sample_svp_npu_acl_dataset_deinit(task_idx);
        (td_void)memset_s(&g_svp_npu_task[task_idx], sizeof(sample_svp_npu_task_cfg), 0,
            sizeof(sample_svp_npu_task_cfg));
    }
    if (g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr != TD_NULL) {
        (td_void)svp_acl_rt_free(g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr);
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr = TD_NULL;
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size = 0;
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_stride = 0;
    }
}

static td_s32 sample_svp_npu_acl_create_shared_work_buf(td_u32 task_num, td_u32 shared_work_buf_idx)
{
    td_u32 task_idx, work_buf_size, work_buf_stride;
    td_s32 ret;

    for (task_idx = 0; task_idx < task_num; task_idx++) {
        ret = sample_common_svp_npu_get_work_buf_info(&g_svp_npu_task[task_idx], &work_buf_size, &work_buf_stride);
        sample_svp_check_exps_return(ret != TD_SUCCESS, ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "get %u-th task work buf info failed!\n", task_idx);

        if (g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size < work_buf_size) {
            g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size = work_buf_size;
            g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_stride = work_buf_stride;
        }
    }
    ret = svp_acl_rt_malloc_cached(&g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr,
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size, SVP_ACL_MEM_MALLOC_NORMAL_ONLY);
    sample_svp_check_exps_return(ret != TD_SUCCESS, TD_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "malloc %u-th shared work buf failed!\n", shared_work_buf_idx);

    (td_void)svp_acl_rt_mem_flush(g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_ptr,
        g_svp_npu_shared_work_buf[shared_work_buf_idx].work_buf_size);
    return TD_SUCCESS;
}

static td_s32 sample_svp_npu_acl_init_task(td_u32 task_num, td_bool is_share_work_buf,
    td_u32 shared_work_buf_idx)
{
    td_u32 task_idx;
    td_s32 ret;
    // wtjdebug: create input/output dataset and its data buffers
    printf("\n[wtjdebug][sample_svp_npu_process][sample_svp_npu_acl_init_task]: ================\n");
    printf("[wtjdebug]: task_num = %u\n", 
            task_num); // wtjdebug: 1
    printf("[wtjdebug]: is_share_work_buf = %u\n", 
            is_share_work_buf); // wtjdebug: 0
    printf("[wtjdebug]: shared_work_buf_idx = %u\n", 
            shared_work_buf_idx); // wtjdebug: 0
    printf("\n");

    if (is_share_work_buf == TD_TRUE) {
        ret = sample_svp_npu_acl_create_shared_work_buf(task_num, shared_work_buf_idx);
        sample_svp_check_exps_return(ret != TD_SUCCESS, TD_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "create shared work buf failed!\n");
    }

    for (task_idx = 0; task_idx < task_num; task_idx++) {
        ret = sample_svp_npu_acl_dataset_init(task_idx);
        if (ret != TD_SUCCESS) {
            goto task_init_end_0;
        }
        ret = sample_common_svp_npu_create_task_buf(&g_svp_npu_task[task_idx]);
        if (ret != TD_SUCCESS) {
            sample_svp_trace_err("create task buf failed.\n");
            goto task_init_end_0;
        }
        if (is_share_work_buf == TD_FALSE) {
            ret = sample_common_svp_npu_create_work_buf(&g_svp_npu_task[task_idx]);
        } else {
            /* if all tasks are on the same stream, work buf can be shared */
            ret = sample_common_svp_npu_share_work_buf(&g_svp_npu_shared_work_buf[shared_work_buf_idx],
                &g_svp_npu_task[task_idx]);
        }
        if (ret != TD_SUCCESS) {
            sample_svp_trace_err("create work buf failed.\n");
            goto task_init_end_0;
        }
    }
    return TD_SUCCESS;

task_init_end_0:
    (td_void)sample_svp_npu_acl_deinit_task(task_num, shared_work_buf_idx);
    return ret;
}

static td_s32 sample_svp_npu_acl_load_multi_model(const td_char *model_path,
    td_u32 model_num, td_bool is_with_config, const td_char *src_file[], td_u32 file_num)
{
    td_u32 i, j;
    td_s32 ret;

    for (i = 0; i < model_num; i++) {
        if (is_with_config == TD_TRUE) {
            ret = sample_common_svp_npu_load_model_with_config(model_path, i, TD_FALSE);
        } else {
            ret = sample_common_svp_npu_load_model(model_path, i, TD_FALSE);
        }
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "load model failed!\n");

        /* set task cfg */
        g_svp_npu_task[i].cfg.max_batch_num = 1;
        g_svp_npu_task[i].cfg.dynamic_batch_num = 1;
        g_svp_npu_task[i].cfg.total_t = 0;
        g_svp_npu_task[i].cfg.is_cached = TD_FALSE;
        g_svp_npu_task[i].cfg.model_idx = i;
    }

    ret = sample_svp_npu_acl_init_task(model_num, TD_FALSE, 0);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR, "init task failed!\n");

    for (i = 0; i < model_num; i++) {
        ret = sample_common_svp_npu_get_input_data(src_file, file_num, &g_svp_npu_task[i]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_ERROR, "get data failed!\n");
    }

    return TD_SUCCESS;

process_end1:
    (td_void)sample_svp_npu_acl_deinit_task(model_num, 0);

process_end0:
    for (j = 0; j < i; j++) {
        (td_void)sample_common_svp_npu_unload_model(j);
    }

    return ret;
}

static td_void sample_svp_npu_acl_unload_multi_model(td_u32 model_num)
{
    td_u32 i;

    (td_void)sample_svp_npu_acl_deinit_task(model_num, 0);

    for (i = 0; i < model_num; i++) {
        (td_void)sample_common_svp_npu_unload_model(i);
    }
}



static td_s32 sample_svp_npu_acl_frame_proc(const ot_video_frame_info *ext_frame,
    const ot_video_frame_info *base_frame, td_void *args)
{

    // =====================================================================
    // [新增] 异步抓图拦截器：如果在别的线程触发了抓图，在这里极速 Copy
    // =====================================================================
    if (g_trigger_capture == TD_TRUE) {
        printf("here1\n");
        pthread_mutex_lock(&g_capture_mutex);
        
        // 计算 YUV420SP 的真实大小 (Y分量 + UV分量)
        td_u32 y_size = base_frame->video_frame.stride[0] * base_frame->video_frame.height;
        td_u32 uv_size = base_frame->video_frame.stride[1] * base_frame->video_frame.height / 2;
        td_u32 total_size = y_size + uv_size;

        // 懒加载：只在第一次抓图时分配内存
        if (g_capture_buf == TD_NULL) {
            g_capture_buf = (td_u8 *)malloc(total_size);
            g_capture_size = total_size;
        }

        if (g_capture_buf != TD_NULL) {
            // 直接无视 virt_addr，强制映射真实的物理内存
            td_u8 *y_virt = (td_u8 *)ss_mpi_sys_mmap(base_frame->video_frame.phys_addr[0], y_size);
            td_u8 *uv_virt = (td_u8 *)ss_mpi_sys_mmap(base_frame->video_frame.phys_addr[1], uv_size);
            
            printf("here2\n");
            
            // 极速内存拷贝
            if (y_virt != TD_NULL && uv_virt != TD_NULL) {
                memcpy(g_capture_buf, y_virt, y_size);
                memcpy(g_capture_buf + y_size, uv_virt, uv_size);
                printf("[Capture] YUV frame copied (%u Bytes). Waking up sender...\n", total_size);
                
                // 复位触发标志，唤醒后台发送线程
                g_trigger_capture = TD_FALSE; 
                sem_post(&g_capture_sem); 
            }
            printf("here3\n");

            // 既然强制做了 mmap，用完就必须强制 munmap，防止内存泄漏
            if (y_virt != TD_NULL) ss_mpi_sys_munmap(y_virt, y_size);
            if (uv_virt != TD_NULL) ss_mpi_sys_munmap(uv_virt, uv_size);
        }
        pthread_mutex_unlock(&g_capture_mutex);
    }
    // =====================================================================

    td_s32 ret;
    td_void *virt_addr = TD_NULL;
    sample_svp_npu_detection_info *detection_info = TD_NULL;
    sample_svp_npu_thread_args *thread_args = (sample_svp_npu_thread_args *)args;
    td_u32 size = (td_u32)(ext_frame->video_frame.height * ext_frame->video_frame.stride[0] *
        SAMPLE_SVP_NPU_IMG_THREE_CHN / SAMPLE_SVP_NPU_DOUBLE);

    // [新增] 定义时间戳变量
    struct timeval t_start, t_npu, t_post, t_apriltag, t_vgs;

    virt_addr = g_svp_npu_vb_virt_addr +
        (ext_frame->video_frame.phys_addr[0] - g_svp_npu_vb_pool_info.pool_phy_addr);
    ret = sample_common_svp_npu_update_input_data_buffer_info(virt_addr, size,
        ext_frame->video_frame.stride[0], 0, &g_svp_npu_task[0]);
    sample_svp_check_exps_return(ret != TD_SUCCESS, ret, SAMPLE_SVP_ERR_LEVEL_ERROR, "update data buffer failed!\n");

    // ================== [计时起点] ==================
    gettimeofday(&t_start, NULL); 

    ret = sample_common_svp_npu_model_execute(&g_svp_npu_task[0]);
    sample_svp_check_exps_return(ret != TD_SUCCESS, ret, SAMPLE_SVP_ERR_LEVEL_ERROR, "model execute failed!\n");

    // ================== [打点 1：NPU 推理完成] ==================
    gettimeofday(&t_npu, NULL); 

    if (thread_args->model_name == SAMPLE_SVP_NPU_RFCN || thread_args->model_name == SAMPLE_SVP_NPU_YOLO) {
        detection_info = thread_args->detection_info;
        if (USE_NEW_RPN_MODEL) {
            ret = sample_common_svp_npu_roi_to_rect_for_new_rpn(&g_svp_npu_task[0], detection_info, ext_frame, base_frame,
                &g_svp_npu_rect_info);
        } else {
            ret = sample_common_svp_npu_roi_to_rect(&g_svp_npu_task[0], detection_info, ext_frame, base_frame,
            &g_svp_npu_rect_info);
        }
        sample_svp_check_exps_return(ret != TD_SUCCESS, ret, SAMPLE_SVP_ERR_LEVEL_ERROR, "roi to rect failed!\n");

        // ================== [打点 2：YOLO CPU 后处理完成] ==================
        gettimeofday(&t_post, NULL); 

        // 执行 AprilTag 姿态解算
        sample_apriltag_proc(base_frame, &g_svp_npu_rect_info);

        // ================== [打点 3：AprilTag 解算完成] ==================
        gettimeofday(&t_apriltag, NULL); 

        if (USE_NEW_RPN_MODEL) {
            ret = sample_common_svp_vgs_fill_rect_for_new_rpn(base_frame, &g_svp_npu_rect_info, SAMPLE_SVP_NPU_RECT_COLOR);
        } else {
            ret = sample_common_svp_vgs_fill_rect(base_frame, &g_svp_npu_rect_info, SAMPLE_SVP_NPU_RECT_COLOR);
        }
        sample_svp_check_exps_return(ret != TD_SUCCESS, ret, SAMPLE_SVP_ERR_LEVEL_ERROR, "vgs fill rect failed!\n");

        // ================== [打点 4：VGS 硬件画图完成] ==================
        gettimeofday(&t_vgs, NULL); 

        // [新增] 打印该帧各阶段耗时情况
        printf("[Profile] NPU: %5.2f ms | YOLO_Post: %5.2f ms | AprilTag: %5.2f ms | VGS: %5.2f ms | Total: %5.2f ms\n",
            CALC_TIME_MS(t_start, t_npu),
            CALC_TIME_MS(t_npu, t_post),
            CALC_TIME_MS(t_post, t_apriltag),
            CALC_TIME_MS(t_apriltag, t_vgs),
            CALC_TIME_MS(t_start, t_vgs));

    } else if (thread_args->model_name == SAMPLE_SVP_NPU_HRNET) {
        ret = sample_common_svp_npu_get_joint_coords(&g_svp_npu_task[0], ext_frame, base_frame, &g_svp_npu_coords_info);
        sample_svp_check_exps_return(ret != TD_SUCCESS, ret, SAMPLE_SVP_ERR_LEVEL_ERROR, "roi to joint failed!\n");

        ret = sample_common_svp_vgs_fill_joint_coords_lines(base_frame, &g_svp_npu_coords_info);
        sample_svp_check_exps_return(ret != TD_SUCCESS, ret, SAMPLE_SVP_ERR_LEVEL_ERROR, "vgs fill line failed!\n");
    }
    return ret;
}

static td_s32 sample_svp_npu_acl_vb_map(td_u32 vb_pool_idx)
{
    td_s32 ret;

    if (g_svp_npu_vb_virt_addr == TD_NULL) {
        ret = ss_mpi_vb_get_pool_info(g_svp_npu_media_cfg.vb_pool[vb_pool_idx], &g_svp_npu_vb_pool_info);
        sample_svp_check_exps_return(ret != TD_SUCCESS, TD_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "get pool info failed!\n");
        g_svp_npu_vb_virt_addr = ss_mpi_sys_mmap(g_svp_npu_vb_pool_info.pool_phy_addr,
            g_svp_npu_vb_pool_info.pool_size);
        sample_svp_check_exps_return(g_svp_npu_vb_virt_addr == TD_NULL, TD_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "map vb pool failed!\n");
    }
    return TD_SUCCESS;
}

static td_void *sample_svp_npu_acl_to_venc(td_void *args)
{
    td_s32 ret;
    ot_video_frame_info base_frame, ext_frame;
    const td_s32 milli_sec = SAMPLE_SVP_NPU_MILLIC_SEC;
    const td_s32 vpss_grp = 0;
    td_s32 vpss_chn[] = { OT_VPSS_CHN0, OT_VPSS_CHN1 };
    td_u32 size, stride;
    td_u8 *data = TD_NULL;
    td_s32 frame_index = 0;

    (td_void)prctl(PR_SET_NAME, "svp_npu_to_venc", 0, 0, 0);

    ret = svp_acl_rt_set_device(g_svp_npu_dev_id);
    sample_svp_check_exps_return(ret != TD_SUCCESS, TD_NULL, SAMPLE_SVP_ERR_LEVEL_ERROR, "open device failed!\n");

    ret = sample_svp_npu_acl_vb_map(OT_VPSS_CHN1);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, fail_0, SAMPLE_SVP_ERR_LEVEL_ERROR, "map vb pool failed!\n");

    ret = sample_common_svp_npu_get_input_data_buffer_info(&g_svp_npu_task[0], 0, &data, &size, &stride);  // first input data buffer info
    sample_svp_check_exps_goto(ret != TD_SUCCESS, fail_1, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x),get_input_data_buffer_info failed!\n", ret);

    printf("[wtjdebug][sample_svp_npu_process][sample_svp_npu_acl_to_venc]: ==========");

    printf("[wtjdebug]: g_svp_npu_dev_id = %u\n", 
        g_svp_npu_dev_id); // wtjdebug: 0
    printf("[wtjdebug]: data = %p\n", 
        data); // wtjdebug: pointer to input data buffer "images"
    printf("[wtjdebug]: size = %u\n", 
        size); // wtjdebug: 614400 = 640 * 640 * 1.5 , yuv420 size
    printf("[wtjdebug]: stride = %u\n", 
        stride); // wtjdebug: 640
    printf("[wtjdebug]: milli_sec = %d\n", 
        milli_sec); // wtjdebug: 20000 ms = 2s

    // 在 while 循环外面定义时间变量
    struct timeval start_time, end_time;
    double cost_time_ms = 0.0;

    while (g_svp_npu_thread_stop == TD_FALSE) {
        frame_index++;

        ret = ss_mpi_vpss_get_chn_frame(vpss_grp, vpss_chn[1], &ext_frame, milli_sec);
        sample_svp_check_exps_continue(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error(%#x),ss_mpi_vpss_get_chn_frame failed, vpss_grp(%d), vpss_chn(%d)!\n", ret, vpss_grp, vpss_chn[1]);

        ret = ss_mpi_vpss_get_chn_frame(vpss_grp, vpss_chn[0], &base_frame, milli_sec);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, ext_release, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error(%#x),ss_mpi_vpss_get_chn_frame failed, VPSS_GRP(%d), VPSS_CHN(%d)!\n", ret, vpss_grp, vpss_chn[0]);

        // ================= [新增] 记录开始时间 =================
        gettimeofday(&start_time, NULL);
        // ======================================================

        ret = sample_svp_npu_acl_frame_proc(&ext_frame, &base_frame, args);
        
        // ================= [新增] 记录结束时间并计算耗时 =================
        gettimeofday(&end_time, NULL);
        cost_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + 
                       (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        
        // 每 30 帧打印一次，避免刷屏，同时换算成大概的 FPS
        if (frame_index % 10 == 0) {
            printf("\n[wtjdebug] Frame[%u] Pipeline Cost: %.2f ms (Approx: %.1f FPS)\n", 
                   frame_index, cost_time_ms, 1000.0 / cost_time_ms);
        }
        // ===============================================================

        sample_svp_check_exps_goto(ret != TD_SUCCESS, base_release, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error(%#x),sample_svp_npu_acl_frame_proc failed!\n", ret);

        ret = sample_common_svp_venc_send_stream(&g_svp_npu_media_cfg.svp_switch, 0, &base_frame);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, base_release, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error(%#x),sample_common_svp_venc_send_stream failed!\n", ret);
base_release:
        ret = ss_mpi_vpss_release_chn_frame(vpss_grp, vpss_chn[0], &base_frame);
        sample_svp_check_exps_trace(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error(%#x),release_frame failed,grp(%d) chn(%d)!\n", ret, vpss_grp, vpss_chn[0]);
ext_release:
        ret = ss_mpi_vpss_release_chn_frame(vpss_grp, vpss_chn[1], &ext_frame);
        sample_svp_check_exps_trace(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error(%#x),release_frame failed,grp(%d) chn(%d)!\n", ret, vpss_grp, vpss_chn[1]);
    }
    ret = sample_common_svp_npu_update_input_data_buffer_info(data, size, stride, 0, &g_svp_npu_task[0]);
    sample_svp_check_exps_trace(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR, "update buffer failed!\n");
fail_1:
    (td_void)ss_mpi_sys_munmap(g_svp_npu_vb_virt_addr, g_svp_npu_vb_pool_info.pool_size);
fail_0:
    (td_void)svp_acl_rt_reset_device(g_svp_npu_dev_id);
    return TD_NULL;
}

static td_void sample_svp_npu_acl_pause(td_void)
{
    printf("---------------press Enter key to exit!---------------\n");
    if (g_svp_npu_terminate_signal == TD_TRUE) {
        return;
    }
    (td_void)getchar();
    if (g_svp_npu_terminate_signal == TD_TRUE) {
        return;
    }
}

/* function : show the sample of svp npu resnet50 */
td_void sample_svp_npu_acl_resnet50(td_void)
{
    td_s32 ret;
    const td_u32 model_idx = 0;
    const td_char *acl_config_path = "";
    const td_char *src_file[SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM] = {"./data/image/3_224_224_batch_1.bgr"};
    const td_char *om_model_path = "./data/model/resnet50.om";

    g_svp_npu_terminate_signal = TD_FALSE;

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        /* init acl */
        ret = sample_svp_npu_acl_init(acl_config_path);
        sample_svp_check_exps_return_void(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR, "init failed!\n");

        /* load model */
        ret = sample_common_svp_npu_load_model(om_model_path, model_idx, TD_FALSE);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "load model failed!\n");

        /* set task cfg */
        g_svp_npu_task[0].cfg.max_batch_num = 1;
        g_svp_npu_task[0].cfg.dynamic_batch_num = 1;
        g_svp_npu_task[0].cfg.total_t = 0;
        g_svp_npu_task[0].cfg.is_cached = TD_FALSE;
        g_svp_npu_task[0].cfg.model_idx = model_idx;

        ret = sample_svp_npu_acl_init_task(1, TD_FALSE, 0);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "init task failed!\n");
    }

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        ret = sample_common_svp_npu_get_input_data(src_file, SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM,
            &g_svp_npu_task[0]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR, "get data failed!\n");
    }
    if (g_svp_npu_terminate_signal == TD_FALSE) {
        ret = sample_common_svp_npu_model_execute(&g_svp_npu_task[0]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR, "execute failed!\n");
        (td_void)sample_common_svp_npu_output_classification_result(&g_svp_npu_task[0]);
    }

process_end2:
    (td_void)sample_svp_npu_acl_deinit_task(1, 0);
process_end1:
    (td_void)sample_common_svp_npu_unload_model(model_idx);
process_end0:
    (td_void)sample_svp_npu_acl_deinit();
    (td_void)sample_svp_npu_acl_terminate();
}

td_void sample_svp_npu_acl_resnet50_dynamic_batch(td_void)
{
    td_s32 ret;
    const td_u32 model_idx = 0;
    const td_char *acl_config_path = "";
    const td_char *src_file[SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM] = {"./data/image/3_224_224_batch_8.bgr"};
    const td_char *om_model_path = "./data/model/resnet50.om";

    g_svp_npu_terminate_signal = TD_FALSE;

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        /* init acl */
        ret = sample_svp_npu_acl_init(acl_config_path);
        sample_svp_check_exps_return_void(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR, "init failed!\n");

        /* load model */
        ret = sample_common_svp_npu_load_model(om_model_path, model_idx, TD_TRUE);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "load model failed!\n");

        /* set task cfg */
        g_svp_npu_task[0].cfg.max_batch_num = 8; /* 8 is max batch num, it can't be less than dynamic_batch_num */
        g_svp_npu_task[0].cfg.dynamic_batch_num = 8; /* 8 is batch num of task to be processed */
        g_svp_npu_task[0].cfg.total_t = 0;
        g_svp_npu_task[0].cfg.is_cached = TD_TRUE;
        g_svp_npu_task[0].cfg.model_idx = model_idx;
        ret = sample_svp_npu_acl_init_task(1, TD_FALSE, 0);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_ERROR, "init task failed!\n");
    }

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        ret = sample_common_svp_npu_get_input_data(src_file, SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM,
            &g_svp_npu_task[0]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "get input data failed!\n");
        ret = sample_common_svp_npu_set_dynamic_batch(&g_svp_npu_task[0]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "set dynamic batch failed!\n");
    }
    if (g_svp_npu_terminate_signal == TD_FALSE) {
        ret = sample_common_svp_npu_model_execute(&g_svp_npu_task[0]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR, "execute failed!\n");
        (td_void)sample_common_svp_npu_output_classification_result(&g_svp_npu_task[0]);
    }

process_end2:
    (td_void)sample_svp_npu_acl_deinit_task(1, 0);
process_end1:
    (td_void)sample_common_svp_npu_unload_model(model_idx);
process_end0:
    (td_void)sample_svp_npu_acl_deinit();
    (td_void)sample_svp_npu_acl_terminate();
}

/* function : show the sample of svp npu resnet50 multi thread */
td_void sample_svp_npu_acl_resnet50_multi_thread(td_void)
{
    td_u32 i;
    td_s32 ret;
    const td_u32 model_idx = 0;
    const td_char *acl_config_path = "";
    const td_char *src_file[SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM] = {"./data/image/3_224_224_batch_1.bgr"};
    const td_char *om_model_path = "./data/model/resnet50.om";

    g_svp_npu_terminate_signal = TD_FALSE;

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        /* init acl */
        ret = sample_svp_npu_acl_init(acl_config_path);
        sample_svp_check_exps_return_void(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR, "init failed!\n");

        /* load model */
        ret = sample_common_svp_npu_load_model(om_model_path, model_idx, TD_FALSE);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "load model failed!\n");

        /* set cfg */
        for (i = 0; i < SAMPLE_SVP_NPU_MAX_TASK_NUM; i++) {
            g_svp_npu_task[i].cfg.max_batch_num = 1;
            g_svp_npu_task[i].cfg.dynamic_batch_num = 1;
            g_svp_npu_task[i].cfg.total_t = 0;
            g_svp_npu_task[i].cfg.is_cached = TD_FALSE;
            g_svp_npu_task[i].cfg.model_idx = model_idx;
        }

        ret = sample_svp_npu_acl_init_task(SAMPLE_SVP_NPU_MAX_TASK_NUM, TD_TRUE, 0);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_ERROR, "init task failed!\n");
    }

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        for (i = 0; i < SAMPLE_SVP_NPU_MAX_TASK_NUM; i++) {
            ret = sample_common_svp_npu_get_input_data(src_file, SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM,
                &g_svp_npu_task[i]);
            sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2,
                SAMPLE_SVP_ERR_LEVEL_ERROR, "get %u-th input failed!\n", i);
        }
    }

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        (td_void)sample_svp_npu_acl_model_execute_multithread();
    }

process_end2:
    (td_void)sample_svp_npu_acl_deinit_task(SAMPLE_SVP_NPU_MAX_TASK_NUM, 0);
process_end1:
    (td_void)sample_common_svp_npu_unload_model(model_idx);
process_end0:
    (td_void)sample_svp_npu_acl_deinit();
    (td_void)sample_svp_npu_acl_terminate();
}

td_void sample_svp_npu_acl_lstm(td_void)
{
    td_s32 ret;
    const td_u32 model_idx = 0;
    const td_char *src_file[SAMPLE_SVP_NPU_LSTM_INPUT_FILE_NUM] = {"./data/vector/xt.txt.bin",
        "./data/vector/cont.txt.bin"};
    const td_char *om_model_path = "./data/model/lstm.om";

    g_svp_npu_terminate_signal = TD_FALSE;

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        /* init acl */
        ret = sample_svp_npu_acl_init(TD_NULL);
        sample_svp_check_exps_return_void(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR, "init failed!\n");

        /* load model */
        ret = sample_common_svp_npu_load_model(om_model_path, model_idx, TD_TRUE);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "load model failed!\n");

        /* set task cfg */
        g_svp_npu_task[0].cfg.max_batch_num = 1;
        g_svp_npu_task[0].cfg.dynamic_batch_num = 1;
        g_svp_npu_task[0].cfg.total_t = 3; /* 3 is total t */
        g_svp_npu_task[0].cfg.is_cached = TD_TRUE;
        g_svp_npu_task[0].cfg.model_idx = model_idx;
        ret = sample_svp_npu_acl_init_task(1, TD_FALSE, 0);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_ERROR, "init task failed!\n");
    }

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        ret = sample_common_svp_npu_get_input_data(src_file, SAMPLE_SVP_NPU_LSTM_INPUT_FILE_NUM, &g_svp_npu_task[0]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "get input data failed!\n");
        ret = sample_common_svp_npu_set_dynamic_batch(&g_svp_npu_task[0]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "set dynamic batch failed!\n");
    }
    if (g_svp_npu_terminate_signal == TD_FALSE) {
        ret = sample_common_svp_npu_model_execute(&g_svp_npu_task[0]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR, "execute failed!\n");
    }

process_end2:
    (td_void)sample_svp_npu_acl_deinit_task(1, 0);
process_end1:
    (td_void)sample_common_svp_npu_unload_model(model_idx);
process_end0:
    (td_void)sample_svp_npu_acl_deinit();
    (td_void)sample_svp_npu_acl_terminate();
}

static td_void sample_svp_npu_acl_set_task_info(td_u32 task_idx, td_u32 model_idx)
{
    g_svp_npu_task[task_idx].cfg.max_batch_num = 1;
    g_svp_npu_task[task_idx].cfg.dynamic_batch_num = 1;
    g_svp_npu_task[task_idx].cfg.total_t = 0;
    g_svp_npu_task[task_idx].cfg.is_cached = TD_TRUE;
    g_svp_npu_task[task_idx].cfg.model_idx = model_idx;

    printf("\n[wtjdebug][sample_svp_npu_process][sample_svp_npu_acl_set_task_info]: ============\n"); // wtjdebug
    printf("[wtjdebug]: task_idx = %u\n", 
        task_idx); // wtjdebug
    printf("[wtjdebug]: model_idx = %u\n", 
        model_idx); // wtjdebug
    printf("\n"); // wtjdebug
}

static td_s32 sample_svp_npu_acl_set_input_aipp_info(td_u32 model_index, td_u32 index)
{
    const sample_svp_npu_model_aipp model_aipp = {
        {
            SVP_ACL_YUV420SP_U8,
            1, 1, 0,
            720, 576,       /* src image size */
            1192, 2166, 0,  /* csc coef: yuv420 -> BGR */
            1192, -218, -547,
            1192, 0, 1836,
            0, 0, 0,
            16, 128, 128
        }, {
            1, AIPP_PADDING_SWITCH_REPLICATION, AIPP_RESIZE_SWITCH_BILINEAR, 1,
            0, 0, 716, 572, /* crop param */
            2, 2, 2, 2, /* padding param */
            720, 576, 1920, 1080, /* resize param */
            1, 1, 1, 1, 0, 0, 0, 0, -32768, -32768, -32768, -32768 /* preprocess param */
        }
    };
    return sample_common_svp_npu_set_input_aipp_info(model_index, index, &model_aipp, &g_svp_npu_task[model_index]);
}

static td_s32 sample_svp_npu_acl_set_aipp_info(td_u32 model_index)
{
    td_s32 ret;

    ret = sample_svp_npu_acl_set_input_aipp_info(model_index, 0);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, fail, SAMPLE_SVP_ERR_LEVEL_ERROR, "set input aipp failed!\n");

    return TD_SUCCESS;

fail:
    return ret;
}

td_void sample_svp_npu_acl_aipp(td_void)
{
    td_s32 ret;
    const td_u32 model_idx = 0;
    const td_char *acl_config_path = "";
    const td_char *src_file[SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM] = {"./data/image/st_lk_720x576_420sp.yuv"};
    const td_char *om_model_path = "./data/model/aipp.om";

    /* init acl */
    ret = sample_svp_npu_acl_init(acl_config_path);
    sample_svp_check_exps_return_void(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR, "init failed!\n");

    /* load model */
    ret = sample_common_svp_npu_load_model(om_model_path, model_idx, TD_FALSE);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "load model failed!\n");

    /* set task cfg */
    g_svp_npu_task[0].cfg.max_batch_num = 1;
    g_svp_npu_task[0].cfg.dynamic_batch_num = 1;
    g_svp_npu_task[0].cfg.total_t = 0;
    g_svp_npu_task[0].cfg.is_cached = TD_FALSE;
    g_svp_npu_task[0].cfg.model_idx = model_idx;

    ret = sample_svp_npu_acl_init_task(1, TD_FALSE, 0);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "init task failed!\n");

    ret = sample_svp_npu_acl_set_aipp_info(model_idx);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "set aipp info failed!\n");

    ret = sample_common_svp_npu_get_input_data(src_file, SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM,
        &g_svp_npu_task[0]);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR, "get data failed!\n");

    ret = sample_common_svp_npu_model_execute(&g_svp_npu_task[0]);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR, "execute failed!\n");

process_end2:
    (td_void)sample_svp_npu_acl_deinit_task(1, 0);
process_end1:
    (td_void)sample_common_svp_npu_unload_model(model_idx);
process_end0:
    (td_void)sample_svp_npu_acl_deinit();
    (td_void)sample_svp_npu_acl_terminate();
}

td_void sample_svp_npu_acl_preemption(td_void)
{
    td_s32 ret;
    const td_u32 model_num = 2;
    const td_char *acl_config_path = "";
    const td_char *src_file[SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM] = {"./data/image/3_224_224_batch_1.bgr"};
    const td_char *om_model_path = "./data/model/resnet50_with_preemption.om";
    td_u32 i, j;
    td_u32 idx[SAMPLE_SVP_NPU_MAX_MODEL_NUM] = {0};
    pthread_t svp_npu_multi_thread[SAMPLE_SVP_NPU_MAX_MODEL_NUM] = {0};

    sample_svp_npu_model_prior_cfg prior_cfg[] = {
        { SAMPLE_SVP_NPU_PRIORITY_2, 1, 0, 0},
        { SAMPLE_SVP_NPU_PRIORITY_4, 1, SAMPLE_SVP_NPU_PRIOR_TIMEOUT, 0 }
    };

    /* init acl */
    ret = sample_svp_npu_acl_init(acl_config_path);
    sample_svp_check_exps_return_void(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR, "init failed!\n");

    for (i = 0; i < model_num; i++) {
        sample_svp_npu_set_model_prior_info(i, &prior_cfg[i]);
    }

    /* load model */
    ret = sample_svp_npu_acl_load_multi_model(om_model_path, model_num, TD_TRUE, src_file,
        SAMPLE_SVP_NPU_RESNET50_INPUT_FILE_NUM);
    sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR, "load model failed!\n");

    g_svp_npu_is_thread_start = TD_TRUE;

    for (i = 0; i < model_num; i++) {
        idx[i] = i;
        ret = pthread_create(&svp_npu_multi_thread[i], NULL, sample_svp_npu_acl_thread_multi_model_process, &idx[i]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, TEST_PICO_ERR_LEVEL_ERROR,
            "pthread_create failed!\n");
    }

    sleep(SAMPLE_SVP_NPU_PREEMPTION_SLEEP_SECOND);

process_end1:
    g_svp_npu_is_thread_start = TD_FALSE;
    for (j = 0; j < i; j++) {
        pthread_join(svp_npu_multi_thread[j], NULL);
    }

    (td_void)sample_svp_npu_acl_unload_multi_model(model_num);

process_end0:
    (td_void)sample_svp_npu_acl_deinit();
    (td_void)sample_svp_npu_acl_terminate();
}

static td_void sample_svp_npu_acl_set_detection_info(sample_svp_npu_detection_info *detection_info, td_u32 index)
{
    detection_info->num_name = g_svp_npu_yolo_roi_info[index].roi_num_name;
    detection_info->roi_name = g_svp_npu_yolo_roi_info[index].roi_class_name;
    detection_info->has_background = TD_FALSE;
    /* use PIC_BUTT to be a flag, get the input resolution form om */
    g_svp_npu_media_cfg.pic_type[1] = PIC_BUTT;
    g_svp_npu_terminate_signal = TD_FALSE;

    printf("[wtjdebug][sample_svp_svc_process][sample_svp_npu_acl_set_detection_info]: detection_info->num_name = %s\n", 
        detection_info->num_name);  // wtjdebug: "output0_"
    printf("[wtjdebug][sample_svp_svc_process][sample_svp_npu_acl_set_detection_info]: detection_info->roi_name = %s\n", 
        detection_info->roi_name);  // wtjdebug: "output0_0"

}

/* function : show the sample of e2e(vi -> vpss -> npu -> venc) yolo */
td_void sample_svp_npu_acl_e2e_yolo(td_u32 index)
{
    td_s32 ret;
    const td_u32 model_idx = 0; 
    td_char om_model_path[SAMPLE_SVP_NPU_PATH_LEN] = {0};
    sample_svp_npu_detection_info detection_info = {0};
    sample_svp_npu_thread_args args;

    args.model_name = SAMPLE_SVP_NPU_YOLO;
    args.detection_info = &detection_info;

    // ================= [新增] 1. 初始化 AprilTag =================
    // 根据图二构建真实的 tag_params 字典
    apriltag_param_t real_params[] = {
        {20,  0.2,     0.2603,  -0.3098},
        {100, 0.2,    -0.2603,  -0.3098},
        {586, 0.14740, 0.0,     -0.2505},
        {518, 0.12640, 0.0,     -0.1034},
        {213, 0.05,    0.1002,  -0.1034},
        {313, 0.05,   -0.1002,  -0.1034},
        {307, 0.03,    0.0,      0.0},
        {69,  0.03,   -0.09,    -0.01},
        {207, 0.03,    0.09,    -0.01}
    };
    
    // 计算数组长度
    int param_count = sizeof(real_params) / sizeof(real_params[0]);

    // 填入图二中的相机内参，cx 和 cy 请根据实际送显的 YUV 图像分辨率进行修改（这里假设为 1920x1080）
    double fx = 1553.682;
    double fy = 1547.282;
    double cx = 955.920; // 如果你的 base_frame 是 1920x1080
    double cy = 584.898; // 如果你的 base_frame 是 1920x1080

    // 注意最后传入 false，对应图二中 alignment 构造函数的 false (关闭 debug 模式)
    g_apriltag_handle = apriltag_align_create(fx, fy, cx, cy, 
                                              real_params, param_count, true);
                                              
    if (g_apriltag_handle == TD_NULL) {
        printf("[wtjdebug] ERROR: Failed to create apriltag handle!\n");
    }
    // ============================================================

    if (USE_NEW_RPN_MODEL) {
        ret = sprintf_s(om_model_path, SAMPLE_SVP_NPU_PATH_LEN - 1, "%s", "./data/model/apriltag20261021_new_rpn_scoreThresh60_original.om");
    } else {
        ret = sprintf_s(om_model_path, SAMPLE_SVP_NPU_PATH_LEN - 1, "%s%u%s", "./data/model/yolov", index, ".om");
    }
    // ret = sprintf_s(om_model_path, SAMPLE_SVP_NPU_PATH_LEN - 1, "%s%u%s", "./data/model/yolov", index, ".om");
    
    sample_svp_check_exps_return_void(ret <= 0, SAMPLE_SVP_ERR_LEVEL_ERROR, "sprintf_s failed!\n");

    printf("[wtjdebug][sample_svp_svc_process][sample_svp_npu_acl_e2e_yolo]: index = %d\n", index);

    sample_svp_npu_acl_set_detection_info(&detection_info, index);

    //RTSP
    if (g_svp_npu_rtsp_enable){
        sample_comm_venc_set_push_rtsp(TD_TRUE);

        const char *rtsp_ip = "0.0.0.0";
        rtsp_server_start(rtsp_ip, 554);
        rtsp_session_create(0, 0);  //ven0 h264
    }

    if (g_svp_npu_terminate_signal == TD_FALSE) {
        /* init acl */
        ret = sample_svp_npu_acl_init(TD_NULL);
        sample_svp_check_exps_return_void(ret != TD_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR, "init failed!\n");

        /* load model */
        ret = sample_common_svp_npu_load_model(om_model_path, model_idx, TD_FALSE);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end0, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "load model failed!\n");

        /* get input resolution */
        ret = sample_common_svp_npu_get_input_resolution(model_idx, 0, &g_svp_npu_media_cfg.pic_size[1]);  // wtj: set resolution for second channel of vpss 
                                                                                                           // based on model input
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "get pic size failed!\n");

        printf("[wtjdebug][sample_svp_svc_process][sample_svp_npu_acl_e2e_yolo]: g_svp_npu_media_cfg.pic_size[1].width = %d\n", 
            g_svp_npu_media_cfg.pic_size[1].width);  // wtjdebug
        printf("[wtjdebug][sample_svp_svc_process][sample_svp_npu_acl_e2e_yolo]: g_svp_npu_media_cfg.pic_size[1].height = %d\n", 
            g_svp_npu_media_cfg.pic_size[1].height);  // wtjdebug

        /* start vi vpss */
        ret = sample_common_svp_npu_start_vi_vpss_venc(&g_vi_config, &g_svp_npu_media_cfg);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end1, SAMPLE_SVP_ERR_LEVEL_DEBUG,
            "init media failed!\n");

        /* set cfg */
        sample_svp_npu_acl_set_task_info(0, model_idx);

        ret = sample_svp_npu_acl_init_task(1, TD_FALSE, 0);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end2, SAMPLE_SVP_ERR_LEVEL_ERROR, "init task failed!\n");
    }
    /* process */
    if (g_svp_npu_terminate_signal == TD_FALSE) {
        ret = sample_common_svp_npu_set_threshold(&g_svp_npu_yolo_threshold[index], SAMPLE_SVP_NPU_YOLO_THRESHOLD_NUM,
            &g_svp_npu_task[0]);
        sample_svp_check_exps_goto(ret != TD_SUCCESS, process_end3, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "set threshold failed!\n");

        g_svp_npu_thread_stop = TD_FALSE;
        ret = pthread_create(&g_svp_npu_thread, 0, sample_svp_npu_acl_to_venc, (td_void*)&args);
        sample_svp_check_exps_goto(ret != 0, process_end3, SAMPLE_SVP_ERR_LEVEL_ERROR, "create thread failed!\n");

        (td_void)sample_svp_npu_acl_pause();

        g_svp_npu_thread_stop = TD_TRUE;
        pthread_join(g_svp_npu_thread, TD_NULL);
    }

process_end3:
    (td_void)sample_svp_npu_acl_deinit_task(1, 0);
process_end2:
    (td_void)sample_common_svp_destroy_vb_stop_vi_vpss_venc(&g_vi_config, &g_svp_npu_media_cfg);
process_end1:
    (td_void)sample_common_svp_npu_unload_model(model_idx);
process_end0:
    (td_void)sample_svp_npu_acl_deinit();
    (td_void)sample_svp_npu_acl_terminate();
    // ================= [新增] 2. 销毁 AprilTag =================
    if (g_apriltag_handle != TD_NULL) {
        apriltag_align_destroy(g_apriltag_handle);
        g_apriltag_handle = TD_NULL;
        printf("[wtjdebug] AprilTag handle destroyed successfully.\n");
    }
    // ==========================================================
}
