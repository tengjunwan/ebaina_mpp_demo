/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __FONT_H__
#define __FONT_H__

#include "ot_type.h"

#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#define FONT_ENCODE_SIZE 16
#define MAX_FONT_STR_LEN 128
#define RESIZE_H 3
#define RESIZE_W 3

td_s32 font_disp_bmp(const td_char *str_content, td_u8 *bmp_data);
#endif