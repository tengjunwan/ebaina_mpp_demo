/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "sample_aibnr.h"
#include "sample_aiisp_common.h"
#include "ss_mpi_aibnr.h"

#define VB_AIBNR_LINE_CNT 5
#define VB_AIBNR_TNR_CNT 5
#define VB_AIBNR_COMMON_VB_CNT 1

static td_u16 g_r_calib_lut[OT_AIBNR_NLC_LUT_NUM] = {
    1043, 1043, 1029, 1024, 1021, 1020, 1019, 1018, 1017, 1017, 1016,
    1015, 1014, 1014, 1013, 1013, 1012, 1012, 1012, 1011, 1011, 1011,
    1010, 1010, 1010, 1010, 1009, 1009, 1008, 1008, 1008, 1000
};

static td_u16 g_b_calib_lut[OT_AIBNR_NLC_LUT_NUM] = {
    1024, 1024, 1027, 1028, 1029, 1029, 1029, 1029, 1027, 1024, 1021,
    1019, 1018, 1016, 1015, 1013, 1011, 1009, 1008, 1006, 1005, 1004,
    1004, 1004, 1003, 1003, 1003, 1002, 1002, 1002, 1002, 1000
};

static td_s32 sample_aibnr_set_nlc(ot_aibnr_nlc *nlc)
{
    td_s32 ret;

    nlc->enable = TD_FALSE;
    nlc->step_bit = 0x3;

    ret = memcpy_s(nlc->r_fact, sizeof(nlc->r_fact), g_r_calib_lut, sizeof(g_r_calib_lut));
    if (ret != EOK) {
        printf("memcpy_s failed!\n");
        return TD_FAILURE;
    }
    ret = memcpy_s(nlc->b_fact, sizeof(nlc->b_fact), g_b_calib_lut, sizeof(g_b_calib_lut));
    if (ret != EOK) {
        printf("memcpy_s failed!\n");
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

static td_s32 sample_aibnr_set_model_attr(td_void)
{
    td_s32 ret;
    ot_aibnr_model_list list = {0};
    ot_aibnr_model_attr model_attr = {0};

    ret = ss_mpi_aibnr_query_model_list(&list);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_aibnr_query_model_list error\n");
        return ret;
    }

    printf("model list info:\n");
    printf("num:%u\n", list.num);

    for (td_u32 i = 0; i < list.num && i < OT_AIISP_MAX_MODEL_NUM; i++) {
        printf("id:%u  is wdr:%d\n", list.info[i].id, list.info[i].is_wdr_mode);
    }

    sample_get_char("select model 0");

    model_attr.op_type = OT_OP_MODE_MANUAL;
    model_attr.manual_id = 0;
    ss_mpi_aibnr_set_model_attr(0, &model_attr);

    sample_get_char("select model 1");

    model_attr.op_type = OT_OP_MODE_MANUAL;
    model_attr.manual_id = 1;
    ss_mpi_aibnr_set_model_attr(0, &model_attr);

    sample_get_char("set auto mode");

    model_attr.op_type = OT_OP_MODE_AUTO;
    for (td_u32 i = 0; i < OT_AIISP_AUTO_ISO_NUM; i++) {
        model_attr.auto_id[i] = 1;
        if (i < 0x2) {
            model_attr.auto_id[i] = 0;
        }
    }
    ss_mpi_aibnr_set_model_attr(0, &model_attr);

    return TD_SUCCESS;
}

static td_s32 sample_aibnr_set_attr(ot_vi_pipe vi_pipe, sample_aibnr_param *aibnr_param)
{
    td_s32 ret;
    ot_aibnr_attr aibnr_attr;

    ret = ss_mpi_aibnr_get_attr(vi_pipe, &aibnr_attr);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_aibnr_get_attr error\n");
        return ret;
    }

    aibnr_attr.enable = TD_TRUE;
    aibnr_attr.bnr_bypass = TD_FALSE;
    aibnr_attr.blend = aibnr_param->is_blend;
    aibnr_attr.op_type = OT_OP_MODE_MANUAL;
    aibnr_attr.manual_attr.sfs = 31; /* sfs: 31 */
    ret = sample_aibnr_set_nlc(&aibnr_attr.manual_attr.nlc);
    if (ret != TD_SUCCESS) {
        printf("call sample_aibnr_set_nlc failed!\n");
        return TD_FAILURE;
    }

    ret = ss_mpi_aibnr_set_attr(vi_pipe, &aibnr_attr);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_aibnr_set_attr error\n");
        return ret;
    }

    if (aibnr_param->is_pro == TD_FALSE) {
        sample_aibnr_set_model_attr();
    }

    return TD_SUCCESS;
}

