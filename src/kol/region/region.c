/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <string.h>
#include "ot_common_region.h"
#include "sample_comm.h"
#include "loadbmp.h"
#include "font.h"
#include "region.h"

static td_u32 g_rect_handle_num[AIDETECT_MAX_ROUTE] = {0};
static td_u32 g_aidetect_rect_color[AIDETECT_MAX_CLASS_NUM] = {AIDETECT_RECT_BLUE, AIDETECT_RECT_YELLOW,
                                                               AIDETECT_RECT_ORANGE, AIDETECT_RECT_GREEN,
                                                               AIDETECT_RECT_TEAL, AIDETECT_RECT_RED,
                                                               AIDETECT_RECT_RED, AIDETECT_RECT_RED};

static td_u32 g_overlay_handle_num[MAX_ROUTE_NUM] = { 0, 1, 2, 3 };

static td_s32 g_overlay_min_handle = -1;
static td_s32 g_rect_min_handle = -1;

td_s32 region_create_overlay(td_u32 handle_num)
{
    td_s32 ret;
    td_u32 i, j;
    td_u32 min_handle;
    ot_rgn_attr region;

    region.type = OT_RGN_OVERLAY;
    region.attr.overlay.canvas_num = 2; /* 2: canvas_num */
    region.attr.overlay.pixel_format = OT_PIXEL_FORMAT_ARGB_1555;
    region.attr.overlay.size.width = 180; /* 180: rect width */
    region.attr.overlay.size.height = FONT_HEIGHT * RESIZE_H;
    region.attr.overlay.bg_color = 0xffff;

    g_overlay_min_handle = sample_comm_region_get_min_handle(OT_RGN_OVERLAY);
    if (sample_comm_check_min(g_overlay_min_handle) != TD_SUCCESS) {
        sample_print("g_overlay_min_handle(%d) should be in [0, %d).\n", g_overlay_min_handle, OT_RGN_HANDLE_MAX);
        return TD_FAILURE;
    }
    min_handle = (td_u32)g_overlay_min_handle;
    for (i = min_handle; i < min_handle + handle_num; i++) {
        ret = ss_mpi_rgn_create(i, &region);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_rgn_create failed with %#x!\n", ret);
            goto exit;
        }
    }
    return TD_SUCCESS;
exit:
    for (j = i - 1; j >= min_handle; j--) {
        (td_void)ss_mpi_rgn_destroy(j);
    }
    return ret;
}

td_void region_destroy_overlay(td_u32 handle_num)
{
    td_u32 min_handle = (td_u32)g_overlay_min_handle;
    for (td_u32 i = min_handle; i < min_handle + handle_num; i++) {
        (td_void)ss_mpi_rgn_destroy(i);
    }
}

static td_s32 region_attach_overlay(td_u32 route_id, td_s32 venc_chn)
{
    td_s32 ret;
    ot_rgn_chn_attr chn_attr;

    ot_mpp_chn chn;
    chn.mod_id = OT_ID_VENC;
    chn.dev_id = 0;
    chn.chn_id = venc_chn;

    chn_attr.is_show = TD_TRUE;
    chn_attr.type = OT_RGN_OVERLAY;
    td_u32 min_handle = (td_u32)g_overlay_min_handle;

    chn_attr.attr.overlay_chn.bg_alpha = 255; /** < 255:alpha value */
    chn_attr.attr.overlay_chn.fg_alpha = 255; /** < 255:alpha value */
    chn_attr.attr.overlay_chn.qp_info.enable = TD_TRUE;
    chn_attr.attr.overlay_chn.qp_info.is_abs_qp = TD_FALSE;
    chn_attr.attr.overlay_chn.qp_info.qp_val = 0;
    chn_attr.attr.overlay_chn.dst = OT_RGN_ATTACH_JPEG_MAIN;
    chn_attr.attr.overlay_chn.point.x = 50; /** 50 :point x */
    chn_attr.attr.overlay_chn.point.y = 50; /** 50 :point y */
    chn_attr.attr.overlay_chn.layer = g_overlay_handle_num[route_id];
    ret = ss_mpi_rgn_attach_to_chn(min_handle + g_overlay_handle_num[route_id], &chn, &chn_attr);
    if (ret != TD_SUCCESS) {
        sample_print("sample_region_attach failed!\n");
        ss_mpi_rgn_detach_from_chn(min_handle + g_overlay_handle_num[route_id], &chn);
        return ret;
    }

    return TD_SUCCESS;
}

