/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "ff_vsrc.h"

#include "mbuf_ext.h"
#include "ss_mpi_sys_mem.h"
#include "avplay_common.h"
#include "sample_comm.h"

#define ONE_STEP 1
#define FF_SRC_MBUF_MMZ_NAME "anonymous"

static td_s32 ff_src_mbuf_alloc(const td_void *argv, td_u32 buf_size, td_void **vm_addr)
{
    AVPLAY_RETURN_VAL_IF_NULL(argv);
    AVPLAY_RETURN_VAL_IF_NULL(vm_addr);
    ff_vdec_source *vdec_src = (ff_vdec_source *)argv;
    AVPLAY_RETURN_VAL_IF_NULL(vdec_src->stream_buf.vir_addr);

    if (memset_s(vdec_src->stream_buf.vir_addr, buf_size, 0x00, buf_size) != EOK) {
        sample_print(" set virtual addr fail\n");
        return TD_FAILURE;
    }

    *vm_addr = vdec_src->stream_buf.vir_addr;
    return TD_SUCCESS;
}

static td_s32 ff_src_mbuf_free(const td_void *argv, td_void *vm_addr)
{
    AVPLAY_RETURN_VAL_IF_NULL(argv);
    AVPLAY_RETURN_VAL_IF_NULL(vm_addr);
    ff_vdec_source *vdec_src = (ff_vdec_source *)argv;

    if (vm_addr != vdec_src->stream_buf.vir_addr) {
        sample_print(" free virtual addr not same as allocated\n");
        return TD_FAILURE;
    }
    return TD_SUCCESS;
}

static td_s32 ff_src_mbuf_create_lock(const td_void *argv)
{
    AVPLAY_RETURN_VAL_IF_NULL(argv);
    ff_vdec_source *vdec_src = (ff_vdec_source *)argv;
    return pthread_mutex_init(&(vdec_src->mbuf_lock), NULL);
}

static td_s32 ff_src_mbuf_destroy_lock(const td_void *argv)
{
    AVPLAY_RETURN_VAL_IF_NULL(argv);
    ff_vdec_source *vdec_src = (ff_vdec_source *)argv;
    return pthread_mutex_destroy(&(vdec_src->mbuf_lock));
}

static td_s32 ff_src_mbuf_lock(const td_void *argv)
{
    AVPLAY_RETURN_VAL_IF_NULL(argv);
    ff_vdec_source *vdec_src = (ff_vdec_source *)argv;
    return pthread_mutex_lock(&(vdec_src->mbuf_lock));
}

static td_s32 ff_src_mbuf_unlock(const td_void *argv)
{
    AVPLAY_RETURN_VAL_IF_NULL(argv);
    ff_vdec_source *vdec_src = (ff_vdec_source *)argv;
    return pthread_mutex_unlock(&(vdec_src->mbuf_lock));
}

td_s32 ff_vdec_source_create(ff_vdec_source *source, const td_char srcname[], td_u32 buf_size, td_u8 payload)
{
    AVPLAY_RETURN_VAL_IF_NULL(source);
    // alloc video  stream buffer mmz
    td_s32 ret = ss_mpi_sys_mmz_alloc_cached((td_phys_addr_t *) &source->stream_buf.phy_addr,
        &source->stream_buf.vir_addr, srcname, FF_SRC_MBUF_MMZ_NAME, buf_size);
    if (ret != TD_SUCCESS) {
        sample_print("ss_mpi_sys_mmz_alloc_cached failed,ret=0x%x\n", ret);
        return TD_FAILURE;
    }

    source->bufsize = buf_size;

    ot_mbuf_cfg mbuf_cfg = {0};

    mbuf_cfg.max_payload = 1;
    mbuf_cfg.buf_size = buf_size;
    mbuf_cfg.m_buf_func.pfn_alloc = ff_src_mbuf_alloc;
    mbuf_cfg.m_buf_func.pfn_free = ff_src_mbuf_free;
    mbuf_cfg.m_buf_func.pfn_memcpy = TD_NULL;
    mbuf_cfg.m_buf_func.pfn_create_lock = ff_src_mbuf_create_lock;
    mbuf_cfg.m_buf_func.pfn_destroy_lock = ff_src_mbuf_destroy_lock;
    mbuf_cfg.m_buf_func.pfn_lock = ff_src_mbuf_lock;
    mbuf_cfg.m_buf_func.pfn_un_lock = ff_src_mbuf_unlock;
    ret = ot_mbuf_get_buffer((td_void *)source, &mbuf_cfg, &source->buf_handle);
    if (ret != TD_SUCCESS) {
        sample_print("ot_mbuf_get_buffer fail:%d\n", ret);
        (td_void)ss_mpi_sys_mmz_free(source->stream_buf.phy_addr, source->stream_buf.vir_addr);
        return TD_FAILURE;
    }

    if (ot_mbuf_register_payload(source->buf_handle, payload) != TD_SUCCESS) {
        sample_print("ot_mbuf_register_payload fail\n");
        (td_void)ot_mbuf_release_buffer((td_void *)source, source->buf_handle);
        (td_void)ss_mpi_sys_mmz_free(source->stream_buf.phy_addr, source->stream_buf.vir_addr);
        return TD_FAILURE;
    }

    if (ot_mbuf_set_rw_enable(source->buf_handle, payload, TD_TRUE, TD_TRUE) != TD_SUCCESS) {
        sample_print("ot_mbuf_set_rw_enable fail\n");
        (td_void)ot_mbuf_release_buffer((td_void *)source, source->buf_handle);
        (td_void)ss_mpi_sys_mmz_free(source->stream_buf.phy_addr, source->stream_buf.vir_addr);
        return TD_FAILURE;
    }

    (td_void)pthread_mutex_init(&(source->read_write_lock), NULL);

    return TD_SUCCESS;
}

