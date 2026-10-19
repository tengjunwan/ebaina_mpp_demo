/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "sample_comm.h"
#include "ss_mpi_bla.h"
#include "ss_mpi_sys_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IMG_NUM 11
#define CALIB_IMG_HEIGHT 448
#define CALIB_IMG_WIDTH 800

const char *g_slave_image_paths[IMG_NUM] = {
    "./data/slave-chn1-w800-h448-x190-y10.nv12", "./data/slave-chn1-w800-h448-x200-y15.nv12",
    "./data/slave-chn1-w800-h448-x210-y20.nv12", "./data/slave-chn1-w800-h448-x220-y25.nv12",
    "./data/slave-chn1-w800-h448-x230-y30.nv12", "./data/slave-chn1-w800-h448-x240-y35.nv12",
    "./data/slave-chn1-w800-h448-x250-y40.nv12", "./data/slave-chn1-w800-h448-x260-y45.nv12",
    "./data/slave-chn1-w800-h448-x270-y50.nv12", "./data/slave-chn1-w800-h448-x280-y55.nv12",
    "./data/slave-chn1-w800-h448-x290-y60.nv12"};
const char *g_master_image_paths[IMG_NUM] = {
    "./data/master-chn1-w800-h448-x190-y10.nv12", "./data/master-chn1-w800-h448-x200-y15.nv12",
    "./data/master-chn1-w800-h448-x210-y20.nv12", "./data/master-chn1-w800-h448-x220-y25.nv12",
    "./data/master-chn1-w800-h448-x230-y30.nv12", "./data/master-chn1-w800-h448-x240-y35.nv12",
    "./data/master-chn1-w800-h448-x250-y40.nv12", "./data/master-chn1-w800-h448-x260-y45.nv12",
    "./data/master-chn1-w800-h448-x270-y50.nv12", "./data/master-chn1-w800-h448-x280-y55.nv12",
    "./data/master-chn1-w800-h448-x290-y60.nv12"};
const char* g_calibration_info_paths = "./calibration_info-w800-h448.bin";

ot_point_f g_master_coords[IMG_NUM] = {0};
ot_point_f g_slave_coords[IMG_NUM] = {{190, 10}, {200, 15}, {210, 20}, {220, 25},
                                      {230, 30}, {240, 35}, {250, 40}, {260, 45},
                                      {270, 50}, {280, 55}, {290, 60}};
td_bool g_valid[IMG_NUM] = {0};
td_float g_ratio = 0.4;

static td_void sample_bla_read_frame(FILE *fp, td_u8 *virt_addr, td_s32 width, td_s32 height, td_s32 stride)

{
    if (feof(fp)) {
        fseek(fp, 0, SEEK_SET);
    }

    td_u8 *data = TD_NULL;
    td_s32 row = 0;
    data = virt_addr;
    td_s32 ret = -1;
    for (row = 0; row < height; row++, data += stride) {
        ret = (td_s32)fread(data, width, 1, fp);
        if (ret <= 0) {
            printf("pY fread failed.\n");
        }
    }
}

td_s32 sample_bla_malloc_mmz(ot_video_frame *master_frame, ot_video_frame *slave_frame)
{
    td_s32 ret;
    size_t size;

    slave_frame->height = CALIB_IMG_HEIGHT;
    slave_frame->width = CALIB_IMG_WIDTH;
    slave_frame->stride[0] = CALIB_IMG_WIDTH;

    master_frame->height = CALIB_IMG_HEIGHT;
    master_frame->width = CALIB_IMG_WIDTH;
    master_frame->stride[0] = CALIB_IMG_WIDTH;

    size = CALIB_IMG_HEIGHT * CALIB_IMG_WIDTH * sizeof(td_u8);
    ret = ss_mpi_sys_mmz_alloc_cached(&master_frame->phys_addr[0], &master_frame->virt_addr[0],
        "bla_master_frame", TD_NULL, size);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    ret = ss_mpi_sys_mmz_alloc_cached(&slave_frame->phys_addr[0], &slave_frame->virt_addr[0],
        "bla_slave_frame", TD_NULL, size);
    if (ret != TD_SUCCESS) {
        ss_mpi_sys_mmz_free(master_frame->phys_addr[0], master_frame->virt_addr);
        return ret;
    }

    master_frame->compress_mode = OT_COMPRESS_MODE_NONE;
    master_frame->pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    master_frame->dynamic_range = OT_DYNAMIC_RANGE_SDR8;
    slave_frame->compress_mode = OT_COMPRESS_MODE_NONE;
    slave_frame->pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    slave_frame->dynamic_range = OT_DYNAMIC_RANGE_SDR8;

    return TD_SUCCESS;
}

