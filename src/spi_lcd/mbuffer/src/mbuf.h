/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef OT_MBUF_H
#define OT_MBUF_H

#include "mw_type.h"
#include "list.h"
#include "mbuf_define_ext.h"

/* \addtogroup     COMMON */
/* @{ */ /** <!-- [COMMON] */

#define ALIGNTYPE 4

/* mbuffer slice node info struct */
typedef struct {
    td_u32 u32next_pos;               /* next pos */
    ot_mbuf_sliceinfo slice_info; /* mbuffer slice info */
} ot_mbuf_slicenode;

/* payload info */
typedef struct {
    td_bool in_use;       /* payload is in use or not */
    td_bool read_enable;  /* read enable or not */
    td_bool write_enable; /* write enable or not */
    td_u32 read_pos;    /* read pos offset value */
    td_u32 write_pos;   /* write pos offset value */
    td_u8 data_type;     /* data type */
    td_u8 u8flag;         /* flag for write data 0-lose and wait for data ,1 can write */
} ot_mbuf_payload;

/* MBUF handle */
typedef struct {
    td_u32 buf_size;    /* size of mbuffer */
    td_s32 max_payload; /* max payload type num */
    td_u32 read_pos;    /* global read position */
    td_u32 write_pos;   /* global write position */
    td_u32 loop_pos;    /* loop position */
    td_u32 pre_alloc;   /* pre allocated memory space */
} ot_mbuf_head;

/* mbuffer handle list */
typedef struct {
    ot_mbuf_head *head;           /* mbuffer list head */
    td_void *argv;                     /* passing param */
    ot_mbuf_abstractfunc m_buf_func; /* register mbuf func structs */
    td_void *zero_addr;                /* starting address of buffer data except head and payload info */
    struct list_head list_node;         /* head for the list */
} mbufhandle_list_node;

/** @} */ /** <!-- ==== COMMON end ==== */

#endif /* __OT_MBUF_H__ */
