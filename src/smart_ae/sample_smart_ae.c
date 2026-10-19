/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <limits.h>
#include "securec.h"
#include "ss_mpi_ae.h"
#include "sample_comm.h"
#include "sample_ipc.h"
#include "ss_mpi_smartae.h"

#ifdef AIDETECT_SUPPORT
#include "ss_mpi_aidetect.h"
#endif

#define OT_SAMPLE_AIDETECT_MAX_OUTPUT_RECT_NUM (20)
#define OT_SAMPLE_AIDETECT_DEMO_MAX_LEN (256)
#define OT_SAMPLE_AIDETECT_DEMO_MAX_PLUS_ONE_LEN (256 + 1)
#define OT_SAMPLE_AIDETECT_DEMO_TWO_MAX_LEN (256 * 2)
#define OT_SAMPLE_AIDETECT_DEMO_TWO_MAX_PLUS_ONE_LEN (256 * 2 + 1)
#define OT_SAMPLE_AIDETECT_MODEL_PATH_INPUT (1)
#define OT_SAMPLE_AIDETECT_STATICS_CONVERT_NUM (100)
#define OT_SAMPLE_AIDETECT_SEC_TIME_CONVERT_NUM (1000000)
#define OT_SAMPLE_AIDETECT_MICROSEC_TIME_CONVERT_NUM (1000.f)
#define OT_SAMPLE_AIDETECT_FILE_SEEK_POSITION (-512)
#define OT_SAMPLE_AIDETECT_IS_VIDEO (1)
#define OT_SAMPLE_AIDETECT_CHN_0 (0)
#define OT_SAMPLE_AIDETECT_SHOW_GRAY_RECT (1)
#define OT_SAMPLE_AIDETECT_SHOW_GRAY_THICKNESS (2)
#define OT_SAMPLE_AIDETECT_INDEX_NUM (5)
#define SAMPLE_AIDETECT_SHOW_RESULT (1)
#define VENC_MAX_CHN_NUM 8

#ifndef AIDETECT_SUPPORT
typedef td_u32 ot_aidetect_chn;
typedef struct {} ot_aidetect_input_model;
typedef struct {} ot_aidetect_chn_attr;
typedef struct {} ot_aidetect_chn_param;
typedef struct {} ot_aidetect_model_info;
typedef struct {} ot_aidetect_result_array;
typedef struct {} ot_aidetect_chn_status;
#endif

typedef enum {
    SMART_AE_DETECT_FACE = 0,
    SMART_AE_DETECT_HUMAN,
    SMART_AE_DETECT_FACE_AND_HUMAN,
    SMART_AE_DETECT_BUTT
} smart_ae_detect_type;

static volatile sig_atomic_t g_sig_flag = 0;
static sample_vpss_cfg g_vpss_cfg = {0};
static td_u32 g_type = 0;

#ifdef AIDETECT_SUPPORT
static td_char g_src_model_path[OT_SAMPLE_AIDETECT_DEMO_MAX_PLUS_ONE_LEN] =
    "./model/normal/det_hvf_normal.bin";
static td_u32 g_input_frame_width = 0;
static td_u32 g_input_frame_height = 0;
static td_s32 g_video_flg = 1;
    // 1: input video; other: input image, the format is NV21, and resolutin is got by "ss_mpi_aidetect_get_model_info"
static td_s32 g_path_or_men = 0;           // 1: input model path ,other: input model memory
static ot_aidetect_model_info g_model_info = {0};
static td_char class_types[OT_AIDETECT_CLASS_BUTT][OT_SAMPLE_AIDETECT_DEMO_MAX_LEN] = {
    "face",
    "human",
    "vehicle",
    "pet",
    "garbage",
    "bag",
    "wallet",
    "phone"
};
#endif

static td_s32 g_handle_num = 0;
static td_bool g_is_run_finish = TD_FALSE;
static pthread_mutex_t g_lock;

typedef struct {
    td_s32  (*pfn_create_chn)(ot_aidetect_chn chn, const ot_aidetect_input_model *input_model,
        const ot_aidetect_chn_attr *chn_attr);
    td_s32  (*pfn_destroy_chn)(ot_aidetect_chn chn);
    td_s32  (*pfn_set_chn_attr)(ot_aidetect_chn chn, const ot_aidetect_chn_attr *chn_attr);
    td_s32  (*pfn_get_chn_attr)(ot_aidetect_chn chn, ot_aidetect_chn_attr *chn_attr);
    td_s32 (*pfn_set_chn_param)(ot_aidetect_chn chn, const ot_aidetect_chn_param *chn_param);
    td_s32 (*pfn_get_chn_param)(ot_aidetect_chn chn, ot_aidetect_chn_param *chn_param);
    td_s32 (*pfn_get_model_info)(ot_aidetect_chn chn, ot_aidetect_model_info *model_info);
    td_s32 (*pfn_detect_process)(ot_aidetect_chn chn, const ot_video_frame *frame,
        ot_aidetect_result_array *detect_result);
    td_s32  (*pfn_set_log_level)(td_u32 log_level);
    td_s32  (*pfn_get_log_level)(td_u32 *log_level);
    td_s32  (*pfn_query_status)(ot_aidetect_chn chn, ot_aidetect_chn_status *status);
} smart_ae_detect_obj;

#ifndef AIDETECT_SUPPORT
static smart_ae_detect_obj g_detect_obj = {
    .pfn_create_chn = TD_NULL,
    .pfn_destroy_chn = TD_NULL,
    .pfn_set_chn_attr = TD_NULL,
    .pfn_get_chn_attr = TD_NULL,
    .pfn_set_chn_param = TD_NULL,
    .pfn_get_chn_param = TD_NULL,
    .pfn_get_model_info = TD_NULL,
    .pfn_detect_process = TD_NULL,
    .pfn_set_log_level = TD_NULL,
    .pfn_get_log_level = TD_NULL,
    .pfn_query_status = TD_NULL
};
#else
static smart_ae_detect_obj g_detect_obj = {
    .pfn_create_chn = ss_mpi_aidetect_create_chn,
    .pfn_destroy_chn = ss_mpi_aidetect_destroy_chn,
    .pfn_set_chn_attr = ss_mpi_aidetect_set_chn_attr,
    .pfn_get_chn_attr = ss_mpi_aidetect_get_chn_attr,
    .pfn_set_chn_param = ss_mpi_aidetect_set_chn_param,
    .pfn_get_chn_param = ss_mpi_aidetect_get_chn_param,
    .pfn_get_model_info = ss_mpi_aidetect_get_model_info,
    .pfn_detect_process = ss_mpi_aidetect_process,
    .pfn_set_log_level = ss_mpi_aidetect_set_log_level,
    .pfn_get_log_level = ss_mpi_aidetect_get_log_level,
    .pfn_query_status = ss_mpi_aidetect_query_status
};
#endif

static td_void sample_get_char(td_void)
{
    if (g_sig_flag == 1) {
        return;
    }

    sample_pause();
}

//  the parameters introduction of sample
static td_void sample_smart_ae_usage(const td_char *prg_name)
{
    (td_void)printf("usage : %s <index> \n", prg_name);
    (td_void)printf("index:\n");
    (td_void)printf("    (0) only detect face.\n");
    (td_void)printf("    (1) only detect people.\n");
    (td_void)printf("    (2) detect face and people.\n");
}