static td_s32 sample_aibnr_load_model(ot_aibnr_model *model_info, td_s32 *model_id, td_char *model_file)
{
    td_s32 ret;

    ot_aiisp_model *model = &model_info->model;

    if (model->mem_info.virt_addr == TD_NULL) {
        ret = sample_aiisp_load_mem((ot_aiisp_mem_info *)&(model->mem_info), model_file);
        if (ret != TD_SUCCESS) {
            sample_print("sample_aiisp_load_mem error\n");
            return ret;
        }
    }

    ret = ss_mpi_aibnr_load_model(model_info, model_id);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_aibnr_load_model error(%#x)\n", ret);
        goto unload_mem;
    }

    return ret;

unload_mem:
    sample_aiisp_unload_mem((ot_aiisp_mem_info *)&(model->mem_info));
    return ret;
}

static td_s32 sample_aibnr_unload_model(ot_aibnr_model *model, td_s32 model_id)
{
    td_s32 ret;

    ret = ss_mpi_aibnr_unload_model(model_id);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_aibnr_unload_cfg error(%#x)\n", ret);
    }

    sample_aiisp_unload_mem((ot_aiisp_mem_info *)&(model->model.mem_info));

    return ret;
}

static td_s32 sample_aibnr_load(ot_size in_size, ot_aibnr_model *model_info, td_s32 *model_id,
    sample_aibnr_param *aibnr_param)
{
    td_s32 ret;
    td_char model_file[FILE_NAME_LEN];

    if (aibnr_param->is_pro == TD_FALSE) {
        ret = snprintf_s(model_file, FILE_NAME_LEN, FILE_NAME_LEN - 1, "./aibnr/aibnr_model_%ux%u.bin",
            in_size.width, in_size.height);
    } else {
        ret = snprintf_s(model_file, FILE_NAME_LEN, FILE_NAME_LEN - 1, "./aibnr/aibnr_model_%ux%u_pro.bin",
            in_size.width, in_size.height);
    }
    if (ret < 0) {
        printf("set name failed!\n");
        return ret;
    }

    model_info[0].model.preempted_en = TD_FALSE;
    model_info[0].model.image_size.width  = in_size.width;
    model_info[0].model.image_size.height = in_size.height;
    model_info[0].is_wdr_mode = aibnr_param->is_wdr_mode;
    model_info[0].ref_mode = aibnr_param->ref_mode;
    model_info[0].nlc_en = TD_FALSE;
    model_info[0].is_wdr_mode = TD_FALSE;
    ret = sample_aibnr_load_model(&model_info[0], &model_id[0], model_file);
    if (ret != TD_SUCCESS) {
        return ret;
    }

    if (aibnr_param->is_pro == TD_FALSE) {
        ret = snprintf_s(model_file, FILE_NAME_LEN, FILE_NAME_LEN - 1, "./aibnr/aibnr_model_%ux%u_high_iso.bin",
            in_size.width, in_size.height);
        if (ret < 0) {
            printf("set name failed!\n");
            goto unload;
        }

        model_info[1].model.preempted_en = TD_FALSE;
        model_info[1].model.image_size.width  = in_size.width;
        model_info[1].model.image_size.height = in_size.height;
        model_info[1].is_wdr_mode = aibnr_param->is_wdr_mode;
        model_info[1].ref_mode = aibnr_param->ref_mode;
        model_info[1].nlc_en = TD_FALSE;
        model_info[1].is_wdr_mode = TD_FALSE;
        ret = sample_aibnr_load_model(&model_info[1], &model_id[1], model_file);
        if (ret != TD_SUCCESS) {
            goto unload;
        }
    }

    return TD_SUCCESS;

unload:
    sample_aibnr_unload_model(&model_info[0], model_id[0]);

    return TD_FAILURE;
}

