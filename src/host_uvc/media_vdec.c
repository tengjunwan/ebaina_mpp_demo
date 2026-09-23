/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "media_vdec.h"
#include <unistd.h>


static td_u32 g_input_width;
static td_u32 g_input_height;
static FILE* g_save_file;


td_s32 sample_uvc_media_init(const td_char *type_name, td_u32 width, td_u32 height, const td_char *file_name)
{
    if (type_name == TD_NULL || file_name == NULL) {
        printf("NULL type name or file name\n");
        return TD_FAILURE;
    }
    g_input_width = width;
    g_input_height = height;
    g_save_file = fopen(file_name, "w+");
    if (g_save_file == TD_NULL) {
        printf("create file failed, errno %d\n", errno);
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

td_s32 sample_uvc_media_exit(td_void)
{
    if (g_save_file != TD_NULL) {
        fclose(g_save_file);
        g_save_file = TD_NULL;
    }
    return TD_SUCCESS;
}

td_s32 sample_uvc_media_send_data(td_void *data, td_u32 size, td_u32 stride,
    const ot_size *pic_size, const td_char *type_name)
{
    size_t total = 0;
    size_t data_size = size;

    if (data == NULL || pic_size == NULL || type_name == NULL) {
        sample_print("data, pic_size or type_name is NULL\n");
        return TD_FAILURE;
    }

    if (g_save_file == TD_NULL) {
        return TD_FAILURE;
    }
    while (total < data_size) {
        size_t ret = fwrite(data, 1, data_size - total, g_save_file);
        if (ret > 0) {
            total += ret;
        } else {
            printf("write file return %u, failed errno %d\n", ret, errno);
            break;
        }
    }

    return TD_SUCCESS;
}

td_s32 sample_uvc_media_stop_receive_data(td_void)
{
    return TD_SUCCESS;
}