td_s32 sample_bla_free_mmz(ot_video_frame *master_frame, ot_video_frame *slave_frame)
{
    ss_mpi_sys_mmz_free(master_frame->phys_addr[0], master_frame->virt_addr);
    ss_mpi_sys_mmz_free(slave_frame->phys_addr[0], slave_frame->virt_addr);

    return TD_SUCCESS;
}

td_void sample_bla_correct_calibration_info(td_s32 num, ot_bla_calibration_info *calib_info, td_s32 *valid_num)
{
    td_s32 i, index;

    index = 0;
    for (i = 0; i < num; i++) {
        if (g_valid[i] == TD_TRUE) {
            calib_info->master_coords[index] = calib_info->master_coords[i];
            calib_info->slave_coords[index] = calib_info->slave_coords[i];
            index++;
        }
    }
    *valid_num = index;
    return;
}

td_s32 sample_bla_prepare_frame(ot_video_frame *master_frame, ot_video_frame *slave_frame, td_s32 i)
{
    FILE *fp_master_image = TD_NULL;
    FILE *fp_slave_image = TD_NULL;
    td_char real_file_name[PATH_MAX + 1] = {0};
    size_t size;
    td_s32 ret;

    size = CALIB_IMG_HEIGHT * CALIB_IMG_WIDTH * sizeof(td_u8);
    if ((strlen(g_slave_image_paths[i]) > PATH_MAX) ||
        (realpath(g_slave_image_paths[i], real_file_name) == TD_NULL)) {
        printf("model_file_name realpath err!\n");
        return TD_FAILURE;
    }
    fp_slave_image = fopen(real_file_name, "rb");
    if (fp_slave_image == TD_NULL) {
        printf("open path of %s failed.\n", real_file_name);
        return TD_FAILURE;
    }
    (td_void)memset_s((td_u8 *)slave_frame->virt_addr[0], size, 0, size);
    sample_bla_read_frame(fp_slave_image, (td_u8 *)slave_frame->virt_addr[0], slave_frame->width,
        slave_frame->height, slave_frame->stride[0]);
    ret = fclose(fp_slave_image);
    if (ret != 0) {
        printf("fp_slave_image close fp err, ret = 0x%x\n", ret);
    }

    // ----------read master img----------------
    printf("Read master image: %d\n", i);
    if ((strlen(g_master_image_paths[i]) > PATH_MAX) ||
        (realpath(g_master_image_paths[i], real_file_name) == TD_NULL)) {
        printf("model_file_name realpath err!\n");
        return TD_FAILURE;
    }
    fp_master_image = fopen(real_file_name, "rb");
    if (fp_master_image == TD_NULL) {
        printf("open path of %s failed.\n", real_file_name);
        return TD_FAILURE;
    }
    (td_void)memset_s((td_u8 *)master_frame->virt_addr[0], size, 0, size);
    sample_bla_read_frame(fp_master_image, (td_u8 *)master_frame->virt_addr[0], master_frame->width,
        master_frame->height, master_frame->stride[0]);
    ret = fclose(fp_master_image);
    if (ret != 0) {
        printf("fp_master_image close fp err, ret = 0x%x\n", ret);
    }
    return TD_SUCCESS;
}

#ifdef BLA_SUPPORT
static td_s32 sample_bla_save_calibration_info(ot_bla_calibration_info *calib_info, td_s32 valid_num)
{
    FILE *fp_calibration_info = TD_NULL;
    td_s32 ret;

    fp_calibration_info = fopen(g_calibration_info_paths, "wb");
    if (fp_calibration_info == TD_NULL) {
        printf("open path of %s failed.\n", g_calibration_info_paths);
        return TD_FAILURE;
    }

    if (feof(fp_calibration_info)) {
        fseek(fp_calibration_info, 0, SEEK_SET);
    }

    ret = fwrite(&g_ratio, sizeof(td_float), 1, fp_calibration_info);
    if (ret != 1) {
        printf("fp_calibration_info write ratio fp err, ret = 0x%x\n", ret);
        goto exit;
    }
    ret = fwrite(&valid_num, sizeof(td_s32), 1, fp_calibration_info);
    if (ret != 1) {
        printf("fp_calibration_info write valid_num fp err, ret = 0x%x\n", ret);
        goto exit;
    }

    ret = fwrite(calib_info->master_coords, sizeof(ot_point_f), valid_num, fp_calibration_info);
    if (ret != valid_num) {
        printf("fp_calibration_info write master_coords fp err, ret = 0x%x\n", ret);
        goto exit;
    }
    ret = fwrite(calib_info->slave_coords, sizeof(ot_point_f), valid_num, fp_calibration_info);
    if (ret != valid_num) {
        printf("fp_calibration_info write motor_coords fp err, ret = 0x%x\n", ret);
        goto exit;
    }

    ret = TD_SUCCESS;
exit:
    ret = fclose(fp_calibration_info);
    if (ret != 0) {
        printf("fp_calibration_info close fp err, ret = 0x%x\n", ret);
    }

    return ret;
}

