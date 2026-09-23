/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef AVPLAYER_VDEC_MNG_H
#define AVPLAYER_VDEC_MNG_H

#include "ot_type.h"
#include "ot_common.h"

td_void vdec_mng_init(td_void);

td_void vdec_mng_deinit(td_void);

td_s32 vdec_mng_get_free_handle(ot_vdec_chn *vdec_chn);

td_s32 vdec_mng_free_handle(ot_vdec_chn vdec_chn);

#endif /* AVPLAYER_VDEC_MNG_H */