static td_s32 sample_smart_ae_parase_param(td_s32 argc, td_char *argv[])
{
    td_char *end_ptr = TD_NULL;

    if (argc != 2) { /* 2:arg num */
        return TD_FAILURE;
    }

    if (!strncmp(argv[1], "-h", 2)) { /* 2:arg num */
        return TD_FAILURE;
    }

    if (strlen(argv[1]) > 1 || strlen(argv[1]) == 0 || !check_digit(argv[1][0])) {
        return TD_FAILURE;
    }

    g_type = (td_u32)strtol(argv[1], &end_ptr, 10); /* base 10, argv[1] has been check between [0, 2] */
    if ((end_ptr == argv[1]) || (*end_ptr) != '\0') {
        return TD_FAILURE;
    }

    if (g_type > SMART_AE_DETECT_FACE_AND_HUMAN) {
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

#ifdef AIDETECT_SUPPORT
static td_s32 sample_smart_ae_open_model_file(const td_char *model_file_name, FILE **fd)
{
    td_char real_file_name[PATH_MAX + 1] = {0};
    if ((strlen(model_file_name) > PATH_MAX) || (realpath(model_file_name, real_file_name) == TD_NULL)) {
        sample_print("model_file_name realpath err!\n");
        return TD_FAILURE;
    }

    *fd = fopen(real_file_name, "rb");
    if (*fd == TD_NULL) {
        sample_print("open file %s err!\n", real_file_name);
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

static td_char *sample_smart_ae_read_model_file(const td_char *path, td_u32 *len)
{
    td_s32 ret;
    FILE *fd = TD_NULL;
    td_char *data = TD_NULL;

    ret = sample_smart_ae_open_model_file(path, &fd);
    if (ret != TD_SUCCESS) {
        return TD_NULL;
    }
    (td_void)fseek(fd, 0, SEEK_END);
    *len = (td_u32)ftell(fd);
    (td_void)fseek(fd, 0, SEEK_SET);
    data = (td_char *)malloc(*len);
    if (data == TD_NULL) {
        (td_void)fclose(fd);
        return TD_NULL;
    }
    ret = memset_s(data, *len, 0, *len);
    if (ret != EOK) {
        sample_print("memset_s failed!\n");
        free(data);
        return TD_NULL;
    }
    (td_void)fread(data, *len, 1, fd);
    (td_void)fclose(fd);
    fd = TD_NULL;

    return data;
}

static td_s32 sample_smart_ae_proc_model_info()
{
    td_s32 ret = TD_SUCCESS;
    ot_aidetect_chn_attr chn_attr;
    td_u32 i = 0;

    ret = memset_s(&g_model_info, sizeof(ot_aidetect_model_info), 0, sizeof(ot_aidetect_model_info));
    if (ret != EOK) {
        sample_print("memset_s failed!\n");
        return TD_FAILURE;
    }
    if (g_detect_obj.pfn_get_model_info != TD_NULL) {
        ret = g_detect_obj.pfn_get_model_info(OT_SAMPLE_AIDETECT_CHN_0, &g_model_info);
        if (TD_SUCCESS != ret) {
            sample_print("ss_mpi_aidetect_get_model_info error: %X.\n", ret);
            return TD_FAILURE;
        }
    }

    g_input_frame_width = g_model_info.size.width;
    g_input_frame_height = g_model_info.size.height;
    printf("g_input_frame_width:%d g_input_frame_height:%d\n", g_model_info.size.width,  g_model_info.size.height);
    sample_print("input image w:%u,h:%u, class num: %u\n", g_input_frame_width, g_input_frame_height,
        g_model_info.class_num);
    if (OT_SAMPLE_AIDETECT_IS_VIDEO == g_video_flg) { // input video  then enable the track
        ret = memset_s(&chn_attr, sizeof(ot_aidetect_chn_attr), 0, sizeof(ot_aidetect_chn_attr));
        if (ret != EOK) {
            sample_print("memset_s failed!\n");
            return TD_FAILURE;
        }
        chn_attr.track_class_num = g_model_info.class_num;
        for (i = 0; i < g_model_info.class_num; i++) {
            (td_void)printf("class type name:%s[%d]\n", class_types[g_model_info.classes[i]],
                (td_s32)g_model_info.classes[i]);
            chn_attr.track_class_attr[i].class_type = g_model_info.classes[i];
            chn_attr.track_class_attr[i].track_en = TD_FALSE;
        }

        if (g_detect_obj.pfn_set_chn_attr != TD_NULL) {
            ret = g_detect_obj.pfn_set_chn_attr(OT_SAMPLE_AIDETECT_CHN_0, &chn_attr);
            if (ret != TD_SUCCESS) {
                sample_print("ss_mpi_aidetect_set_chn_attr ret:%X\n", ret);
                return TD_FAILURE;
            }
        }
    }

    return TD_SUCCESS;
}
#endif

#ifdef AIDETECT_SUPPORT
static td_void sample_smart_ae_show_chn_param()
{
    td_u32 i = 0;
    td_s32 ret = TD_SUCCESS;
    ot_aidetect_chn_param chn_param;

    ret = memset_s(&chn_param, sizeof(ot_aidetect_chn_param), 0, sizeof(ot_aidetect_chn_param));
    if (ret != EOK) {
        sample_print("memset_s failed %d\n", ret);
        return;
    }
    if (g_detect_obj.pfn_get_chn_param == TD_NULL) {
        return;
    }
    ret = g_detect_obj.pfn_get_chn_param(OT_SAMPLE_AIDETECT_CHN_0, &chn_param);
    if (ret == TD_SUCCESS) {
        for (i = 0; i < chn_param.detect_threshold_num; i++) {
            (td_void)printf("class type:%s,det threshold:%f, track miss num:%u,model prority:[%d,%u,%u,%u]\n",
                class_types[chn_param.detect_threshold[i].class_type], chn_param.detect_threshold[i].detect_threshold,
                chn_param.detect_threshold[i].track_miss_frame_num, chn_param.model_priority.preemp_en,
                chn_param.model_priority.priority, chn_param.model_priority.priority_up_step_timeout,
                chn_param.model_priority.priority_up_top_timeout);
        }
    } else {
        sample_print("ss_mpi_aidetect_get_chn_param error:%X\n", ret);
    }
}

#endif
static td_s32 sample_smart_ae_init_model()
{
#ifdef AIDETECT_SUPPORT
    td_char *model_data = TD_NULL;
    td_u32 model_len = 0;
    ot_aidetect_input_model t_input_model_info;
    ot_aidetect_chn_attr chn_attr;
    td_s32 ret = TD_SUCCESS;

    (td_void)memset_s(&t_input_model_info, sizeof(ot_aidetect_input_model), 0, sizeof(ot_aidetect_input_model));
    if (OT_SAMPLE_AIDETECT_MODEL_PATH_INPUT == g_path_or_men) {
        t_input_model_info.model_load_mode = OT_AIDETECT_MODEL_LOAD_FROM_PATH;
        t_input_model_info.model = (td_void *)g_src_model_path;
        t_input_model_info.size = (td_u32)strlen(g_src_model_path);
    } else {
        model_data = sample_smart_ae_read_model_file(g_src_model_path, &model_len);
        if (TD_NULL == model_data) {
            sample_print("read model[%s] data error.\n", g_src_model_path);
            return TD_FAILURE;
        }
        t_input_model_info.model_load_mode = OT_AIDETECT_MODEL_LOAD_FROM_MEMORY;
        t_input_model_info.model = model_data;
        t_input_model_info.size = model_len;
    }

    (td_void)memset_s(&chn_attr, sizeof(ot_aidetect_chn_attr), 0, sizeof(ot_aidetect_chn_attr));

    if (g_detect_obj.pfn_create_chn != TD_NULL) {
        ret = g_detect_obj.pfn_create_chn(OT_SAMPLE_AIDETECT_CHN_0, &t_input_model_info, &chn_attr);
    }

    if (model_data != TD_NULL) {
        free(model_data);
        model_data = TD_NULL;
    }

    if (TD_SUCCESS != ret) {
        sample_print("ss_mpi_aidetect_create_chn error: %X.\n", ret);
        return TD_FAILURE;
    }

    if (sample_smart_ae_proc_model_info() != TD_SUCCESS) {
        if (g_detect_obj.pfn_destroy_chn != TD_NULL) {
            (td_void)g_detect_obj.pfn_destroy_chn(OT_SAMPLE_AIDETECT_CHN_0);
        }
        return TD_FAILURE;
    }

    sample_smart_ae_show_chn_param();
#endif
    return TD_SUCCESS;
}

static td_void sample_smart_ae_result_clear(ot_aidetect_result_array *result)
{
#ifdef AIDETECT_SUPPORT
    td_s32 ret;
    td_u32 i = 0;
    for (i = 0; i < result->class_num; ++i) {
        result->object_class[i].object_num = 0;
        ret = memset_s(result->object_class[i].objects,
            sizeof(ot_aidetect_object) * result->object_class[i].object_capacity, 0,
            sizeof(ot_aidetect_object) * result->object_class[i].object_capacity);
        if (ret != EOK) {
            sample_print("memset_s failed!\n");
            return;
        }
    }
#endif
}

static td_void sample_smart_ae_result_free(ot_aidetect_result_array *result)
{
#ifdef AIDETECT_SUPPORT
    td_u32 i = 0;
    if (result == TD_NULL)
        return;
    for (i = 0; i < result->class_num; ++i) {
        if (result->object_class[i].objects != TD_NULL) {
            free(result->object_class[i].objects);
            result->object_class[i].objects = TD_NULL;
        }
    }
#endif
}

static td_s32 sample_smart_ae_result_init(ot_aidetect_result_array *result)
{
#ifdef AIDETECT_SUPPORT
    td_u32 i = 0;
    td_s32 ret;

    ret = memset_s(result, sizeof(ot_aidetect_result_array), 0, sizeof(ot_aidetect_result_array));
    if (ret != EOK) {
        sample_print("memset_s failed!\n");
        return ret;
    }
    result->class_num =
        (g_model_info.class_num > OT_AIDETECT_CLASS_BUTT ? OT_AIDETECT_CLASS_BUTT : g_model_info.class_num);
    for (i = 0; i < result->class_num; ++i) {
        result->object_class[i].class_type = g_model_info.classes[i];
        result->object_class[i].object_capacity = OT_SAMPLE_AIDETECT_MAX_OUTPUT_RECT_NUM;
        result->object_class[i].objects =
            (ot_aidetect_object *)malloc(sizeof(ot_aidetect_object) * result->object_class[i].object_capacity);
        if (result->object_class[i].objects == TD_NULL) {
            sample_print("malloc failed!\n");
            goto exit;
        }
    }

    sample_smart_ae_result_clear(result);
    return TD_SUCCESS;
exit:
    sample_smart_ae_result_free(result);
    return TD_FAILURE;
#else
    return TD_SUCCESS;
#endif
}

#ifdef AIDETECT_SUPPORT
static td_s32 sample_smart_ae_config_aidetect_param()
{
    td_s32 ret = 0;
    ot_aidetect_chn_param param;
    ret = memset_s(&param, sizeof(ot_aidetect_chn_param), 0, sizeof(ot_aidetect_chn_param));
    if (ret != EOK) {
        sample_print("memset_s failed!\n");
        return TD_FAILURE;
    }

    if (g_type == SMART_AE_DETECT_FACE) {
        param.detect_threshold_num = 1;
        param.detect_threshold[0].class_type = OT_AIDETECT_CLASS_FACE;
        param.detect_threshold[0].detect_threshold = 0.67f;
        param.detect_threshold[0].track_miss_frame_num = 30; /* 30 */
    } else if (g_type == SMART_AE_DETECT_HUMAN) {
        param.detect_threshold_num = 1;
        param.detect_threshold[0].class_type = OT_AIDETECT_CLASS_HUMAN;
        param.detect_threshold[0].detect_threshold = 0.67f;
        param.detect_threshold[0].track_miss_frame_num = 30; /* 30 */
    } else if (g_type == SMART_AE_DETECT_FACE_AND_HUMAN) {
        param.detect_threshold_num = 2; /* detect 2 calss */
        param.detect_threshold[0].class_type = OT_AIDETECT_CLASS_FACE;
        param.detect_threshold[0].detect_threshold = 0.67f;
        param.detect_threshold[0].track_miss_frame_num = 30; /* 30 */
        param.detect_threshold[1].class_type = OT_AIDETECT_CLASS_HUMAN;
        param.detect_threshold[1].detect_threshold = 0.66f;
        param.detect_threshold[1].track_miss_frame_num = 30; /* 30 */
    }

    if (g_detect_obj.pfn_set_chn_param != TD_NULL) {
        ret = g_detect_obj.pfn_set_chn_param(OT_SAMPLE_AIDETECT_CHN_0, &param);
        if (TD_SUCCESS != ret) {
            sample_print("ss_mpi_aidetect_set_chn_param error:%X!!\n", ret);
            return TD_FAILURE;
        }
    }
    return TD_SUCCESS;
}

static td_s32 sample_smart_ae_config_aidetect_attr()
{
    td_s32 ret = 0;
    ot_aidetect_chn_attr attr;
    ret = memset_s(&attr, sizeof(ot_aidetect_chn_attr), 0, sizeof(ot_aidetect_chn_attr));
    if (ret != EOK) {
        sample_print("memset_s failed!\n");
        return TD_FAILURE;
    }

    if (g_type == SMART_AE_DETECT_FACE) {
        attr.track_class_num = 1;
        attr.track_class_attr[0].class_type = OT_AIDETECT_CLASS_FACE;
        attr.track_class_attr[0].track_en = TD_FALSE;
    } else if (g_type == SMART_AE_DETECT_HUMAN) {
        attr.track_class_num = 1;
        attr.track_class_attr[0].class_type = OT_AIDETECT_CLASS_HUMAN;
        attr.track_class_attr[0].track_en = TD_FALSE;
    } else if (g_type == SMART_AE_DETECT_FACE_AND_HUMAN) {
        attr.track_class_num = 2; /* detect 2 calss */
        attr.track_class_attr[0].class_type = OT_AIDETECT_CLASS_FACE;
        attr.track_class_attr[0].track_en = TD_FALSE;
        attr.track_class_attr[1].class_type = OT_AIDETECT_CLASS_HUMAN;
        attr.track_class_attr[1].track_en = TD_FALSE;
    }

    if (g_detect_obj.pfn_set_chn_attr != TD_NULL) {
        ret = g_detect_obj.pfn_set_chn_attr(OT_SAMPLE_AIDETECT_CHN_0, &attr);
        if (TD_SUCCESS != ret) {
            sample_print("ss_mpi_aidetect_set_chn_attr error:%X!!\n", ret);
            return TD_FAILURE;
        }
    }
    return TD_SUCCESS;
}
#endif

static td_s32 sample_smart_ae_config_aidetect(td_void)
{
#ifdef AIDETECT_SUPPORT
    td_s32 ret = 0;

    ret = sample_smart_ae_config_aidetect_param();
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

    ret = sample_smart_ae_config_aidetect_attr();
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }

#endif
    return TD_SUCCESS;
}

#ifdef AIDETECT_SUPPORT
static td_void sample_smart_get_smart_info(ot_aidetect_result_array *result, ot_smartae_roi_info *smart_info)
{
    td_u32 i, j, smart_info_index;

    smart_info->class_num = 0;
    smart_info_index = 0;

    for (i = 0; i < result->class_num; i++) {
        if ((i == 0 && g_type == SMART_AE_DETECT_HUMAN) || (i == 1 && g_type == SMART_AE_DETECT_FACE) ||
            (i != 0 && i != 1)) { /* only support face and people */
            continue;
        }
        if (result->object_class[i].object_num > 0) {
            smart_info->obj_class[smart_info_index].type = i;
            smart_info->obj_class[smart_info_index].rect_num = result->object_class[i].object_num;
            for (j = 0; j < result->object_class[i].object_num && j < OT_SMARTAE_MAX_RECT_NUM; j++) {
                smart_info->obj_class[smart_info_index].objs[j].score =
                    result->object_class[i].objects[j].detect_confidence;
                (td_void)memcpy_s(&smart_info->obj_class[smart_info_index].objs[j].rect, sizeof(ot_rect),
                    &result->object_class[i].objects[j].detect_rect, sizeof(ot_rect));
            }
            smart_info->class_num++;
            smart_info_index++;
        }
    }
}
#endif

static td_void sample_smart_ae_clean_rect(td_void)
{
    td_s32 min_handle, i;
    ot_mpp_chn chn;

    min_handle = sample_comm_region_get_min_handle(OT_RGN_CORNER_RECT);
    chn.mod_id = OT_ID_VPSS;
    chn.dev_id = 0;
    chn.chn_id = 0;
    for (i = min_handle; i < min_handle + g_handle_num; i++) {
        ss_mpi_rgn_detach_from_chn(i, &chn);
    }
}

#ifdef AIDETECT_SUPPORT
static td_s32 sample_smart_ae_draw_rect(ot_aidetect_result_array *result, ot_mpp_chn* chn)
{
    td_s32 ret;
    td_u32 i, j, min_handle;
    ot_rgn_chn_attr chn_attr;
    ot_aidetect_object *obj = TD_NULL;

    chn_attr.is_show = TD_TRUE;
    chn_attr.type = OT_RGN_CORNER_RECT;
    min_handle = sample_comm_region_get_min_handle(OT_RGN_CORNER_RECT);
    g_handle_num = 0;
    for (i = 0; i < result->class_num; i++) {
        if ((i == 0 && g_type == SMART_AE_DETECT_HUMAN) || (i == 1 && g_type == SMART_AE_DETECT_FACE) ||
            (i != 0 && i != 1)) { /* only support face and people */
            continue;
        }
        for (j = 0; j < result->object_class[i].object_num; j++) {
            obj = &result->object_class[i].objects[j];
            chn_attr.attr.corner_rect_chn.corner_rect.rect.height = g_vpss_cfg.chn_attr[0].height *
                obj->detect_rect.height / (g_vpss_cfg.chn_attr[1].height * 2) * 2 ; /* 2 align */
            chn_attr.attr.corner_rect_chn.corner_rect.rect.width = g_vpss_cfg.chn_attr[0].width *
                obj->detect_rect.width / (g_vpss_cfg.chn_attr[1].width * 2) * 2;    /* 2 align */
            chn_attr.attr.corner_rect_chn.corner_rect.rect.x = g_vpss_cfg.chn_attr[0].height *
                obj->detect_rect.x / (g_vpss_cfg.chn_attr[1].height * 2) * 2 + 2;   /* 2 align */
            chn_attr.attr.corner_rect_chn.corner_rect.rect.y = g_vpss_cfg.chn_attr[0].width *
                obj->detect_rect.y / (g_vpss_cfg.chn_attr[1].width * 2) * 2 + 2;    /* 2 align */
            chn_attr.attr.corner_rect_chn.corner_rect.hor_len = obj->detect_rect.width / 2 * 2; /* 2 align */
            chn_attr.attr.corner_rect_chn.corner_rect.ver_len = obj->detect_rect.height / 2 * 2; /* 2 align */
            chn_attr.attr.corner_rect_chn.corner_rect_attr.color = 0xff0000; /* RED color */
            chn_attr.attr.corner_rect_chn.corner_rect_attr.corner_rect_type = OT_CORNER_RECT_TYPE_CORNER;
            chn_attr.attr.corner_rect_chn.corner_rect.thick = OT_RGN_CORNER_RECT_MIN_THICK + 2; /* 2 */
            chn_attr.attr.corner_rect_chn.layer = 0;

            ret = ss_mpi_rgn_attach_to_chn(min_handle + g_handle_num, chn, &chn_attr);
            if (ret != TD_SUCCESS) {
                sample_print("sample_region_attach %d failed!\n", min_handle + g_handle_num);
                sample_smart_ae_clean_rect();
                return ret;
            }
            g_handle_num++;
            if (g_handle_num >= OT_RGN_VPSS_MAX_CORNER_RECT_NUM) { /* rgn max corner is 4. */
                return TD_SUCCESS;
            }
        }
    }

    return TD_SUCCESS;
}
#endif

static td_s32 sample_smart_ae_proc_one_frame(td_u32 index_num, ot_aidetect_result_array *result, ot_mpp_chn* chn)
{
    td_s32 ret = TD_SUCCESS;
    ot_video_frame_info src_frame;
    td_u32 size = 0, size_c = 0;

    ret = ss_mpi_vpss_get_chn_frame(0, 1, &src_frame, 500); /* 500ms */
    if (ret != TD_SUCCESS) {
        printf("ss_mpi_vpss_get_chn_frame fail, ret = 0x%x\n", ret);
    }
    size = src_frame.video_frame.stride[0] * src_frame.video_frame.height;
    size_c = src_frame.video_frame.stride[1] * src_frame.video_frame.height / 2; /* YUV420, 4/2 */

    src_frame.video_frame.virt_addr[0] = (td_char *)ss_mpi_sys_mmap(src_frame.video_frame.phys_addr[0], size);
    src_frame.video_frame.virt_addr[1] = (td_char *)ss_mpi_sys_mmap(src_frame.video_frame.phys_addr[1], size_c);
    if (src_frame.video_frame.virt_addr[0] == TD_NULL) {
        isp_err_trace("ss_mpi_sys_mmap failed.\n");
        return TD_FAILURE;
    }
    if (src_frame.video_frame.virt_addr[1] == TD_NULL) {
        ss_mpi_sys_munmap(src_frame.video_frame.virt_addr[0], size);
        isp_err_trace("ss_mpi_sys_mmap failed.\n");
        return TD_FAILURE;
    }

    if (g_detect_obj.pfn_detect_process != TD_NULL) {
        ret = g_detect_obj.pfn_detect_process(OT_SAMPLE_AIDETECT_CHN_0, &src_frame.video_frame, result);
        if (TD_SUCCESS != ret) {
            sample_print("ss_mpi_aidetect_process error:%X!!\n", ret);
            goto exit;
        }
    }

    sample_smart_ae_clean_rect();
#ifdef AIDETECT_SUPPORT
    ot_smartae_roi_info smart_info = {0};
    sample_smart_get_smart_info(result, &smart_info);
    ss_mpi_smartae_update_roi_info(0, &src_frame, &smart_info);
    if (result->object_class[0].object_num + result->object_class[1].object_num > 0) {
        if ((result->object_class[0].object_num == 0 && g_type == SMART_AE_DETECT_FACE) ||
            (result->object_class[1].object_num == 0 && g_type == SMART_AE_DETECT_HUMAN)) {
            goto exit;
        }
        sample_smart_ae_draw_rect(result, chn);
    }
#endif

exit:
    ss_mpi_sys_munmap(src_frame.video_frame.virt_addr[0], size);
    ss_mpi_sys_munmap(src_frame.video_frame.virt_addr[1], size_c);
    if (ss_mpi_vpss_release_chn_frame(0, 1, &src_frame) != TD_SUCCESS) {
        printf("ss_mpi_vpss_release_chn_frame fail.\n");
    }

    sample_smart_ae_result_clear(result);
    return ret;
}

static td_void sample_smart_ae_set_smart_exposure_attr(td_void)
{
    ot_isp_smart_exposure_attr smart_exp_attr = {0};
    ss_mpi_isp_get_smart_exposure_attr(0, &smart_exp_attr);
    smart_exp_attr.enable = TD_TRUE;
    smart_exp_attr.ir_mode = TD_FALSE;
    smart_exp_attr.smart_exp_type = OT_OP_MODE_AUTO;
    smart_exp_attr.luma_target = 80; /* 80 */
    smart_exp_attr.exp_coef_max = 8192; /* 8192 */
    smart_exp_attr.exp_coef_min = 64; /* 64 */
    smart_exp_attr.smart_interval = 5; /* 5 */
    smart_exp_attr.smart_speed = 32; /* 32 */
    smart_exp_attr.smart_delay_num = 60; /* 60 */
    ss_mpi_isp_set_smart_exposure_attr(0, &smart_exp_attr);
}

static td_s32 sample_smart_ae_rgn_init(td_u32 handle_num)
{
    td_s32 ret;
    td_s32 i, j, min_handle;
    ot_rgn_attr region;

    min_handle = sample_comm_region_get_min_handle(OT_RGN_CORNER_RECT);
    region.type = OT_RGN_CORNER_RECT;
    for (i = min_handle; i < min_handle + (td_s32)handle_num; i++) {
        ret = ss_mpi_rgn_create(i, &region);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_rgn_create failed with 0x%x!\n", ret);
            goto exit;
        }
    }
    return TD_SUCCESS;

exit:
    for (j = i - 1; j > min_handle; j--) {
        (td_void)ss_mpi_rgn_destroy(j);
    }
    return ret;
}

static td_void *sample_ae_detect_thread(td_void *args)
{
    td_s32 ret;
    ot_aidetect_result_array result;
    td_u32 index_num = 0, handle_num = 10; /* 10 */
    ot_mpp_chn chn;

    chn.mod_id = OT_ID_VPSS;
    chn.dev_id = 0;
    chn.chn_id = 0;

    ret = sample_smart_ae_result_init(&result);
    if (ret != TD_SUCCESS) {
        return TD_NULL;
    }
    sample_smart_ae_set_smart_exposure_attr();
    ret = sample_smart_ae_rgn_init(handle_num);
    if (ret != TD_SUCCESS) {
        goto exit;
    }
    while (1) {
        if (g_is_run_finish) {
            break;
        }
        ss_mpi_isp_get_vd_time_out(0, OT_ISP_VD_FE_START, 500); /* 500ms */
        sample_smart_ae_proc_one_frame(index_num, &result, &chn);
        index_num++;
    }
    sample_smart_ae_result_free(&result);
    sample_smart_ae_clean_rect();

    ret = sample_comm_region_destroy(handle_num, OT_RGN_CORNER_RECT);
    if (ret != TD_SUCCESS) {
        sample_print("sample_comm_region_destroy failed!\n");
    }

    return TD_NULL;

exit:
    sample_smart_ae_result_free(&result);
    return TD_NULL;
}

static td_s32 sample_smart_ae_video_proc()
{
    td_s32 ret;
    pthread_t smart_ae_detect_th;

    sample_smart_ae_config_aidetect();
    ret = pthread_mutex_init(&g_lock, TD_NULL);
    if (ret != 0) {
        sample_print("pthread mutex init fail!\n");
        return ret;
    }

    ret = pthread_create(&smart_ae_detect_th, TD_NULL, sample_ae_detect_thread, TD_NULL);
    if (ret != 0) {
        (td_void)pthread_mutex_lock(&g_lock);
        g_is_run_finish = TD_TRUE;
        (td_void)pthread_mutex_unlock(&g_lock);
        (td_void)pthread_mutex_destroy(&g_lock);
        printf("create ae_detect_thread thread failed!, error: %d\r\n", ret);
        return ret;
    }

    sample_get_char();
    (td_void)pthread_mutex_lock(&g_lock);
    g_is_run_finish = TD_TRUE;
    (td_void)pthread_mutex_unlock(&g_lock);
    (td_void)pthread_join(smart_ae_detect_th, TD_NULL);
    (td_void)pthread_mutex_destroy(&g_lock);

    return TD_SUCCESS;
}

/* this configuration is used to adjust the size and number of buffer(VB).  */
static sample_vb_param g_vb_param = {
    /* raw, yuv, vpss chn1 */
    .vb_size = {{2688, 1520}, {2688, 1520}, {1024, 576}},
    .pixel_format = {OT_PIXEL_FORMAT_RGB_BAYER_12BPP, OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420,
        OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420},
    .compress_mode = {OT_COMPRESS_MODE_LINE, OT_COMPRESS_MODE_NONE,
        OT_COMPRESS_MODE_NONE},
    .video_format = {OT_VIDEO_FORMAT_LINEAR, OT_VIDEO_FORMAT_LINEAR,
        OT_VIDEO_FORMAT_LINEAR},
    .blk_num = {4, 7, 3}
};

static sampe_sys_cfg g_vie_sys_cfg = {
    .route_num = 1,
    .mode_type = OT_VI_OFFLINE_VPSS_OFFLINE,
    .vpss_wrap_en = TD_TRUE,
    .vpss_wrap_size = 3200 * 128 *1.5,
};

static sample_comm_venc_chn_param g_venc_chn_param = {
    .frame_rate           = 30, /* 30 is a number */
    .stats_time           = 2,  /* 2 is a number */
    .gop                  = 60, /* 60 is a number */
    .venc_size            = {1920, 1080},
    .size                 = -1,
    .profile              = 0,
    .is_rcn_ref_share_buf = TD_FALSE,
    .gop_attr             = {
        .gop_mode = OT_VENC_GOP_MODE_NORMAL_P,
        .normal_p = {2},
    },
    .type                 = OT_PT_H265,
    .rc_mode              = SAMPLE_RC_CBR,
};

/* define SAMPLE_MEM_SHARE_ENABLE, when use tools to dump YUV/RAW. */
#ifdef SAMPLE_MEM_SHARE_ENABLE
td_void sample_smart_ae_init_mem_share(td_void)
{
    td_u32 i;
    ot_vb_common_pools_id pools_id = {0};

    if (ss_mpi_vb_get_common_pool_id(&pools_id) != TD_SUCCESS) {
        sample_print("get common pool_id failed!\n");
        return;
    }
    for (i = 0; i < pools_id.pool_cnt; ++i) {
        ss_mpi_vb_pool_share_all(pools_id.pool[i]);
    }
}
#endif

/* default hy006_3814_0011 x 2 */
static ot_vi_wrap_in_param g_vi_wrap_param = {
    .is_slave = TD_FALSE,
    .pipe_num = 2,
    .wrap_param[0] = {
        .pipe_size = {2560, 1440},
        .frame_rate_100x = 3000,
        .full_lines_std = 1500,
        .slave_param = {
            .vi_vpss_online = TD_TRUE,
            .vpss_chn0_size = {2560, 1440},
            .vpss_venc_wrap = TD_TRUE,
            .large_stream_size = {2560, 1440},
            .small_stream_size = {1024, 576},
        }
    },
    .wrap_param[1] = {
        .pipe_size = {2560, 1440},
        .frame_rate_100x = 3000,
        .full_lines_std = 1500,
        .slave_param.vi_vpss_online = TD_TRUE,
        .slave_param.vpss_chn0_size = {2560, 1440},
        .slave_param.vpss_venc_wrap = TD_TRUE,
        .slave_param.large_stream_size = {2560, 1440},
        .slave_param.small_stream_size = {1024, 576},
    }
};

static td_void sample_smart_ae_get_pipe_wrap_line(sample_vi_cfg vi_cfg[], td_s32 route_num)
{
    td_s32 ret;
    ot_vi_wrap_out_param out_param;
    ot_vi_wrap_in_param in_param;
    td_s32 i;

    if (g_vie_sys_cfg.mode_type == OT_VI_ONLINE_VPSS_ONLINE || g_vie_sys_cfg.mode_type == OT_VI_ONLINE_VPSS_OFFLINE) {
        return;
    }

    ret = memcpy_s(&in_param, sizeof(ot_vi_wrap_in_param), &g_vi_wrap_param, sizeof(ot_vi_wrap_in_param));
    if (ret != EOK) {
        sample_print("memcpy_s fail!\n");
        return;
    }
    if (g_vie_sys_cfg.mode_type == OT_VI_OFFLINE_VPSS_OFFLINE) {
        in_param.wrap_param[0].slave_param.vi_vpss_online = TD_FALSE;
        in_param.wrap_param[1].slave_param.vi_vpss_online = TD_FALSE;
    }
    in_param.pipe_num = (td_u32)route_num;

    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &in_param.wrap_param[0].pipe_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &in_param.wrap_param[0].slave_param.vpss_chn0_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &in_param.wrap_param[0].slave_param.large_stream_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR1_TYPE, &in_param.wrap_param[1].pipe_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR1_TYPE, &in_param.wrap_param[1].slave_param.vpss_chn0_size);
    sample_comm_vi_get_size_by_sns_type(SENSOR1_TYPE, &in_param.wrap_param[1].slave_param.large_stream_size);
    in_param.wrap_param[0].full_lines_std = in_param.wrap_param[0].pipe_size.height;
    in_param.wrap_param[1].full_lines_std = in_param.wrap_param[1].pipe_size.height;
    if (ss_mpi_sys_get_vi_wrap_buffer_line(&in_param, &out_param) != TD_SUCCESS) {
        sample_print("sample_vie get wrap line fail, pipe_wrap not enable!\n");
        return;
    }

    for (i = 0; i < route_num; i++) {
        vi_cfg[i].pipe_info[0].wrap_attr.buf_line = out_param.buf_line[i];
        vi_cfg[i].pipe_info[0].wrap_attr.enable = TD_FALSE;
    }
}

static td_s32 sample_smart_ae_sys_init(td_void)
{
    ot_vb_cfg vb_cfg;
    td_u32 supplement_config = OT_VB_SUPPLEMENT_BNR_MOT_MASK | OT_VB_SUPPLEMENT_MOTION_DATA_MASK;

    sample_comm_sys_get_default_vb_cfg(&g_vb_param, &vb_cfg);
    /* prepare vpss wrap vb */
    if (g_vie_sys_cfg.vpss_wrap_en) {
        vb_cfg.common_pool[SAMPLE_VIE_POOL_NUM].blk_cnt = 1;
        vb_cfg.common_pool[SAMPLE_VIE_POOL_NUM].blk_size = g_vie_sys_cfg.vpss_wrap_size;
    }
    if (sample_comm_sys_init_with_vb_supplement(&vb_cfg, supplement_config) != TD_SUCCESS) {
        return TD_FAILURE;
    }

#ifdef SAMPLE_MEM_SHARE_ENABLE
    sample_smart_ae_init_mem_share();
#endif

    if (sample_comm_vi_set_vi_vpss_mode(g_vie_sys_cfg.mode_type, OT_VI_AIISP_MODE_DEFAULT) != TD_SUCCESS) {
        goto sys_exit;
    }

    return TD_SUCCESS;
sys_exit:
    sample_comm_sys_exit();
    return TD_FAILURE;
}

static td_s32 sample_smart_ae_vpss_set_wrap_grp_int_attr(ot_vi_vpss_mode_type mode_type,
    td_bool wrap_en, td_u32 max_height)
{
    td_s32 ret;
    if (mode_type == OT_VI_ONLINE_VPSS_ONLINE && wrap_en) {
        ot_frame_interrupt_attr frame_interrupt_attr;
        frame_interrupt_attr.interrupt_type = OT_FRAME_INTERRUPT_EARLY_END;
        frame_interrupt_attr.early_line = max_height / 2; /* 2 half */
        ret = ss_mpi_vpss_set_grp_frame_interrupt_attr(0, &frame_interrupt_attr);
        if (ret != TD_SUCCESS) {
            printf("ss_mpi_vpss_set_grp_frame_interrupt_attr failed!\n");
            return ret;
        }
    }
    return TD_SUCCESS;
}

static td_s32 sample_smart_ae_start_vpss(ot_vpss_grp grp, sample_vpss_cfg *vpss_cfg)
{
    td_s32 ret;
    sample_vpss_chn_attr vpss_chn_attr = {0};

    if (grp == 0) {
        ret = sample_smart_ae_vpss_set_wrap_grp_int_attr(g_vie_sys_cfg.mode_type, vpss_cfg->wrap_attr[0].enable,
            vpss_cfg->grp_attr.max_height);
        if (ret != TD_SUCCESS) {
            return ret;
        }
    }

    ret = memcpy_s(&vpss_chn_attr.chn_attr[0], sizeof(ot_vpss_chn_attr) * OT_VPSS_MAX_PHYS_CHN_NUM,
        vpss_cfg->chn_attr, sizeof(ot_vpss_chn_attr) * OT_VPSS_MAX_PHYS_CHN_NUM);
    if (ret != EOK) {
        sample_print("memcpy_s fail!\n");
        return TD_FAILURE;
    }
    ret = memcpy_s(vpss_chn_attr.chn_enable, sizeof(vpss_chn_attr.chn_enable),
        vpss_cfg->chn_en, sizeof(vpss_chn_attr.chn_enable));
    if (ret != EOK) {
        sample_print("memcpy_s fail!\n");
        return TD_FAILURE;
    }
    ret = memcpy_s(&vpss_chn_attr.wrap_attr[0], sizeof(ot_vpss_chn_buf_wrap_attr) * OT_VPSS_MAX_PHYS_CHN_NUM,
        vpss_cfg->wrap_attr, sizeof(ot_vpss_chn_buf_wrap_attr) * OT_VPSS_MAX_PHYS_CHN_NUM);
    if (ret != EOK) {
        sample_print("memcpy_s fail!\n");
        return TD_FAILURE;
    }
    vpss_chn_attr.chn_array_size = OT_VPSS_MAX_PHYS_CHN_NUM;
    ret = sample_common_vpss_start(grp, &vpss_cfg->grp_attr, &vpss_chn_attr);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    if (vpss_cfg->chn_en[1]) {
        const ot_low_delay_info low_delay_info = { TD_FALSE, 200, TD_FALSE }; /* 200: lowdelay line */
        if (ss_mpi_vpss_set_chn_low_delay(grp, 1, &low_delay_info) != TD_SUCCESS) {
            goto stop_vpss;
        }
    }

    ot_vpss_chn_attr chn_attr = {0};
    ss_mpi_vpss_get_chn_attr(0, 1, &chn_attr);
    chn_attr.depth = 2; /* 2 for dump frame */
    ss_mpi_vpss_set_chn_attr(0, 1, &chn_attr);

    return TD_SUCCESS;
stop_vpss:
    sample_common_vpss_stop(grp, vpss_cfg->chn_en, OT_VPSS_MAX_PHYS_CHN_NUM);
    return TD_FAILURE;
}

static td_void sample_smart_ae_stop_vpss(ot_vpss_grp grp)
{
    td_bool chn_enable[OT_VPSS_MAX_PHYS_CHN_NUM] = {TD_TRUE, TD_TRUE, TD_FALSE};

    sample_common_vpss_stop(grp, chn_enable, OT_VPSS_MAX_PHYS_CHN_NUM);
}

static td_s32 sample_smart_ae_start_venc(ot_venc_chn venc_chn[], ot_size venc_size[], size_t size, td_u32 chn_num)
{
    td_s32 i;
    td_s32 ret;

    if (chn_num > size) {
        return TD_FAILURE;
    }

    for (i = 0; i < (td_s32)chn_num; i++) {
        g_venc_chn_param.venc_size.width = venc_size[i].width;
        g_venc_chn_param.venc_size.height = venc_size[i].height;
        g_venc_chn_param.is_rcn_ref_share_buf = 1;
        ret = sample_comm_venc_start(venc_chn[i], &g_venc_chn_param);
        if (ret != TD_SUCCESS) {
            goto exit;
        }
    }

    ret = sample_comm_venc_start_get_stream(venc_chn, chn_num);
    if (ret != TD_SUCCESS) {
        goto exit;
    }

    return TD_SUCCESS;

exit:
    for (i = i - 1; i >= 0; i--) {
        sample_comm_venc_stop(venc_chn[i]);
    }
    return TD_FAILURE;
}

static td_void sample_smart_ae_stop_venc(ot_venc_chn venc_chn[], size_t size, td_u32 chn_num)
{
    td_u32 i;

    if (chn_num > size) {
        return;
    }

    sample_comm_venc_stop_get_stream(chn_num);

    for (i = 0; i < chn_num; i++) {
        sample_comm_venc_stop(venc_chn[i]);
    }
}

static td_s32 sample_smart_ae_start_and_bind_venc(ot_vpss_grp vpss_grp[], sample_vpss_cfg *vpss_cfg, size_t size,
    td_u32 grp_num)
{
    td_u32 i;
    td_s32 ret;
    ot_venc_chn venc_chn[VENC_MAX_CHN_NUM];
    ot_size venc_size[VENC_MAX_CHN_NUM];
    td_u32 chn_num;

    if (grp_num > size) {
        return TD_FAILURE;
    }

    for (i = 0; i < VENC_MAX_CHN_NUM; i++) {
        venc_chn[i] = (td_s32)i;
        if (i % 2 == 0) { /* 2: divisor */
            venc_size[i].width = vpss_cfg->chn_attr[0].width;
            venc_size[i].height = vpss_cfg->chn_attr[0].height;
        } else {
            venc_size[i].width = vpss_cfg->chn_attr[1].width;
            venc_size[i].height = vpss_cfg->chn_attr[1].height;
        }
    }

    chn_num = grp_num;
    if (g_vie_sys_cfg.vpss_wrap_en) {
        chn_num *= 2; /* 2 chns */
    }
    ret = sample_smart_ae_start_venc(venc_chn, venc_size, sizeof(venc_chn) / sizeof(venc_chn[0]), chn_num);
    if (ret != TD_SUCCESS) {
        return TD_FAILURE;
    }
    for (i = 0; i < grp_num; i++) {
        sample_comm_vpss_bind_venc(vpss_grp[i], OT_VPSS_CHN0, venc_chn[i * 2]); /* 2 chns */
        if (g_vie_sys_cfg.vpss_wrap_en) {
            sample_comm_vpss_bind_venc(vpss_grp[i], OT_VPSS_CHN1, venc_chn[i * 2 + 1]); /* 2 chns */
        }
    }

    return TD_SUCCESS;
}

static td_void sample_smart_ae_stop_and_unbind_venc(ot_vpss_grp vpss_grp[], size_t size, td_u32 grp_num)
{
    ot_venc_chn venc_chn[VENC_MAX_CHN_NUM];
    td_u32 i, chn_num;

    if (grp_num > size) {
        return;
    }

    for (i = 0; i < VENC_MAX_CHN_NUM; i++) {
        venc_chn[i] = (td_s32)i;
    }

    for (i = 0; i < grp_num; i++) {
        sample_comm_vpss_un_bind_venc(vpss_grp[i], OT_VPSS_CHN0, venc_chn[i * 2]); /* 2 chns */
        if (g_vie_sys_cfg.vpss_wrap_en) {
            sample_comm_vpss_un_bind_venc(vpss_grp[i], OT_VPSS_CHN1, venc_chn[i * 2 + 1]); /* 2 chns */
        }
    }

    chn_num = grp_num;
    if (g_vie_sys_cfg.vpss_wrap_en) {
        chn_num *= 2; /* 2 chns */
    }
    sample_smart_ae_stop_venc(venc_chn, sizeof(venc_chn) / sizeof(venc_chn[0]), chn_num);
}

static td_s32 sample_smart_ae_start_route(sample_vi_cfg *vi_cfg, sample_vpss_cfg *vpss_cfg, td_s32 route_num)
{
    td_s32 i, j, ret;
    ot_vpss_grp vpss_grp[SAMPLE_VIE_MAX_ROUTE_NUM] = {0, 1, 2, 3};

    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &g_vb_param.vb_size[0]);
    sample_comm_vi_get_size_by_sns_type(SENSOR0_TYPE, &g_vb_param.vb_size[1]);
    if (sample_smart_ae_sys_init() != TD_SUCCESS) {
        return TD_FAILURE;
    }

    for (i = 0; i < route_num; i++) {
        ret = sample_comm_vi_start_vi(&vi_cfg[i]);
        if (ret != TD_SUCCESS) {
            goto start_vi_failed;
        }
    }

    for (i = 0; i < route_num; i++) {
        sample_comm_vi_bind_vpss(i, 0, vpss_grp[i], 0);
    }

    for (i = 0; i < route_num; i++) {
        ret = sample_smart_ae_start_vpss(vpss_grp[i], vpss_cfg);
        if (ret != TD_SUCCESS) {
            goto start_vpss_failed;
        }
    }

    ret = sample_smart_ae_start_and_bind_venc(vpss_grp, vpss_cfg, SAMPLE_VIE_MAX_ROUTE_NUM, route_num);
    if (ret != TD_SUCCESS) {
        goto start_vpss_failed;
    }

    return TD_SUCCESS;

start_vpss_failed:
    for (j = i - 1; j >= 0; j--) {
        sample_smart_ae_stop_vpss(vpss_grp[j]);
    }
    for (i = 0; i < route_num; i++) {
        sample_comm_vi_un_bind_vpss(i, 0, vpss_grp[i], 0);
    }
start_vi_failed:
    for (j = i - 1; j >= 0; j--) {
        sample_comm_vi_stop_vi(&vi_cfg[j]);
    }
    sample_comm_sys_exit();
    return TD_FAILURE;
}