static td_s32 sample_bla_load_calibration_info(ot_bla_calibration_info *calib_info, td_s32 *valid_num)
{
    FILE *fp_calibration_info = TD_NULL;
    td_char real_file_name[PATH_MAX + 1] = {0};
    td_s32 ret;

    if ((strlen(g_calibration_info_paths) > PATH_MAX) ||
        (realpath(g_calibration_info_paths, real_file_name) == TD_NULL)) {
        printf("model_file_name realpath err!\n");
        return TD_FAILURE;
    }
    fp_calibration_info = fopen(real_file_name, "rb");
    if (fp_calibration_info == TD_NULL) {
        printf("open path of %s failed.\n", real_file_name);
        return TD_FAILURE;
    }

    if (feof(fp_calibration_info)) {
        fseek(fp_calibration_info, 0, SEEK_SET);
    }

    ret = fread(&g_ratio, sizeof(td_float), 1, fp_calibration_info);
    if (ret != 1) {
        printf("fp_calibration_info read ratio fp err, ret = 0x%x\n", ret);
        goto exit;
    }
    ret = fread(valid_num, sizeof(td_s32), 1, fp_calibration_info);
    if (ret != 1) {
        printf("fp_calibration_info read valid_num fp err, ret = 0x%x\n", ret);
        goto exit;
    }
    if (*valid_num <= 0) {
        printf("read valid_num %d is illagal.\n", *valid_num);
        goto exit;
    }

    ret = fread(calib_info->master_coords, sizeof(ot_point_f), *valid_num, fp_calibration_info);
    if (ret != *valid_num) {
        printf("fp_calibration_info read master_coords fp err, ret = 0x%x\n", ret);
        goto exit;
    }
    ret = fread(calib_info->slave_coords, sizeof(ot_point_f), *valid_num, fp_calibration_info);
    if (ret != *valid_num) {
        printf("fp_calibration_info read motor_coords fp err, ret = 0x%x\n", ret);
        goto exit;
    }

    ret = TD_SUCCESS;
exit:
    ret = fclose(fp_calibration_info);
    if (ret != 0) {
        printf("fp_calibration_info close fp err, ret = 0x%x\n", ret);
    }
    return ret;
}

static td_s32 sample_bla_calibration()
{
    td_s32 i, ret, valid_num;
    ot_bla_calibration_info calib_info = {0};
    calib_info.master_coords = g_master_coords;
    calib_info.slave_coords = g_slave_coords;

    ot_video_frame slave_frame;
    ot_video_frame master_frame;

    ret = sample_bla_malloc_mmz(&master_frame, &slave_frame);
    if (ret != TD_SUCCESS) {
        printf("malloc mmz failed, ret = 0x%x\n", ret);
        return ret;
    }

    printf("\n==================Calibrate Start=====================\n");
    for (i = 0; i < IMG_NUM; i++) {
        printf("\n=============================\n");
        printf("Read slave image: %d\n", i);
        ret = sample_bla_prepare_frame(&master_frame, &slave_frame, i);
        if (ret != TD_SUCCESS) {
            sample_bla_free_mmz(&master_frame, &slave_frame);
            return ret;
        }

        // calibrate, must change image pair if fail, otherwise will make a endless loop!!!
        ret = ss_mpi_bla_calibration(&master_frame, &slave_frame, g_ratio, &g_master_coords[i]);
        if (ret != TD_SUCCESS) {
            printf("ss_mpi_bla_calibration fail, ret = 0x%x\n", ret);
            sample_bla_free_mmz(&master_frame, &slave_frame);
            return ret;
        }
    }
    sample_bla_free_mmz(&master_frame, &slave_frame);
    ss_mpi_bla_check_calibration_info(IMG_NUM, &calib_info, g_valid);
    sample_bla_correct_calibration_info(IMG_NUM, &calib_info, &valid_num);
    for (i = 0; i < valid_num; ++i) {
        printf("arch point: [%lf, %lf]\n", g_master_coords[i].x, g_master_coords[i].y);
    }
    printf("==================Calibrate Done=====================\n");

    sample_bla_save_calibration_info(&calib_info, valid_num);

    return TD_SUCCESS;
}

