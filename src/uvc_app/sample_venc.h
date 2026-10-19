/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __SAMPLE_VENC_H__
#define __SAMPLE_VENC_H__

#include "ot_type.h"
#include "sample_comm.h"

td_void set_user_config_format(td_u32 dev_no, ot_payload_type *format, ot_pic_size *wh, td_s32 *c);
td_void sample_venc_config(td_void);

#endif /* __SAMPLE_VENC_H__ */
