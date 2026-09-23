/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "font.h"
#include "asc16.dat"

static td_void font_fill_bitmap(td_bool is_text, td_u16 *pixel)
{
    if (is_text) {
        *pixel = 0;
    } else {
        *pixel = 0xffff;
    }
}

static td_void process_pixel(td_u16 **pixel, td_char str_ptr, td_u32 height)
{
    td_u8 lib = asc16[(td_u32)str_ptr * FONT_ENCODE_SIZE + height];
    for (td_u32 w = 0; w < FONT_WIDTH; w++) {
        td_bool is_text = (lib >> (7 - w)) & 0x1;
        for (td_u32 i = 0; i < RESIZE_W; i++) {
            font_fill_bitmap(is_text, *pixel);
            (*pixel)++;
        }
    }
}

td_s32 font_disp_bmp(const td_char *str_content, td_u8 *bmp_data)
{
    td_u16 *pixel = (td_u16 *)bmp_data;
    for (td_u32 h = 0; h < FONT_HEIGHT; h++) {
        for (td_u32 n = 0; n < RESIZE_H; n++) {
            const td_char *str_ptr = str_content;
            while (*str_ptr != '\0') {
                process_pixel(&pixel, *str_ptr, h);
                str_ptr++;
            }
        }
    }

    return TD_SUCCESS;
}