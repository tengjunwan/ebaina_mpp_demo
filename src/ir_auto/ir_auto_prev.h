/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef _OT_IR_AUTO_PREV_H_
#define _OT_IR_AUTO_PREV_H_

#include "ot_common.h"
#include "sample_comm.h"

typedef struct {
    sample_vi_cfg vi_config;
    ot_vi_pipe vi_pipe;
    ot_vi_chn vi_chn;
    ot_vpss_grp vpss_grp;
    ot_venc_chn venc_chn;
    sample_comm_venc_chn_param venc_param;
} ir_auto_prev;

#endif