static td_s32 sample_bla_get_master_coords(ot_point_f *master_coords)
{
    td_char input[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* max_len: 10 */
    td_s32 max_len = 10; /* max_len: 10 */
    size_t i;
    td_char *para_stop, *input_para;

    printf("input the master coords: \n");
    if (gets_s(input, max_len) == TD_NULL || strlen(input) < 3) { /* 3 is the least param */
        printf("please input the master coords. e.g.\n");
        printf("input the master coords: 225 50\n");
        return TD_FAILURE;
    }

    for (i = 0; i < strlen(input); i++) {
        if (!check_digit(input[i]) && (input[i] != ' ') && (input[i] != '\n')) {
            printf("please input the master coords. e.g.\n");
            printf("input the master coords: 225 50\n");
            return TD_FAILURE;
        }
    }

    input_para = &input[0];
    master_coords->x = (td_float)(td_u32)strtol(input_para, &para_stop, 10); /* 10 dec */
    input_para = para_stop;
    master_coords->y = (td_float)(td_u32)strtol(input_para, &para_stop, 10); /* 10 dec */
    if (input_para == para_stop) {
        printf("please input the master coords. e.g.\n");
        printf("input the master coords: 225 50\n");
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

static td_s32 sample_bla_calculate()
{
    td_s32  ret, valid_num;
    ot_bla_calibration_info calib_info = {0};
    calib_info.master_coords = g_master_coords;
    calib_info.slave_coords = g_slave_coords;
    ot_point_f master_coords = {0, 0};
    ot_point_f slave_motor_coords;

    ret = sample_bla_get_master_coords(&master_coords);
    if (ret != TD_SUCCESS) {
        printf("sample_bla_get_master_coords fail, ret = 0x%x\n", ret);
        return ret;
    }

    ret = sample_bla_load_calibration_info(&calib_info, &valid_num);
    if (ret != TD_SUCCESS) {
        printf("sample_bla_load_calibration_info fail, ret = 0x%x\n", ret);
        return ret;
    }
    ot_point_f master_coords_resize;
    master_coords_resize.x = master_coords.x * g_ratio;
    master_coords_resize.y = master_coords.y * g_ratio;

    ss_mpi_bla_calculate_motor_coord(valid_num, &calib_info, &master_coords_resize, &slave_motor_coords);
    // check the Validity of slave_motor_coords, develop by customer
    printf("master_coords: [%f, %f]\n", master_coords.x, master_coords.y);
    printf("slave_motor_coords: [%lf, %lf]\n", slave_motor_coords.x, slave_motor_coords.y);

    return TD_SUCCESS;
}
#endif

static td_void sample_bla_usage(const char *prg_name)
{
    printf("usage : %s <index> \n", prg_name);
    printf("index:\n");
    printf("    (0) calibration motor coords.\n");
    printf("    (1) calculate motor coords.\n");
}

static td_s32 sample_bla_execute_case(td_u32 case_index)
{
    td_s32 ret = TD_SUCCESS;
#ifdef BLA_SUPPORT
    switch (case_index) {
        case 0: /* 0 calibration motor coords */
            ret = sample_bla_calibration();
            break;
        case 1: /* 1 calculate motor coords */
            ret = sample_bla_calculate();
            break;
        default:
            ret = TD_FAILURE;
            break;
    }
#endif
    return ret;
}

int main(int argc, char *argv[])
{
    td_s32 ret;
    td_u32 index;
    td_char *end_ptr = TD_NULL;

    if (argc != 2) { /* 2:arg num */
        sample_bla_usage(argv[0]);
        return TD_FAILURE;
    }

    if (!strncmp(argv[1], "-h", 2)) { /* 2:arg num */
        sample_bla_usage(argv[0]);
        return TD_FAILURE;
    }

    if (strlen(argv[1]) > 1 || strlen(argv[1]) == 0 || !check_digit(argv[1][0])) { /* 2:arg len */
        sample_bla_usage(argv[0]);
        return TD_FAILURE;
    }

    index = (td_u32)strtol(argv[1], &end_ptr, 10); /* base 10 */
    if ((end_ptr == argv[1]) || (*end_ptr) != '\0') {
        sample_bla_usage(argv[0]);
        return TD_FAILURE;
    }

    ret = sample_bla_execute_case(index);
    if (ret == TD_SUCCESS) {
        printf("\033[0;32mprogram exit normally!\033[0;39m\n");
    } else {
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    return TD_SUCCESS;
}

#ifdef __cplusplus
}
#endif