td_s32 ff_vdec_source_destory(ff_vdec_source *source)
{
    AVPLAY_RETURN_VAL_IF_NULL(source);
    td_s32 ret = TD_SUCCESS;

    if (source->buf_handle) {
        ret = ot_mbuf_release_buffer((td_void *)source, source->buf_handle);
        if (ret != TD_SUCCESS) {
            sample_print("ot_mbuf_release_buffer fail\n");
        }
        source->buf_handle = NULL;
    }

    if (source->stream_buf.vir_addr) {
        ret = ss_mpi_sys_mmz_free(source->stream_buf.phy_addr, source->stream_buf.vir_addr);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_sys_mmz_free fail\n");
        }
    }

    (td_void)pthread_mutex_destroy(&(source->read_write_lock));

    if (memset_s(source, sizeof(ff_vdec_source), 0x00, sizeof(ff_vdec_source)) != EOK) {
        sample_print("set  source fail\n");
        return TD_FAILURE;
    }
    return ret;
}

td_s32 ff_vdec_source_acquire_data(ff_vdec_source *source, ot_mbuf_sliceinfo *packet)
{
    AVPLAY_RETURN_VAL_IF_NULL(source);
    AVPLAY_RETURN_VAL_IF_NULL(packet);
    if (!source->buf_handle) {
        sample_print("input a_vsource handle invalid\n");
        return TD_FAILURE;
    }

    AVPLAY_LOCK(source->read_write_lock);
    td_s32 ret = ot_mbuf_read_slice(source->buf_handle, ANY_PAYLOAD, packet);
    AVPLAY_UNLOCK(source->read_write_lock);

    return ret;
}

td_s32 ff_vdec_source_release_data(ff_vdec_source *source)
{
    AVPLAY_RETURN_VAL_IF_NULL(source);
    ot_mbuf_sliceinfo mbuf_slice_info;
    if (memset_s(&mbuf_slice_info, sizeof(mbuf_slice_info), 0x00, sizeof(mbuf_slice_info)) != EOK) {
        sample_print("set  mbuf_slice_info fail\n");
        return TD_FAILURE;
    }

    if (!source->buf_handle) {
        sample_print("input a_vsource handle invalid\n");
        return TD_FAILURE;
    }

    AVPLAY_LOCK(source->read_write_lock);

    td_s32 ret = ot_mbuf_forward(source->buf_handle, ANY_PAYLOAD, ONE_STEP);
    if (ret != TD_SUCCESS) {
        sample_print("ot_mbuf_forward  failed, ret:%d \n", ret);
    }
    AVPLAY_UNLOCK(source->read_write_lock);

    return ret;
}

td_s32 ff_vdec_source_write_data(ff_vdec_source *source, ot_mbuf_sliceinfo *packet)
{
    AVPLAY_RETURN_VAL_IF_NULL(source);
    AVPLAY_RETURN_VAL_IF_NULL(packet);
    AVPLAY_RETURN_VAL_IF_NULL(source->buf_handle);
    if (packet->frame_len >= source->bufsize) {
        sample_print("input vid framelen %d big than buffer len: %d err\n", packet->frame_len, source->bufsize);
        return TD_FAILURE;
    }

    AVPLAY_LOCK(source->read_write_lock);

    td_s32 ret = ot_mbuf_write_slice(source->buf_handle, packet);
    if (ret != TD_SUCCESS) {
        sample_print("ot_mbuf_write_slice exec failed, ret:%d \n", ret);
    }

    AVPLAY_UNLOCK(source->read_write_lock);

    return ret;
}

td_s32 ff_vdec_source_reset(ff_vdec_source *source)
{
    AVPLAY_RETURN_VAL_IF_NULL(source);
    AVPLAY_RETURN_VAL_IF_NULL(source->buf_handle);
    AVPLAY_LOCK(source->read_write_lock);

    // clear all cached data
    while (ot_mbuf_forward(source->buf_handle, ANY_PAYLOAD, ONE_STEP) == TD_SUCCESS) { };

    AVPLAY_UNLOCK(source->read_write_lock);
    return TD_SUCCESS;
}