static td_void sample_smart_ae_stop_route(sample_vi_cfg *vi_cfg, td_s32 route_num)
{
    td_s32 i;
    ot_vpss_grp vpss_grp[SAMPLE_VIE_MAX_ROUTE_NUM] = {0, 1, 2, 3};

    sample_smart_ae_stop_and_unbind_venc(vpss_grp, SAMPLE_VIE_MAX_ROUTE_NUM, route_num);
    for (i = route_num - 1; i >= 0; i--) {
        sample_smart_ae_stop_vpss(vpss_grp[i]);
        sample_comm_vi_un_bind_vpss(i, 0, vpss_grp[i], 0);
        sample_comm_vi_stop_vi(&vi_cfg[i]);
    }
    sample_comm_sys_exit();
}

static td_void sample_smart_ae_get_vi_vpss_mode(td_bool is_wdr_mode)
{
    g_vie_sys_cfg.mode_type = OT_VI_OFFLINE_VPSS_OFFLINE;
    g_vb_param.blk_num[0] = is_wdr_mode ? 4 : 0; /* raw_vb num 4 or 0 */
}

static td_void sample_smart_ae_vpss_get_wrap_cfg(sampe_sys_cfg *vie_sys_cfg, sample_vpss_cfg *vpss_cfg)
{
    td_u32 full_lines_std;
    if (vie_sys_cfg->vpss_wrap_en) {
        if (SENSOR0_TYPE == HY006_3814_0011_MIPI_4M_30FPS_10BIT ||
            SENSOR0_TYPE == HY006_3814_0011_MIPI_3M_30FPS_10BIT ||
            SENSOR0_TYPE == HY006_3814_0011_MIPI_2M_30FPS_10BIT ||
            SENSOR0_TYPE == HY006_3814_0011_MIPI_4M_30FPS_10BIT_WDR2TO1 ||
            SENSOR0_TYPE == SC4336P_MIPI_4M_30FPS_10BIT ||
            SENSOR0_TYPE == SC4336P_MIPI_3M_30FPS_10BIT ||
            SENSOR0_TYPE == SC4336P_MIPI_2M_30FPS_10BIT) {
            full_lines_std = 1500; /* full_lines_std: 1500 */
        } else if (SENSOR0_TYPE == SC450AI_MIPI_4M_30FPS_10BIT || SENSOR0_TYPE == SC450AI_MIPI_4M_30FPS_10BIT_WDR2TO1) {
            full_lines_std = 1585; /* full_lines_std: 1585 */
        } else if (SENSOR0_TYPE == SC500AI_MIPI_5M_30FPS_10BIT || SENSOR0_TYPE == SC500AI_MIPI_5M_30FPS_10BIT_WDR2TO1) {
            full_lines_std = 1700; /* full_lines_std: 1700 */
        } else {
            vie_sys_cfg->vpss_wrap_en = TD_FALSE;
            vpss_cfg->wrap_attr[0].enable = TD_FALSE;
            return;
        }
        (td_void)sample_comm_vpss_get_wrap_cfg(vpss_cfg->chn_attr, vie_sys_cfg->mode_type, full_lines_std,
            &vpss_cfg->wrap_attr[0]);
        vie_sys_cfg->vpss_wrap_size = vpss_cfg->wrap_attr[0].buf_size;
    }
}