static td_s32 region_set_bit_map(ot_rgn_handle handle, const td_char *str)
{
    td_u32 len = (td_u32)strlen(str);
    ot_bmp bmp = {0};
    bmp.pixel_format = OT_PIXEL_FORMAT_ARGB_1555;
    bmp.width = len * FONT_WIDTH * RESIZE_W;
    bmp.height = FONT_HEIGHT * RESIZE_H;
    bmp.data = (td_u8 *)malloc(bmp.width * bmp.height * BYTE_PER_PIX_1555);
    if (bmp.data == TD_NULL) {
        sample_print("malloc failed!\n");
        return TD_FAILURE;
    }
    font_disp_bmp(str, bmp.data);

    td_s32 ret = ss_mpi_rgn_set_bmp(handle, &bmp);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_rgn_set_bmp failed with %#x!\n", ret);
    }
    free(bmp.data);

    return ret;
}

static td_s32 region_set_overlay_attr(td_u32 route_id, const td_char *font_str)
{
    td_s32 ret;
    td_u32 min_handle;
    ot_rgn_attr rgn_attr;

    min_handle = (td_u32)g_overlay_min_handle;

    ret = ss_mpi_rgn_get_attr(min_handle + g_overlay_handle_num[route_id], &rgn_attr);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    rgn_attr.attr.overlay.size.width = (td_u32)strlen(font_str) * FONT_WIDTH * RESIZE_W;
    ret = ss_mpi_rgn_set_attr(min_handle + g_overlay_handle_num[route_id], &rgn_attr);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    return TD_SUCCESS;
}

static td_s32 region_overlay_detach(td_u32 route_id, td_s32 venc_chn)
{
    td_s32 ret;
    td_u32 min_handle;
    ot_mpp_chn chn;

    chn.mod_id = OT_ID_VENC;
    chn.dev_id = 0;
    chn.chn_id = venc_chn;

    min_handle = (td_u32)g_overlay_min_handle;
    ret = ss_mpi_rgn_detach_from_chn(min_handle + g_overlay_handle_num[route_id], &chn);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_rgn_detach_from_chn failed.\n");
        return ret;
    }

    return TD_SUCCESS;
}

td_s32 region_draw_font(td_u32 route_id, td_s32 venc_chn, const td_char *font_str)
{
    td_s32 ret;

    ret = region_overlay_detach(route_id, venc_chn);
    if (ret != TD_SUCCESS) {
        sample_print("region_overlay_detach failed with %#x!\n", ret);
        return ret;
    }

    ret = region_set_overlay_attr(route_id, font_str);
    if (ret != TD_SUCCESS) {
        sample_print("region_set_overlay_attr failed with %#x!\n", ret);
        return ret;
    }

    ret = region_attach_overlay(route_id, venc_chn);
    if (ret != TD_SUCCESS) {
        sample_print("region_attach_overlay failed!\n");
        return ret;
    }

    td_u32 min_handle = (td_u32)g_overlay_min_handle;

    ret = region_set_bit_map(min_handle + g_overlay_handle_num[route_id], font_str);
    if (ret != TD_SUCCESS) {
        sample_print("sample_comm_region_set_bit_map failed!\n");
        return ret;
    }

    return TD_SUCCESS;
}

td_s32 region_create_corner_rect(td_u32 handle_num)
{
    td_s32 ret;
    td_u32 i, j, min_handle;
    ot_rgn_attr region;

    g_rect_min_handle = sample_comm_region_get_min_handle(OT_RGN_CORNER_RECT);
    if (sample_comm_check_min(g_rect_min_handle) != TD_SUCCESS) {
        sample_print("g_rect_min_handle(%d) should be in [0, %d).\n", g_rect_min_handle, OT_RGN_HANDLE_MAX);
        return TD_FAILURE;
    }

    min_handle = (td_u32)g_rect_min_handle;
    region.type = OT_RGN_CORNER_RECT;
    for (i = min_handle; i < min_handle + handle_num; i++) {
        ret = ss_mpi_rgn_create(i, &region);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_rgn_create failed with 0x%x!\n", ret);
            goto exit;
        }
    }
    return TD_SUCCESS;

