/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef OT_VDEC_FRAME_BUFFER_H
#define OT_VDEC_FRAME_BUFFER_H

#include "ot_type.h"

typedef struct {
    td_void *vir_addr;
    td_u64 phy_addr;
    td_u32 blk_hdl;
    td_u32 pool_id;
} vb_vdec_buffer;

td_s32 ot_vdec_frame_buffer_alloc(td_u32 size, vb_vdec_buffer *buffer);

td_void ot_vdec_frame_buffer_free(const vb_vdec_buffer *buffer);

#endif /* OT_VDEC_FRAME_BUFFER_H */