static sample_vi_cfg g_vi_cfg[1] = {0};
static td_s32 sample_smart_ae_start_video_route(td_void)
{
    sample_sns_type sns_type = SENSOR0_TYPE;

    sample_smart_ae_get_vi_vpss_mode(TD_FALSE);
    sample_comm_vi_get_default_vi_cfg(sns_type, &g_vi_cfg[0]);
    sample_smart_ae_get_pipe_wrap_line(g_vi_cfg, 1);
    sample_comm_vpss_get_default_vpss_cfg(sns_type, &g_vpss_cfg, g_vie_sys_cfg.vpss_wrap_en);
    g_vpss_cfg.chn_attr[1].width = 1024; /* model 1024x576 */
    g_vpss_cfg.chn_attr[1].height = 576; /* model 1024x576 */
    sample_smart_ae_vpss_get_wrap_cfg(&g_vie_sys_cfg, &g_vpss_cfg);

    if (sample_smart_ae_start_route(g_vi_cfg, &g_vpss_cfg, g_vie_sys_cfg.route_num) != TD_SUCCESS) {
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

static td_void sample_smart_ae_stop_video_route(td_void)
{
    sample_smart_ae_stop_route(g_vi_cfg, g_vie_sys_cfg.route_num);
}

static td_s32 sample_smart_ae_start(td_void)
{
    td_s32 ret;

    // start video route
    ret = sample_smart_ae_start_video_route();
    if (ret != TD_SUCCESS) {
        sample_print("start_video_route error!\n");
        return ret;
    }

    // get frame from VPSS and send to aidetect
    ret = sample_smart_ae_video_proc();
    if (ret != TD_SUCCESS) {
        sample_print("video_proc error!\n");
        goto exit;
    }

    if (g_detect_obj.pfn_destroy_chn != TD_NULL) {
        ret = g_detect_obj.pfn_destroy_chn(OT_SAMPLE_AIDETECT_CHN_0);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_aidetect_destroy_chn error: (0x%x)!\n", ret);
            goto exit;
        }
    }

exit:
    sample_smart_ae_stop_video_route();

    return ret;
}

static td_void sample_smart_ae_handle_sig(td_s32 signo)
{
    if (signo == SIGINT || signo == SIGTERM) {
        g_sig_flag = 1;
    }
}

static td_void sample_register_sig_handler(td_void (*sig_handle)(td_s32))
{
    struct sigaction sa;
    td_s32 ret;

    ret = memset_s(&sa, sizeof(struct sigaction), 0, sizeof(struct sigaction));
    if (ret != EOK) {
        sample_print("memset_s failed!\n");
        return;
    }
    sa.sa_handler = sig_handle;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, TD_NULL);
    sigaction(SIGTERM, &sa, TD_NULL);
}