exit:
    for (j = i - 1; j >= min_handle; j--) {
        (td_void)ss_mpi_rgn_destroy(j);
    }
    return ret;
}

td_void region_destroy_corner_rect(td_u32 handle_num)
{
    td_u32 min_handle = (td_u32)g_rect_min_handle;
    for (td_u32 i = min_handle; i < min_handle + handle_num; i++) {
        (td_void)ss_mpi_rgn_destroy(i);
    }
}

td_s32 region_detach_rect(td_s32 route_id)
{
    td_s32 ret;
    td_u32 min_handle;
    ot_mpp_chn chn;

    chn.mod_id = OT_ID_VPSS;
    chn.dev_id = route_id;
    chn.chn_id = 0;

    min_handle = (td_u32)g_rect_min_handle;
    for (td_u32 i = min_handle; i < min_handle + g_rect_handle_num[route_id]; i++) {
        ret = ss_mpi_rgn_detach_from_chn(i, &chn);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_rgn_detach_from_chn %u failed[0x%x].\n", i, ret);
            return ret;
        }
    }
    g_rect_handle_num[route_id] = 0;

    return TD_SUCCESS;
}

td_s32 region_draw_rect(aidetect_rect *rect, td_u32 rect_size, td_s32 route_id)
{
    td_s32 ret;
    ot_mpp_chn chn;
    
    chn.mod_id = OT_ID_VPSS;
    chn.dev_id = route_id;
    chn.chn_id = 0;

    ret = region_detach_rect(route_id);
    if (ret != TD_SUCCESS) {
        sample_print("region_detach_rect failed with 0x%x!\n", ret);
        return ret;
    }

    ot_rgn_chn_attr chn_attr;
    chn_attr.is_show = TD_TRUE;
    chn_attr.type = OT_RGN_CORNER_RECT;

    td_u32 min_handle = (td_u32)g_rect_min_handle;
    for (td_u32 i = 0; i < rect_size; i++) {
        for (td_u32 j = 0; j < rect[i].rect_cnt; j++) {
            chn_attr.attr.corner_rect_chn.corner_rect.rect.height = rect[i].corner_rect[j].height;
            chn_attr.attr.corner_rect_chn.corner_rect.rect.width = rect[i].corner_rect[j].width;
            chn_attr.attr.corner_rect_chn.corner_rect.rect.x = rect[i].corner_rect[j].x;
            chn_attr.attr.corner_rect_chn.corner_rect.rect.y = rect[i].corner_rect[j].y;
            chn_attr.attr.corner_rect_chn.corner_rect.hor_len = CORNER_RECT_HOR_LEN;
            chn_attr.attr.corner_rect_chn.corner_rect.ver_len = CORNER_RECT_VER_LEN;
            chn_attr.attr.corner_rect_chn.corner_rect_attr.color = g_aidetect_rect_color[i];
            chn_attr.attr.corner_rect_chn.corner_rect_attr.corner_rect_type = OT_CORNER_RECT_TYPE_FULL_LINE;
            chn_attr.attr.corner_rect_chn.corner_rect.thick = CORNER_RECT_LINE_THICK;
            chn_attr.attr.corner_rect_chn.layer = 0;

            ret = ss_mpi_rgn_attach_to_chn(min_handle + g_rect_handle_num[route_id], &chn, &chn_attr);
            if (ret != TD_SUCCESS) {
                sample_print("ss_mpi_rgn_attach_to_chn %u failed[0x%x].\n",
                    min_handle + g_rect_handle_num[route_id], ret);
                continue;
            }
            g_rect_handle_num[route_id]++;
            if (g_rect_handle_num[route_id] >= OT_RGN_VPSS_MAX_CORNER_RECT_NUM) {
                return TD_SUCCESS;
            }
        }
    }

    return TD_SUCCESS;
}
