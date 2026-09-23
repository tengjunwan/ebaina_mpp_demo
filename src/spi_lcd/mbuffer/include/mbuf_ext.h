/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __OT_MBUF_EXT_H__
#define __OT_MBUF_EXT_H__

#include "mbuf_define_ext.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

/* \addtogroup     COMMON */
/* @{ */ /** <!-- [COMMON] */

/**
 * @brief allocate of MBUF.
 * @param[in] td_void   *argv   :passing param
 * @param[in,out] ot_mbuf_cfg   *mbuf_cfg : property of buffer
 * @param[out,in]  OT_MW_PTR*         handle:      mbuf_handle
 * @return  TD_SUCCESS success
 * @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
 * @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_get_buffer(const td_void *argv, ot_mbuf_cfg *mbuf_cfg, OT_MW_PTR *handle);

/**
 * @brief realse MBUF.
 * @param[in] td_void   *argv   :passing param
 * @param[out,in]  OT_MW_PTR         h_handle:      mbuf_handle
 * @return  TD_SUCCESS success
 * @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
 * @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_release_buffer(const td_void *argv, OT_MW_PTR h_handle);

/**
 * @brief register MBUF payload type, for example 0 for video 1 for audio.
 * @param[in] OT_MW_PTR h_handle : mbuf_handle
 * @param[in]  td_u8   payload_type:      payload type 0~254
 * @return  TD_SUCCESS success
 * @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
 * @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_register_payload(OT_MW_PTR h_handle, td_u8 payload_type);

/**
 * @brief unregister MBUF payload type.
 * @param[in] OT_MW_PTR h_handle  : mbuf_handle
 * @param[in]  td_u8   payload_type:      payload type 0~254
 * @return  TD_SUCCESS success
 * @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
 * @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_unregister_payload(OT_MW_PTR h_handle, td_u8 payload_type);

/**
* @brief enable read/write ptr for MBUF according to the payload type.
* @param[in] OT_MW_PTR h_handle  : mbuf_handle
* @param[in]  td_u8   payload_type:      payload type 0~254
* @param[in]  td_bool enable_read :      enable read ptr or not
* @param[in]  td_bool enable_write:      enable write ptr or not
* @return  TD_SUCCESS success
* @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
* @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_set_rw_enable(OT_MW_PTR h_handle, td_u8 payload_type, td_bool enable_read, td_bool enable_write);

/**
* @brief get the  read/write enable prt for MBUF according to the payload type.
* @param[in] OT_MW_PTR h_handle  : mbuf_handle
* @param[in]  td_u8   payload_type:      payload type 0~254
* @param[in]  td_bool *enable_read :      enable read ptr or not
* @param[in]  td_bool *enable_write:      enable write ptr or not
* @return  TD_SUCCESS success
* @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
* @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_get_rw_enable(OT_MW_PTR h_handle, td_u8 payload_type, td_bool *enable_read, td_bool *enable_write);

/**
* @brief write slice into MBUF
* @param[in] OT_MW_PTR h_handle : mbuf_handle
* @param[in]   ot_mbuf_sliceinfo *slice_info     slice info data
* @return  TD_SUCCESS success
* @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
* @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
* @return OT_ERR_MBUF_FULL  full of MBUF
 */
td_s32 ot_mbuf_write_slice(OT_MW_PTR h_handle, const ot_mbuf_sliceinfo *slice_info);

/**
* @brief read data from MBUF
* @param[in] OT_MW_PTR h_handle  : mbuf_handle
* @param[in]  td_u8   payload_type    payload type
* @param[out] ot_mbuf_sliceinfo *slice_info  read out slice info
* @return  TD_SUCCESS success
* @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
* @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
* @return OT_ERR_MBUF_NODATA  no such data of the giving payload type
 */
td_s32 ot_mbuf_read_slice(OT_MW_PTR h_handle, td_u8 payload_type, ot_mbuf_sliceinfo *slice_info);

/**
* @brief  forward the read ptr for the payload type data
* @param[in] OT_MW_PTR h_handle  : mbuf_handle
* @param[in]  td_u8   payload_type:    payload type
* @param[out] td_u32  step:  moving steps for the read ptr,1 for move one slice after
* @return  TD_SUCCESS success
* @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
* @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_forward(OT_MW_PTR h_handle, td_u8 payload_type, td_u32 step);

/**
* @brief  print the debug info og MBUF
* @param[in] OT_MW_PTR h_handle  : mbuf_handle
* @param[in]  td_u8   payload_type :   payload type
 */
td_void ot_mbuf_debug(OT_MW_PTR h_handle, td_u8 payload_type);

/**
* @brief  get slice data offset
* @param[in] OT_MW_PTR h_handle  : mbuf_handle
* @param[in]  ot_mbuf_sliceinfo* slice_info:   mbuffer slice info
* @param[out] td_u32* offset: slice info offset
* @return  TD_SUCCESS success
* @return OT_ERR_MBUF_ILLEGALPARAM  illegal params
* @return OT_ERR_MBUF_UNINTIALIZE  uninit of MBUF
* @return OT_ERR_MBUF_FULL   MBUF is full
 */
td_s32 ot_mbuf_get_slice_data_offset(OT_MW_PTR h_handle, const ot_mbuf_sliceinfo *slice_info, td_u32 *offset);

/** @} */ /** <!-- ==== COMMON end ==== */
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* __OT_MBUF_EXT_H__ */