static td_s32 sample_aibnr_start(ot_vi_pipe vi_pipe, ot_size in_size, ot_aibnr_model *model_info, td_s32 *model_id,
    td_s32 model_size, sample_aibnr_param *aibnr_param)
{
    td_s32 ret = ss_mpi_aibnr_init();
    ot_aibnr_cfg cfg;
    td_s32 i;

    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_aibnr_init error(%#x)\n", ret);
        return ret;
    }

    ret = sample_aibnr_load(in_size, model_info, model_id, aibnr_param);
    if (ret != TD_SUCCESS) {
        sample_print("sample_aibnr_load error(%#x)\n", ret);
        goto deinit;
    }

    cfg.ref_mode = aibnr_param->ref_mode; /* ref mode NORM: need reference frame; NONE: no reference frame */
    ret = ss_mpi_aibnr_set_cfg(vi_pipe, &cfg);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_aibnr_set_alg_cfg error(%#x)\n", ret);
        goto unload_model;
    }

    sample_get_char("enable");

    ret = ss_mpi_aibnr_enable(vi_pipe);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_aibnr_enable error(%#x)\n", ret);
        goto unload_model;
    }

    ret = sample_aibnr_set_attr(vi_pipe, aibnr_param);
    if (ret != TD_SUCCESS) {
        sample_print("sample_aibnr_set_attr error(%#x)\n", ret);
        goto aibnr_disable;
    }

    return ret;

aibnr_disable:
    ss_mpi_aibnr_disable(vi_pipe);
unload_model:
    for (i = 0;i < model_size; i++) {
        if (model_id[i] != -1) {
            sample_aibnr_unload_model(&model_info[i], model_id[i]);
        }
    }

deinit:
    ss_mpi_aibnr_exit();

    return ret;
}

static td_s32 sample_aibnr_unload(ot_vi_pipe vi_pipe, ot_aibnr_model *model, td_s32 *model_id, td_s32 model_size)
{
    td_s32 return_value = TD_SUCCESS;
    td_s32 ret;
    td_s32 i;
    for (i = 0; i < model_size; i++) {
        if (model_id[i] != -1) {
            ret = sample_aibnr_unload_model(&model[i], model_id[i]);
            if (ret != TD_SUCCESS) {
                return_value = ret;
            }
        }
    }

    return return_value;
}