static td_s32 sample_smart_ae_msg_proc_vb_pool_share(td_s32 pid)
{
    td_s32 ret;
    td_u32 i;
    td_bool isp_states[OT_VI_MAX_PIPE_NUM];
#ifndef SAMPLE_MEM_SHARE_ENABLE
    ot_vb_common_pools_id pools_id = {0};

    if (ss_mpi_vb_get_common_pool_id(&pools_id) != TD_SUCCESS) {
        sample_print("get common pool_id failed!\n");
        return TD_FAILURE;
    }

    for (i = 0; i < pools_id.pool_cnt; ++i) {
        if (ss_mpi_vb_pool_share(pools_id.pool[i], pid) != TD_SUCCESS) {
            sample_print("vb pool share failed!\n");
            return TD_FAILURE;
        }
    }
#endif
    ret = sample_comm_vi_get_isp_run_state(isp_states, OT_VI_MAX_PIPE_NUM);
    if (ret != TD_SUCCESS) {
        sample_print("get isp states fail\n");
        return TD_FAILURE;
    }

    for (i = 0; i < OT_VI_MAX_PIPE_NUM; i++) {
        if (!isp_states[i]) {
            continue;
        }
        ret = ss_mpi_isp_mem_share(i, pid);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_isp_mem_share vi_pipe %u, pid %d fail\n", i, pid);
        }
    }

    return TD_SUCCESS;
}

