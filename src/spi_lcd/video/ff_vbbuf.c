/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include "ff_vbbuf.h"
#include "ss_mpi_vb.h"
#include "ss_mpi_sys.h"
#include "ss_mpi_sys_mem.h"
#include "sample_comm.h"

td_s32 ot_vdec_frame_buffer_alloc(td_u32 size, vb_vdec_buffer *buffer)
{
    // malloc from vb
    ot_vb_blk blk_hdl = ss_mpi_vb_get_blk(OT_VB_INVALID_POOL_ID, size, NULL);
    if (blk_hdl == OT_VB_INVALID_HANDLE) {
        printf("ss_mpi_vb_get_blk failed \n");
        return TD_FAILURE;
    }

    td_u64 phy_addr = ss_mpi_vb_handle_to_phys_addr(blk_hdl);
    if (phy_addr == 0) {
        ss_mpi_vb_release_blk(blk_hdl);
        sample_print("ss_mpi_vb_handle2_phys_addr failed\n");
        return TD_FAILURE;
    }

    ot_vb_pool pool_id = ss_mpi_vb_handle_to_pool_id(blk_hdl);
    if (pool_id == OT_VB_INVALID_POOL_ID) {
        ss_mpi_vb_release_blk(blk_hdl);
        sample_print("ss_mpi_vb_handle2_pool_id failed\n");
        return TD_FAILURE;
    }

    buffer->vir_addr = ss_mpi_sys_mmap(phy_addr, size);
    if (buffer->vir_addr == 0) {
        ss_mpi_vb_release_blk(blk_hdl);
        sample_print("ss_mpi_sys_mmap_cache failed\n");
        return TD_FAILURE;
    }

    buffer->phy_addr = phy_addr;
    buffer->blk_hdl = blk_hdl;
    buffer->pool_id = pool_id;
    return TD_SUCCESS;
}

td_void ot_vdec_frame_buffer_free(const vb_vdec_buffer *buffer)
{
    if (buffer != TD_NULL) {
        td_s32 ret = ss_mpi_vb_release_blk(buffer->blk_hdl);
        if (ret != TD_SUCCESS) {
            sample_print("ss_mpi_vb_release_block failed\n");
        }
    }
}
