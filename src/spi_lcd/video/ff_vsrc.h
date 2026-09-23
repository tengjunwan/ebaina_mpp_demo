/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef FF_VDEC_SOURCE_H
#define FF_VDEC_SOURCE_H

#include <pthread.h>
#include "ot_type.h"
#include "mbuf_define_ext.h"

#define SINGLE_FRAME_SLICE 0x5

#define ONE_SLICE 1

#define ANY_PAYLOAD 255

typedef struct {
    td_u64 phy_addr;
    td_void *vir_addr;
}ff_vdec_strem_buf;

typedef struct {
    ff_vdec_strem_buf stream_buf;
    pthread_mutex_t mbuf_lock;
    td_u32 bufsize;
    OT_MW_PTR buf_handle;
    pthread_mutex_t read_write_lock;
}ff_vdec_source;

td_s32 ff_vdec_source_create(ff_vdec_source *source, const td_char srcname[], td_u32 buf_size, td_u8 payload);

td_s32 ff_vdec_source_destory(ff_vdec_source *source);

td_s32 ff_vdec_source_reset(ff_vdec_source *source);

td_s32 ff_vdec_source_acquire_data(ff_vdec_source *source, ot_mbuf_sliceinfo *packet);

td_s32 ff_vdec_source_release_data(ff_vdec_source *source);

td_s32 ff_vdec_source_write_data(ff_vdec_source *source, ot_mbuf_sliceinfo *packet);

#endif /* __OT_NW_AVSOURCE_H__ */