static td_void sample_smart_ae_msg_proc_vb_pool_unshare(td_s32 pid)
{
    td_s32 ret;
    td_u32 i;
    td_bool isp_states[OT_VI_MAX_PIPE_NUM];
#ifndef SAMPLE_MEM_SHARE_ENABLE
    ot_vb_common_pools_id pools_id = {0};
    if (ss_mpi_vb_get_common_pool_id(&pools_id) == TD_SUCCESS) {
        for (i = 0; i < pools_id.pool_cnt; ++i) {
            ret = ss_mpi_vb_pool_unshare(pools_id.pool[i], pid);
            if (ret != TD_SUCCESS) {
                sample_print("ss_mpi_vb_pool_unshare vi_pipe %u, pid %d fail\n", pools_id.pool[i], pid);
            }
        }
    }
#endif
    ret = sample_comm_vi_get_isp_run_state(isp_states, OT_VI_MAX_PIPE_NUM);
    if (ret != TD_SUCCESS) {
        sample_print("get isp states fail\n");
        return;
    }

    for (i = 0; i < OT_VI_MAX_PIPE_NUM; i++) {
        if (!isp_states[i]) {
            continue;
        }
        ret = ss_mpi_isp_mem_unshare(i, pid);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_isp_mem_unshare vi_pipe %u, pid %d fail\n", i, pid);
        }
    }
}