static td_s32 sample_aibnr_stop(ot_vi_pipe vi_pipe, ot_aibnr_model *model, td_s32 *model_id, td_s32 model_size)
{
    td_s32 ret1;
    td_s32 ret2;

    ret1 = ss_mpi_aibnr_disable(vi_pipe);
    if (ret1 != TD_SUCCESS) {
        sample_print("ss_mpi_aibnr_disable false error(%#x)\n", ret1);
    }

    ret2 = sample_aibnr_unload(vi_pipe, model, model_id, model_size);
    if (ret2 != TD_SUCCESS) {
        sample_print("sample_aibnr_unload_exit false error(%#x)\n", ret2);
    }

    ss_mpi_aibnr_exit();

    if ((ret1 != TD_SUCCESS) || (ret2 != TD_SUCCESS)) {
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

static td_void sample_aibnr_deinit_aiisp_pool(ot_vi_pipe vi_pipe, const sample_vi_cfg *vi_cfg)
{
    if (vi_cfg->pipe_info[vi_pipe].attach_pool != OT_VB_INVALID_POOL_ID) {
        ss_mpi_vb_destroy_pool(vi_cfg->pipe_info[vi_pipe].attach_pool);
    }
}

static td_void sample_aibnr_stop_vi(ot_vi_pipe vi_pipe, const sample_vi_cfg *vi_cfg)
{
    sample_comm_vi_stop_vi(vi_cfg);
    sample_aibnr_deinit_aiisp_pool(vi_pipe, vi_cfg);
    sample_comm_sys_exit();
}

static td_u32 sample_aibnr_init_aiisp_pool(ot_size *in_size, td_u32 vb_cnt)
{
    td_s32 blk_size;
    ot_vb_pool vb_pool;
    ot_vb_pool_cfg vb_pool_cfg = {0};

    blk_size = (td_s32)ot_aibnr_get_pic_buf_size(in_size->width, in_size->height);
    vb_pool_cfg.blk_size = (td_u64)blk_size;
    vb_pool_cfg.blk_cnt = vb_cnt;
    vb_pool_cfg.remap_mode = OT_VB_REMAP_MODE_NONE;
    vb_pool = ss_mpi_vb_create_pool(&vb_pool_cfg);
    if (vb_pool == OT_VB_INVALID_POOL_ID) {
        sample_print("aibnr create user pool failed!\n");
        return OT_VB_INVALID_POOL_ID;
    }

#ifdef SAMPLE_MEM_SHARE_ENABLE
    ss_mpi_vb_pool_share_all(vb_pool);
#endif

    return vb_pool;
}

static td_void sample_aibnr_set_vpss_crop(ot_vpss_grp grp, ot_size *in_size)
{
    td_s32 ret;
    ot_vpss_crop_info crop_info = {0};
    td_u32 crop_pixel = 8;

    crop_info.enable = TD_TRUE;
    crop_info.crop_mode = OT_COORD_ABS;
    crop_info.crop_rect.x = (td_s32)crop_pixel;
    crop_info.crop_rect.y = (td_s32)crop_pixel;
    crop_info.crop_rect.width = in_size->width - crop_pixel;
    crop_info.crop_rect.height = in_size->height - crop_pixel;
    ret = ss_mpi_vpss_set_grp_crop(grp, &crop_info);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_vpss_set_grp_crop failed!\n");
    }
}

static td_s32 sample_aibnr_start_vi(ot_vi_pipe vi_pipe, sample_vi_cfg *vi_cfg, ot_size *in_size,
    td_u32 vb_cnt)
{
    td_s32 ret;
    sample_vi_pipe_info *pipe_info = &vi_cfg->pipe_info[vi_pipe];

    if (sample_aiisp_sys_init(in_size, VB_AIBNR_COMMON_VB_CNT) != TD_SUCCESS) {
        sample_print("sample_aiisp_sys_init failed.\n");
        return TD_FAILURE;
    }

    /* aiisp route use attach pool */
    pipe_info->attach_pool = sample_aibnr_init_aiisp_pool(&pipe_info->pipe_attr.size, vb_cnt);
    if (pipe_info->attach_pool == OT_VB_INVALID_POOL_ID) {
        goto sys_exit;
    }

    ret = sample_comm_vi_start_vi(vi_cfg);
    if (ret != TD_SUCCESS) {
        sample_print("start vi failed.\n");
        goto denit_vb_pool;
    }

    return TD_SUCCESS;

denit_vb_pool:
    ss_mpi_vb_destroy_pool(pipe_info->attach_pool);
sys_exit:
    sample_comm_sys_exit();

    return TD_FAILURE;
}

static td_s32 sample_aibnr_set_blc(ot_vi_pipe vi_pipe, sample_sns_type sns_type)
{
    td_s32 i, j, ret;
    ot_isp_black_level_attr black_level_attr;

    ret = ss_mpi_isp_get_black_level_attr(vi_pipe, &black_level_attr);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_isp_get_black_level_attr failed.\n");
        return TD_FAILURE;
    }

    black_level_attr.user_black_level_en = TD_TRUE;

    for (i = 0; i < OT_ISP_WDR_MAX_FRAME_NUM; i++) {
        for (j = 0; j < OT_ISP_BAYER_CHN_NUM; j++) {
            black_level_attr.user_black_level[i][j] = 1200; /* user_black_level of aibnr default as 1200 */
        }
    }

    ret = ss_mpi_isp_set_black_level_attr(vi_pipe, &black_level_attr);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_isp_set_black_level_attr failed.\n");
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

static td_void sample_aibnr_update_cfg(ot_vi_pipe vi_pipe, sample_vi_cfg *vi_cfg, td_bool is_pro)
{
    td_u32 dst_fps = (is_pro == TD_TRUE) ? 5 : 12;  /* 5\12fps */

    vi_cfg->pipe_info[vi_pipe].set_early_end_mode = TD_TRUE;
    vi_cfg->pipe_info[vi_pipe].pipe_attr.compress_mode = OT_COMPRESS_MODE_NONE;
    vi_cfg->pipe_info[vi_pipe].pipe_attr.pixel_format = OT_PIXEL_FORMAT_RGB_BAYER_12BPP; /* 12fps */

    vi_cfg->pipe_info[vi_pipe].isp_info.isp_pub_attr.frame_rate = dst_fps;
}

static td_s32 sample_aibnr_check_support(ot_vi_pipe vi_pipe, sample_aibnr_param *aibnr_param)
{
    if (aibnr_param->is_wdr_mode == TD_TRUE) {
        if (aibnr_param->is_blend != TD_FALSE) {
            sample_print("normal_blend must be false in wdr mode\n");
            return TD_FAILURE;
        }

        if (vi_pipe >= OT_VI_MAX_PHYS_PIPE_NUM) {
            sample_print("vi_pipe must be phy pipe in wdr mode\n");
            return TD_FAILURE;
        }
    }

    /* bnr_bypass must be false when attr->enable is false */
    /* blend must be false when bnr_bypass is true */
    /* vi must be offline */

    return TD_SUCCESS;
}

td_s32 sample_aibnr(sample_aibnr_param *aibnr_param)
{
    td_s32 ret = TD_FAILURE;
    sample_vi_cfg vi_cfg = {0};
    sample_sns_type sns_type = sample_get_sns_type(0);
    const td_u32 vb_cnt = (aibnr_param->is_pro == TD_TRUE) ? VB_AIBNR_TNR_CNT : VB_AIBNR_LINE_CNT;
    const ot_vi_pipe vi_pipe = aibnr_param->is_wdr_mode ? 1 : 0;
    ot_vi_pipe master_pipe = 0;
    const ot_vi_chn vi_chn = 0;
    ot_vpss_grp vpss_grp[1] = {0};
    ot_size in_size = {0};
    ot_aibnr_model model_info[2] = {0};
    td_s32 model_id[2] = {-1, -1};
    td_s32 model_size = (td_s32)(sizeof(model_id) / sizeof(td_s32));

    if (sample_aibnr_check_support(vi_pipe, aibnr_param) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_aiisp_get_default_cfg(sns_type, vi_pipe, &in_size, &vi_cfg);
    sample_aibnr_update_cfg(vi_pipe, &vi_cfg, aibnr_param->is_pro);

    if (sample_aibnr_start_vi(vi_pipe, &vi_cfg, &in_size, vb_cnt) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    sample_comm_vi_bind_vpss(master_pipe, vi_chn, vpss_grp[0], 0);
    if (sample_aiisp_start_vpss(vpss_grp[0], &in_size) != TD_SUCCESS) {
        goto stop_vi;
    }

    sample_aibnr_set_vpss_crop(vpss_grp[0], &in_size);

    if (sample_aiisp_start_venc_bind(vpss_grp, sizeof(vpss_grp) / sizeof(vpss_grp[0]), &in_size) != TD_SUCCESS) {
        goto stop_vpss;
    }

    if (sample_aibnr_set_blc(master_pipe, sns_type) != TD_SUCCESS) {
        goto stop_venc_and_vo;
    }

    if (sample_aibnr_start(vi_pipe, in_size, model_info, model_id, model_size, aibnr_param) != TD_SUCCESS) {
        goto stop_venc_and_vo;
    }

    if (sample_aiisp_set_long_frame_mode(master_pipe, aibnr_param->is_wdr_mode) != TD_SUCCESS) {
        goto stop_aibnr;
    }

    sample_get_char("disable");

    ret = TD_SUCCESS;

stop_aibnr:
    sample_aibnr_stop(vi_pipe, model_info, model_id, model_size);
    sample_get_char("exit");
stop_venc_and_vo:
    sample_aiisp_stop_venc_and_unbind(vpss_grp, sizeof(vpss_grp) / sizeof(vpss_grp[0]));
stop_vpss:
    sample_aiisp_stop_vpss(vpss_grp[0]);
stop_vi:
    sample_comm_vi_un_bind_vpss(master_pipe, vi_chn, vpss_grp[0], 0);
    sample_aibnr_stop_vi(vi_pipe, &vi_cfg);
    return ret;
}
