/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __REGION_H__
#define __REGION_H__

#include "ot_type.h"
#include "algorithm.h"

#define RGN_HANDLE_NUM_1 1
#define RGN_HANDLE_NUM_10 10
#define BITS_NUM_PER_BYTE 8
#define STRIDE_ALIGN 4
#define MAX_BIT_COUNT 32
#define BITS_NUM_PER_BYTE 8
#define BYTE_PER_PIX_1555 2
#define BYTE_PER_PIX_8888 4
#define PIX_PER_BYTE_CLUT2 4
#define PIX_PER_BYTE_CLUT4 2
#define MAX_ROUTE_NUM 4

td_s32 region_create_overlay(td_u32 handle_num);
td_s32 region_draw_font(td_u32 route_id, td_s32 venc_chn, const td_char *font_str);
td_void region_destroy_overlay(td_u32 handle_num);
td_s32 region_create_corner_rect(td_u32 handle_num);
td_s32 region_draw_rect(aidetect_rect *rect, td_u32 rect_size, td_s32 route_id);
td_void region_destroy_corner_rect(td_u32 handle_num);
td_s32 region_detach_rect(td_s32 route_id);
#endif