static td_s32 sample_smart_ae_ipc_msg_proc(const sample_ipc_msg_req_buf *msg_req_buf,
    td_bool *is_need_fb, sample_ipc_msg_res_buf *msg_res_buf)
{
    td_s32 ret;

    if (msg_req_buf == TD_NULL || is_need_fb == TD_NULL) {
        return TD_FAILURE;
    }

    /* need feedback default */
    *is_need_fb = TD_TRUE;

    switch ((sample_msg_type)msg_req_buf->msg_type) {
        case SAMPLE_MSG_TYPE_VB_POOL_SHARE_REQ: {
            if (msg_res_buf == TD_NULL) {
                return TD_FAILURE;
            }
            ret = sample_smart_ae_msg_proc_vb_pool_share(msg_req_buf->msg_data.pid);
            msg_res_buf->msg_type = SAMPLE_MSG_TYPE_VB_POOL_SHARE_RES;
            msg_res_buf->msg_data.is_req_success = (ret == TD_SUCCESS) ? TD_TRUE : TD_FALSE;
            break;
        }
        case SAMPLE_MSG_TYPE_VB_POOL_UNSHARE_REQ: {
            if (msg_res_buf == TD_NULL) {
                return TD_FAILURE;
            }
            sample_smart_ae_msg_proc_vb_pool_unshare(msg_req_buf->msg_data.pid);
            msg_res_buf->msg_type = SAMPLE_MSG_TYPE_VB_POOL_UNSHARE_RES;
            msg_res_buf->msg_data.is_req_success = TD_TRUE;
            break;
        }
        default: {
            printf("unsupported msg type(%ld)!\n", msg_req_buf->msg_type);
            return TD_FAILURE;
        }
    }
    return TD_SUCCESS;
}

static td_s32 sample_smart_ae_init_aidetect(int argc, char *argv[])
{
    if (sample_smart_ae_parase_param(argc, argv) != TD_SUCCESS) {
        sample_print("parase_param error, please check your param!\n");
        return TD_FAILURE;
    }

    if (sample_smart_ae_init_model() != TD_SUCCESS) {
        sample_print("init_model error!\n");
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

int main(int argc, char *argv[])
{
    td_s32 ret;
#ifndef __LITEOS__
    sample_register_sig_handler(sample_smart_ae_handle_sig);
#endif

    if (sample_ipc_server_init(sample_smart_ae_ipc_msg_proc) != TD_SUCCESS) {
        printf("sample_ipc_server_init failed!!!\n");
    }

    ret = sample_smart_ae_init_aidetect(argc, argv);
    if (ret != TD_SUCCESS) {
        sample_smart_ae_usage(argv[0]);
        sample_ipc_server_deinit();
        return ret;
    }

    ret = sample_smart_ae_start();

    sample_ipc_server_deinit();

    return ret;
}
