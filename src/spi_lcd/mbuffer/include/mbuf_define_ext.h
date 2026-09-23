/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __OT_MBUF_DEFINE_EXT_H__
#define __OT_MBUF_DEFINE_EXT_H__

#define OT_MBUF_FRAME_MAX_BLOCK (8)

#define OT_MBUF_MAX_PAYLOAD_TYPE_CNT (8)

#include "ot_type.h"
#include "mw_type.h"
/* \addtogroup     COMMON */
/* @{ */ /** <!-- [COMMON] */

/* error number */
#define ERR_MBUF_BASE         TD_FAILURE
#define ERR_MBUF_ILLEGALPARAM (ERR_MBUF_BASE - 1)  /* illegal params */
#define ERR_MBUF_REINTIALIZE  (ERR_MBUF_BASE - 2)  /* re init of mbuffer */
#define ERR_MBUF_UNINTIALIZE  (ERR_MBUF_BASE - 3)  /* used with out init */
#define ERR_MBUF_FULL         (ERR_MBUF_BASE - 4)  /* mbuffer is full */
#define ERR_MBUF_NODATA       (ERR_MBUF_BASE - 5)  /*  cannot find the dada of the payload type */
#define ERR_MBUF_REACHLIMIT   (ERR_MBUF_BASE - 6)  /*  payload_type reach the max num definition */
#define ERR_MBUF_DISABLED     (ERR_MBUF_BASE - 7)  /* read or write is not able to access */
#define ERR_MBUF_DISLOCATED   (ERR_MBUF_BASE - 8)  /*  err with the pos ,only can read in slice */
#define ERR_MBUF_UNCOMPLETED  (ERR_MBUF_BASE - 9)  /* do not write the frame compelety */
/* frame which is first or whose prev frame is not key framef */
#define ERR_MBUF_NOTKEYFRAME  (ERR_MBUF_BASE - 10)

/* call back funs of mbuffer */
typedef td_s32 (*mbuf_alloc_func)(const td_void *argv, td_u32 buf_size, td_void **pp_vm_addr);

typedef td_s32 (*mbuf_free_func)(const td_void *argv, td_void *vm_addr);

typedef td_s32 (*mbuf_mcopy_func)(const td_void *argv, td_void *dest, const td_void *src, td_u32 size);

typedef td_s32 (*mbuf_create_lock_func)(const td_void *argv);

typedef td_s32 (*mbuf_destroy_lock_func)(const td_void *argv);

typedef td_s32 (*mbuf_lock_func)(const td_void *argv);

typedef td_s32 (*mbuf_un_lock_func)(const td_void *argv);

/* funcs of mbuffer abstract */
typedef struct {
    mbuf_alloc_func pfn_alloc;             /* get the memory addr of the mbuffer */
    mbuf_free_func pfn_free;               /* release memory space of the mbuffer func */
    mbuf_mcopy_func pfn_memcpy;            /* mbuffer memory copy func */
    mbuf_create_lock_func pfn_create_lock;   /* mbuffer func of create lock */
    mbuf_destroy_lock_func pfn_destroy_lock; /* mbuffer func of destroy lock */
    mbuf_lock_func pfn_lock;               /* lock of mbuffer */
    mbuf_un_lock_func pfn_un_lock;           /* unlock of mbuffer */
} ot_mbuf_abstractfunc;

/* mbuffer config */
typedef struct {
    td_u32 buf_size;                 /* memory size of mbuffer */
    td_s32 max_payload;              /* max patload type num in mbuffer */
    ot_mbuf_abstractfunc m_buf_func; /* register mbuf func structs */
} ot_mbuf_cfg;

/* mbuffer slice info */
typedef struct {
    td_u64 pts;                                /* timestamp if the frame */
    td_u32 seq;                                /* sequence num of the frame */
    td_u32 frame_len;                           /* length of the frame data */
    td_u32 slice_len;                           /* length of the slice data */
    td_u32 head_len;                            /* length of slice data head space  */
    td_u32 tail_len;                            /* length of slilce data tail space */
    td_u32 slice_num;                           /* slice sequence number */
    td_u32 slice_cnt;                           /* slice count of the frame data */
    td_bool is_key_frame;                          /* whether keyframe or not */
    td_u8 payload_type;                          /* mbuffer data payload type 0~254 */
    td_u8 slice_type;                            /* slice type 0x01-start 0x02-middle 0x04-end 0x05-single frame */
    td_u8 *addr[OT_MBUF_FRAME_MAX_BLOCK];        /* start address of the data block */
    td_u32 au32_block_len[OT_MBUF_FRAME_MAX_BLOCK]; /* len of the block data agree with adder */
    td_u32 block_count;
} ot_mbuf_sliceinfo;

/* mbuffer frame data info */
typedef struct {
    td_u32 slice_cnt;                /* mbuffer slice data count */
    ot_mbuf_sliceinfo *slice_info; /* mbuffer slice data  info */
} ot_mbuf_frameinfo;

/** @} */ /** <!-- ==== COMMON end ==== */

#endif /* __OT_MBUF_DEFINE_EXT_H__ */
