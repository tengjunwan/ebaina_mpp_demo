/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "mbuf.h"
#include "mbuf_define_ext.h"
#include "securec.h"

static pthread_mutex_t g_list_lock = PTHREAD_MUTEX_INITIALIZER;
static struct list_head g_mbuf_handle_list = LIST_HEAD_INIT(g_mbuf_handle_list);

td_s32 mbuf_check_invalid_handle(OT_MW_PTR handle)
{
    if (TD_NULL == handle) {
        return ERR_MBUF_ILLEGALPARAM;
    }
    return TD_SUCCESS;
}

td_bool check_start_slice(td_u8 type)
{
    return (type & 0x01);
}

td_bool check_end_slice(td_u8 type)
{
    return (type & 0x04);
}

td_u32 align_length(td_u32 size, td_u32 align_type)
{
    return ((size + (align_type - 1)) & (~(align_type - 1)));
}


/******************************MBUF inner interface**********************************/

/* pos reprents the read/write ptr location,len is the slice data length
next_point(p, len) points to next byte data at the tail of the frame */
static td_u32 next_point(td_u32 pos, td_u32 head, td_u32 len, td_u32 tail)
{
    return pos + align_length(sizeof(ot_mbuf_slicenode), ALIGNTYPE) + align_length(head, ALIGNTYPE) +
        align_length(len, ALIGNTYPE) + align_length(tail, ALIGNTYPE);
}

/* pos represent the slice data ptr location,payload_addr return the starting address of data */
static td_u32 payload_addr(td_u32 pos, td_u32 head)
{
    return pos + align_length(sizeof(ot_mbuf_slicenode), ALIGNTYPE) + align_length(head, ALIGNTYPE);
}

static td_u32 get_available_space(const mbufhandle_list_node *handle)
{
    td_u32 free_space;
    td_u32 read_pos = handle->head->read_pos;

    if (handle->head->write_pos < read_pos) {
        /* 0******w***************r********/
        free_space = read_pos - handle->head->write_pos;
    } else if (handle->head->write_pos == read_pos) {
        free_space = handle->head->buf_size - ((td_u8 *)(handle->zero_addr) - (td_u8 *)(handle->head));
    } else {
        /* 0*******r***************w********/
        td_u32 payload_buf_size = handle->head->buf_size -
                                  ((td_u8 *)(handle->zero_addr) - (td_u8 *)(handle->head));
        if (payload_buf_size <= handle->head->write_pos) {
            free_space = 0;
        } else {
            free_space = payload_buf_size - (handle->head->write_pos) - 1;
        }
        free_space = (free_space > (read_pos)) ? free_space : (read_pos);
    }

    if (free_space < handle->head->pre_alloc) {
        printf("error free_space %u small than prealloc size: %u\n",
               free_space, handle->head->pre_alloc);
        return 0;
    }
    return (free_space - handle->head->pre_alloc);
}

static td_bool need_loop(const mbufhandle_list_node *handle, const ot_mbuf_sliceinfo *slice_info)
{
    td_u32 next_write = next_point(handle->head->write_pos, slice_info->head_len,
        slice_info->slice_len, slice_info->tail_len);
    if (next_write > (handle->head->buf_size - ((td_u8 *)(handle->zero_addr) - (td_u8 *)(handle->head)))) {
        return TD_TRUE;
    }

    return TD_FALSE;
}

static td_s32 start_write(const mbufhandle_list_node *handle, const ot_mbuf_payload *payload_info,
    const ot_mbuf_sliceinfo *slice_info)
{
    ot_mbuf_slicenode *slice = NULL;
    td_u32 i;
    td_u8 *tmp_addr = NULL;
    td_u32 tmp_len = 0;
    slice = (ot_mbuf_slicenode *)((td_u8 *)(handle->zero_addr) + handle->head->write_pos);
    slice->u32next_pos = 1;
    if (memcpy_s(&(slice->slice_info), sizeof(slice->slice_info), slice_info,
        sizeof(ot_mbuf_sliceinfo)) != EOK) {
        return ERR_MBUF_ILLEGALPARAM;
    }
    slice->slice_info.block_count = 1;
    slice->slice_info.au32_block_len[0] = slice_info->slice_len;
    slice->slice_info.addr[0] = (td_u8 *)(handle->zero_addr) + payload_addr(handle->head->write_pos,
        slice_info->head_len);
    for (i = 1; i < slice_info->block_count; i++) {
        slice->slice_info.au32_block_len[i] = 0;
        slice->slice_info.addr[i] = NULL;
    }
    tmp_addr = slice->slice_info.addr[0];

    for (i = 0; i < slice_info->block_count; i++) {
        if (memcpy_s(tmp_addr, slice_info->slice_len - tmp_len, slice_info->addr[i],
            slice_info->au32_block_len[i]) != EOK) {
            return ERR_MBUF_ILLEGALPARAM;
        }
        tmp_addr += slice_info->au32_block_len[i];
        tmp_len += slice_info->au32_block_len[i];
    }

    /* clear input  slice tail data to zero */
    if (slice_info->tail_len > 0) {
        td_u8 *tail_ptr = slice->slice_info.addr[0] + slice_info->slice_len;
        if (memset_s(tail_ptr, slice->slice_info.au32_block_len[0] - slice_info->slice_len, 0x00,
            slice_info->tail_len) != EOK) {
            return ERR_MBUF_ILLEGALPARAM;
        }
    }

    if (payload_info->write_pos != 2) { /* 2: write pos offset value */
        ot_mbuf_slicenode *last_slice = (ot_mbuf_slicenode *)((td_u8 *)(handle->zero_addr) +
            payload_info->write_pos);
        last_slice->u32next_pos = handle->head->write_pos;
    }
    return TD_SUCCESS;
}

static td_s32 write_data(mbufhandle_list_node *handle, const ot_mbuf_payload *payload_info,
    const ot_mbuf_sliceinfo *slice_info)
{
    td_u32 total;
    if ((sizeof(ot_mbuf_slicenode) + slice_info->head_len + slice_info->tail_len) >
        UINT_MAX / (slice_info->slice_cnt + 1)) {
        return ERR_MBUF_ILLEGALPARAM;
    }
    if (check_start_slice(slice_info->slice_type)) {
        /* first slice of a frame,calculate the space need */
        total = slice_info->frame_len + (sizeof(ot_mbuf_slicenode) + slice_info->head_len +
            slice_info->tail_len) * (slice_info->slice_cnt + 1);
        if (total > get_available_space(handle)) {
            return ERR_MBUF_FULL;
        }
        if (!check_end_slice(slice_info->slice_type)) {
            handle->head->pre_alloc += total;
        }
    } else if (check_end_slice(slice_info->slice_type)) {
        if (!check_start_slice(slice_info->slice_type)) {
            total = slice_info->frame_len + (sizeof(ot_mbuf_slicenode) + slice_info->head_len +
                slice_info->tail_len) * (slice_info->slice_cnt + 1);
            handle->head->pre_alloc -= total;
        }
    }

    if (need_loop(handle, slice_info)) {
        /* update the looppos and writepos */
        handle->head->loop_pos = handle->head->write_pos;
        handle->head->write_pos = 0;
        /* if read get to  write when loop,loop readpos too */
        if (handle->head->read_pos ==
            handle->head->loop_pos) {
            handle->head->read_pos = 0;
        }
    }

    return start_write(handle, payload_info, slice_info);
}

static ot_mbuf_payload *get_payload(const mbufhandle_list_node *handle, td_u8 payload_type)
{
    td_u32 i;
    ot_mbuf_payload *payload_info = NULL;
    td_u8 *head_addr = (td_u8 *)handle->head;

    for (i = 0; i < (td_u32)(handle->head->max_payload); i++) {
        payload_info = (ot_mbuf_payload *)(head_addr + align_length(sizeof(ot_mbuf_head) +
            i * sizeof(ot_mbuf_payload), ALIGNTYPE));
        if (payload_info->data_type == payload_type && payload_info->in_use == TD_TRUE) {
            return payload_info;
        }
    }

    return NULL;
}

static ot_mbuf_payload *get_idle_payload(const mbufhandle_list_node *handle)
{
    td_u32 i;
    ot_mbuf_payload *payload_info = NULL;
    td_u8 *head_addr = (td_u8 *)handle->head;

    for (i = 0; i < (td_u32)(handle->head->max_payload); i++) {
        payload_info = (ot_mbuf_payload *)(head_addr + align_length(sizeof(ot_mbuf_head) +
            i * sizeof(ot_mbuf_payload), ALIGNTYPE));
        if (payload_info->in_use == TD_FALSE) {
            return payload_info;
        }
    }

    return NULL;
}

static td_u32 get_slowest_readpos(const mbufhandle_list_node *handle)
{
    td_u32 i;
    td_u32 j = 0;
    td_u32 offset;
    if ((handle == NULL) || (handle->head == NULL) ||
        (handle->head->max_payload > (INT_MAX / (int32_t)sizeof(ot_mbuf_payload)))) {
        return ERR_MBUF_ILLEGALPARAM;
    }

    td_u8 *head_addr = (td_u8 *)handle->head;
    td_u32 last_offset = (handle->head->buf_size - align_length((td_u32)(sizeof(ot_mbuf_head) +
        handle->head->max_payload * sizeof(ot_mbuf_payload)), ALIGNTYPE));
    ot_mbuf_payload *payload_info = NULL;

    for (i = 0; i < (td_u32)(handle->head->max_payload); i++) {
        payload_info = (ot_mbuf_payload *)(head_addr + align_length(sizeof(ot_mbuf_head) +
            i * sizeof(ot_mbuf_payload), ALIGNTYPE));
        if (payload_info->in_use != TD_TRUE) {
            continue;
        }
        if (payload_info->read_pos < handle->head->read_pos) {
            offset = payload_info->read_pos - handle->head->read_pos +
                (handle->head->buf_size - align_length((sizeof(ot_mbuf_head) +
                (td_u32)(handle->head->max_payload) * sizeof(ot_mbuf_payload)), ALIGNTYPE));
        } else {
            offset = payload_info->read_pos - handle->head->read_pos;
        }

        if (offset < last_offset) {
            last_offset = offset;
            j = i;
        }
    }

    payload_info = (ot_mbuf_payload *)(head_addr + align_length(sizeof(ot_mbuf_head) +
        j * sizeof(ot_mbuf_payload), ALIGNTYPE));
    if (payload_info->read_pos == 1) {
        return handle->head->read_pos;
    }

    return payload_info->read_pos;
}

static mbufhandle_list_node *get_expect_list_n_node(const OT_MW_PTR handle)
{
    struct list_head *pos = NULL;
    mbufhandle_list_node *node = NULL;
    (void)pthread_mutex_lock(&g_list_lock);
    list_for_each(pos, &g_mbuf_handle_list)
    {
        node = list_entry(pos, mbufhandle_list_node, list_node);
        if (node == handle) {
            (void)pthread_mutex_unlock(&g_list_lock);
            return node;
        }
    }

    (void)pthread_mutex_unlock(&g_list_lock);
    return NULL;
}

static td_void set_abstract_func(ot_mbuf_abstractfunc *mbuf_func, const ot_mbuf_abstractfunc *mbuf_func_in)
{
    mbuf_func->pfn_alloc = mbuf_func_in->pfn_alloc;
    mbuf_func->pfn_create_lock = mbuf_func_in->pfn_create_lock;
    mbuf_func->pfn_destroy_lock = mbuf_func_in->pfn_destroy_lock;
    mbuf_func->pfn_free = mbuf_func_in->pfn_free;
    mbuf_func->pfn_lock = mbuf_func_in->pfn_lock;
    mbuf_func->pfn_memcpy = mbuf_func_in->pfn_memcpy;
    mbuf_func->pfn_un_lock = mbuf_func_in->pfn_un_lock;
    return;
}

static td_s32 forward_mbuf(const mbufhandle_list_node *node, td_u8 payload_type)
{
    if (node == NULL || node->head == NULL) {
        return ERR_MBUF_ILLEGALPARAM;
    }
    ot_mbuf_payload *payload_info = get_payload(node, payload_type);
    ot_mbuf_slicenode *slice = NULL;
    if (payload_info == NULL) {
        return ERR_MBUF_ILLEGALPARAM;
    }
    if (payload_info->read_pos != 1) {
        if (payload_info->read_pos == payload_info->write_pos) {
            return ERR_MBUF_NODATA;
        }
        slice = (ot_mbuf_slicenode *)((td_u8 *)(node->zero_addr) + payload_info->read_pos);
        if (slice->u32next_pos != 1) {
            payload_info->read_pos = slice->u32next_pos;
            return TD_SUCCESS;
        } else {
            payload_info->read_pos = payload_info->write_pos;
            return ERR_MBUF_NODATA;
        }
    }
    td_u32 read_pos = node->head->read_pos;
    while (read_pos != node->head->write_pos) {
        slice = (ot_mbuf_slicenode *)((td_u8 *)(node->zero_addr) + read_pos);
        if (slice->slice_info.payload_type == payload_type) {
            payload_info->read_pos = read_pos;
            return TD_SUCCESS;
        }
        read_pos = next_point(read_pos, slice->slice_info.head_len,
            slice->slice_info.slice_len, slice->slice_info.tail_len);
        if ((read_pos >= node->head->loop_pos) &&
            (node->head->write_pos < read_pos)) {
            read_pos = 0;
        }
    }
    return ERR_MBUF_NODATA;
}

static td_s32 read_slice(const mbufhandle_list_node *node, td_u8 payload_type, ot_mbuf_sliceinfo *slice_info)
{
    if (node == NULL || slice_info == NULL || node->head == NULL) {
        return (ERR_MBUF_NODATA);
    }
    ot_mbuf_payload *payload_info = NULL;
    ot_mbuf_slicenode *slice = NULL;
    payload_info = get_payload(node, payload_type);
    if (payload_info == NULL) {
        return (ERR_MBUF_NODATA);
    }
    if (payload_info->read_enable == TD_FALSE) {
        return ERR_MBUF_DISABLED;
    }

    if (payload_info->read_pos != 1) {
        if (payload_info->read_pos == payload_info->write_pos) {
            return (ERR_MBUF_NODATA);
        }
        slice = (ot_mbuf_slicenode *)((td_u8 *)(node->zero_addr) + payload_info->read_pos);
        if (memcpy_s(slice_info, sizeof(ot_mbuf_sliceinfo), &(slice->slice_info),
            sizeof(slice->slice_info)) != EOK) {
            return ERR_MBUF_ILLEGALPARAM;
        }
        /* if used over process,need to calculate the data virtual address again */
        slice_info->addr[0] = (td_u8 *)(node->zero_addr) + payload_addr(payload_info->read_pos,
        slice_info->head_len);
        node->head->read_pos = get_slowest_readpos(node);
        return TD_SUCCESS;
    }
    td_u32 read_pos_temp = node->head->read_pos;

    while (read_pos_temp != node->head->write_pos) {
        slice = (ot_mbuf_slicenode *)((td_u8 *)(node->zero_addr) + read_pos_temp);
        if (slice->slice_info.payload_type == payload_type) {
            if (memcpy_s(slice_info, sizeof(ot_mbuf_sliceinfo), &(slice->slice_info),
                sizeof(slice->slice_info)) != EOK) {
                return ERR_MBUF_ILLEGALPARAM;
            }
            /* if used over process,need to calculate the data virtual address again */
            slice_info->addr[0] = (td_u8 *)(node->zero_addr) + payload_addr(read_pos_temp, slice_info->head_len);
            payload_info->read_pos = read_pos_temp;
            node->head->read_pos = get_slowest_readpos(node);
            return TD_SUCCESS;
        }
        read_pos_temp = next_point(read_pos_temp, slice->slice_info.head_len,
            slice->slice_info.slice_len, slice->slice_info.tail_len);
        if ((read_pos_temp >= node->head->loop_pos) && (node->head->write_pos < read_pos_temp)) {
            read_pos_temp = 0;
        }
    }
    return ERR_MBUF_NODATA;
}

static td_s32 write_slice(const OT_MW_PTR h_handle, const ot_mbuf_sliceinfo *slice_info)
{
    struct list_head *pos = NULL;
    mbufhandle_list_node *node = NULL;
    ot_mbuf_payload *payload_info = NULL;
    (void)pthread_mutex_lock(&g_list_lock);
    list_for_each(pos, &g_mbuf_handle_list)
    {
        node = list_entry(pos, mbufhandle_list_node, list_node);
        if (node != h_handle) {
            continue;
        }
        node->m_buf_func.pfn_lock(node->argv);
        payload_info = get_payload(node, slice_info->payload_type);
        if (payload_info == NULL) {
            (void)pthread_mutex_unlock(&g_list_lock);
            node->m_buf_func.pfn_un_lock(node->argv);
            return (ERR_MBUF_ILLEGALPARAM);
        }
        if (payload_info->write_enable == TD_FALSE) {
            (void)pthread_mutex_unlock(&g_list_lock);
            node->m_buf_func.pfn_un_lock(node->argv);
            return (ERR_MBUF_DISABLED);
        }

        if (((payload_info->u8flag == 0) && (slice_info->is_key_frame == TD_TRUE) &&
            (check_start_slice(slice_info->slice_type))) || (payload_info->u8flag == 1)) {
            td_s32 ret = write_data(node, payload_info, slice_info);
            if (ret == ERR_MBUF_FULL) {
                payload_info->u8flag = 0;
                (void)pthread_mutex_unlock(&g_list_lock);
                node->m_buf_func.pfn_un_lock(node->argv);
                return (ERR_MBUF_FULL);
            } else {
                /* write success and update the writepos */
                payload_info->write_pos = node->head->write_pos;
                node->head->write_pos = next_point(node->head->write_pos,
                    slice_info->head_len, slice_info->slice_len, slice_info->tail_len);
                payload_info->u8flag = 1;
                (void)pthread_mutex_unlock(&g_list_lock);
                node->m_buf_func.pfn_un_lock(node->argv);
                return (TD_SUCCESS);
            }
        } else {
            (void)pthread_mutex_unlock(&g_list_lock);
            node->m_buf_func.pfn_un_lock(node->argv);
            return (ERR_MBUF_NOTKEYFRAME);
        }
    }
    (void)pthread_mutex_unlock(&g_list_lock);
    return (ERR_MBUF_ILLEGALPARAM);
}

static td_s32 get_buffer(const td_void *argv, const ot_mbuf_cfg *mbuf_cfg,
    OT_MW_PTR *handle, td_void *vm_addr, ot_mbuf_head *head)
{
    td_u8 *zero_addr = NULL;
    td_u8 *head_addr = NULL;
    mbufhandle_list_node *node = NULL;
    struct list_head *pos = NULL;
    (void)pthread_mutex_lock(&g_list_lock);
    list_for_each(pos, &g_mbuf_handle_list)
    {
        node = list_entry(pos, mbufhandle_list_node, list_node);
        if (node->head == head) {
            *handle = node;
            (void)pthread_mutex_unlock(&g_list_lock);
            mbuf_cfg->m_buf_func.pfn_un_lock(argv);
            return (TD_SUCCESS);
        }
    }
    mbufhandle_list_node *new_dode = NULL;
    new_dode = (mbufhandle_list_node *)malloc(sizeof(mbufhandle_list_node));
    if (new_dode == NULL) {
        printf("malloc failed [func:%s]line:%d\n", __FUNCTION__, __LINE__);
        (void)pthread_mutex_unlock(&g_list_lock);
        mbuf_cfg->m_buf_func.pfn_un_lock(argv);
        mbuf_cfg->m_buf_func.pfn_destroy_lock(argv);
        mbuf_cfg->m_buf_func.pfn_free(argv, vm_addr);
        return (TD_FAILURE);
    }

    new_dode->head = head;
    new_dode->argv = (td_void *)(argv);
    head_addr = (td_u8 *)head;
    zero_addr = head_addr + align_length((sizeof(ot_mbuf_head) + ((td_u32)(head->max_payload)) *
        sizeof(ot_mbuf_payload)), ALIGNTYPE);
    if (zero_addr > head_addr + mbuf_cfg->buf_size) {
        printf("bufsize is too small for bufhead\n");
        (void)pthread_mutex_unlock(&g_list_lock);
        mbuf_cfg->m_buf_func.pfn_un_lock(argv);
        mbuf_cfg->m_buf_func.pfn_destroy_lock(argv);
        mbuf_cfg->m_buf_func.pfn_free(argv, vm_addr);
        free(new_dode);
        new_dode = NULL;
        return ERR_MBUF_FULL;
    }
    new_dode->zero_addr = (td_void *)zero_addr;
    set_abstract_func(&new_dode->m_buf_func, &mbuf_cfg->m_buf_func);
    list_add(&new_dode->list_node, &g_mbuf_handle_list);
    *handle = new_dode;
    (void)pthread_mutex_unlock(&g_list_lock);
    mbuf_cfg->m_buf_func.pfn_un_lock(argv);
    return (TD_SUCCESS);
}

/**
 * @brief init of MBUF.
 * @return  TD_SUCCESS success
 * @return ERR_MBUF_ILLEGALPARAM  illegal params
 * @return ERR_MBUF_REINTIALIZE  reinit of MBUF
 */
static td_bool check_handle_exist(const OT_MW_PTR h_handle)
{
    struct list_head *pos = NULL;
    mbufhandle_list_node *node = NULL;

    (void)pthread_mutex_lock(&g_list_lock);
    list_for_each(pos, &g_mbuf_handle_list)
    {
        node = list_entry(pos, mbufhandle_list_node, list_node);
        if ((OT_MW_PTR)node == h_handle) {
            (void)pthread_mutex_unlock(&g_list_lock);
            return TD_TRUE;
        }
    }
    (void)pthread_mutex_unlock(&g_list_lock);
    return TD_FALSE;
}

static td_s32 check_buffer_handle(const ot_mbuf_cfg *mbuf_cfg)
{
    /* allocate buffer_handle */
    if (mbuf_cfg == NULL || mbuf_cfg->buf_size <= sizeof(ot_mbuf_head) ||
        mbuf_cfg->m_buf_func.pfn_lock == NULL || mbuf_cfg->m_buf_func.pfn_un_lock == NULL ||
        mbuf_cfg->max_payload > OT_MBUF_MAX_PAYLOAD_TYPE_CNT || mbuf_cfg->max_payload <= 0 ||
        mbuf_cfg->m_buf_func.pfn_destroy_lock == NULL || mbuf_cfg->m_buf_func.pfn_create_lock == NULL ||
        mbuf_cfg->m_buf_func.pfn_free == NULL || mbuf_cfg->m_buf_func.pfn_alloc == NULL) {
        return (TD_FAILURE);
    }
    return (TD_SUCCESS);
}

/**
 * @brief allocate of MBUF.
 * @param[in] td_void   *argv   :passing param
 * @param[in,out] ot_mbuf_cfg   *mbuf_cfg : property of buffer
 * @param[out,in]  td_void         **pp_handle:      mbuf_handle
 * @return  TD_SUCCESS success
 * @return ERR_MBUF_ILLEGALPARAM  illegal params
 * @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_get_buffer(const td_void *argv, ot_mbuf_cfg *mbuf_cfg, OT_MW_PTR *handle)
{
    td_void *vm_addr = NULL;
    struct list_head *pos = NULL;
    mbufhandle_list_node *node = NULL;

    if (mbuf_cfg == NULL || handle == NULL) {
        return (ERR_MBUF_ILLEGALPARAM);
    }
    /* get the mbuffer settings */
    if (*handle != TD_NULL) {
        (void)pthread_mutex_lock(&g_list_lock);
        list_for_each(pos, &g_mbuf_handle_list)
        {
            node = list_entry(pos, mbufhandle_list_node, list_node);
            if (node == *handle) {
                mbuf_cfg->buf_size = node->head->buf_size;
                mbuf_cfg->max_payload = node->head->max_payload;
                set_abstract_func(&mbuf_cfg->m_buf_func, &node->m_buf_func);
                (void)pthread_mutex_unlock(&g_list_lock);
                return (TD_SUCCESS);
            }
        }
        (void)pthread_mutex_unlock(&g_list_lock);
        return (ERR_MBUF_ILLEGALPARAM);
    }
    /* allocate buffer_handle */
    if (check_buffer_handle(mbuf_cfg) != TD_SUCCESS) {
        return (ERR_MBUF_ILLEGALPARAM);
    }

    td_u32 buffer_size = align_length(mbuf_cfg->buf_size, ALIGNTYPE);
    td_s32 ret = mbuf_cfg->m_buf_func.pfn_alloc(argv, buffer_size, &vm_addr);
    if (ret != TD_SUCCESS || vm_addr == NULL) {
        return (ret);
    }

    mbuf_cfg->m_buf_func.pfn_create_lock(argv);
    mbuf_cfg->m_buf_func.pfn_lock(argv);
    ot_mbuf_head *head = (ot_mbuf_head *)vm_addr;
    if (head->buf_size == 0 && head->max_payload == 0 &&
        head->read_pos == 0 && head->write_pos == 0) {
        head->buf_size = buffer_size;
        head->max_payload = mbuf_cfg->max_payload;
        head->loop_pos = (head->buf_size - align_length((sizeof(ot_mbuf_head) +
            (td_u32)head->max_payload * sizeof(ot_mbuf_payload)), ALIGNTYPE));
    } else {
        mbuf_cfg->buf_size = head->buf_size;
        mbuf_cfg->max_payload = head->max_payload;
    }
    return get_buffer(argv, mbuf_cfg, handle, vm_addr, head);
}

/**
 * @brief realse MBUF.
 * @param[in] td_void   *argv   :passing param
 * @param[out,in]  td_void         **pp_handle:      mbuf_handle
 * @return  TD_SUCCESS success
 * @return ERR_MBUF_ILLEGALPARAM  illegal params
 * @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_release_buffer(const td_void *argv, OT_MW_PTR h_handle)
{
    struct list_head *pos = NULL;
    mbufhandle_list_node *node = NULL;
    mbuf_check_invalid_handle(h_handle);

    (void)pthread_mutex_lock(&g_list_lock);
    list_for_each(pos, &g_mbuf_handle_list)
    {
        node = list_entry(pos, mbufhandle_list_node, list_node);
        if (node == h_handle) {
            list_del(pos);
            (void)pthread_mutex_unlock(&g_list_lock);
            node->m_buf_func.pfn_lock(argv);
            node->m_buf_func.pfn_free(argv, node->head);
            node->m_buf_func.pfn_un_lock(argv);
            node->m_buf_func.pfn_destroy_lock(argv);
            free(node);
            return (TD_SUCCESS);
        }
    }
    (void)pthread_mutex_unlock(&g_list_lock);
    return (ERR_MBUF_ILLEGALPARAM);
}

/**
 * @brief register MBUF payload type, for example 0 for video 1 for audio.
 * @param[in] td_void *handle  : mbuf_handle
 * @param[in]  td_u8   payload_type:      payload type 0~254
 * @return  TD_SUCCESS success
 * @return ERR_MBUF_ILLEGALPARAM  illegal params
 * @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_register_payload(OT_MW_PTR h_handle, td_u8 payload_type)
{
    struct list_head *node_pos = NULL;
    mbufhandle_list_node *buf_node = NULL;
    ot_mbuf_payload *payload = NULL;
    mbuf_check_invalid_handle(h_handle);

    if (payload_type > 254) { /* 254: payload type 0~254 */
        return (ERR_MBUF_ILLEGALPARAM);
    }

    (void)pthread_mutex_lock(&g_list_lock);
    list_for_each(node_pos, &g_mbuf_handle_list)
    {
        buf_node = list_entry(node_pos, mbufhandle_list_node, list_node);
        if (buf_node == h_handle) {
            buf_node->m_buf_func.pfn_lock(buf_node->argv);
            if (NULL != get_payload(buf_node, payload_type)) {
                /* already registered */
                (void)pthread_mutex_unlock(&g_list_lock);
                buf_node->m_buf_func.pfn_un_lock(buf_node->argv);
                return (TD_SUCCESS);
            }

            payload = get_idle_payload(buf_node);
            if (payload != NULL) {
                payload->in_use = TD_TRUE;
                payload->read_enable = TD_FALSE;
                payload->write_enable = TD_FALSE;
                payload->read_pos = 1;
                payload->write_pos = 2; /* 2: write pos offset value */
                payload->data_type = payload_type;
                payload->u8flag = 0;
                (void)pthread_mutex_unlock(&g_list_lock);
                buf_node->m_buf_func.pfn_un_lock(buf_node->argv);
                return (TD_SUCCESS);
            } else {
                (void)pthread_mutex_unlock(&g_list_lock);
                buf_node->m_buf_func.pfn_un_lock(buf_node->argv);
                return (ERR_MBUF_REACHLIMIT);
            }
        }
    }
    (void)pthread_mutex_unlock(&g_list_lock);
    return (ERR_MBUF_ILLEGALPARAM);
}

/**
 * @brief unregister MBUF payload type.
 * @param[in] td_void *handle  : mbuf_handle
 * @param[in]  td_u8   payload_type:      payload type 0~254
 * @return  TD_SUCCESS success
 * @return ERR_MBUF_ILLEGALPARAM  illegal params
 * @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_unregister_payload(OT_MW_PTR h_handle, td_u8 payload_type)
{
    struct list_head *pos = NULL;
    mbufhandle_list_node *node = NULL;
    ot_mbuf_payload *payload_info = NULL;
    mbuf_check_invalid_handle(h_handle);

    if (payload_type > 254) { /* 254: payload type 0~254 */
        return (ERR_MBUF_ILLEGALPARAM);
    }

    (void)pthread_mutex_lock(&g_list_lock);
    list_for_each(pos, &g_mbuf_handle_list)
    {
        node = list_entry(pos, mbufhandle_list_node, list_node);
        if (node == h_handle) {
            node->m_buf_func.pfn_lock(node->argv);
            payload_info = get_payload(node, payload_type);
            if (payload_info != NULL) {
                payload_info->in_use = TD_FALSE;
                (void)pthread_mutex_unlock(&g_list_lock);
                node->m_buf_func.pfn_un_lock(node->argv);
                return (TD_SUCCESS);
            } else {
                (void)pthread_mutex_unlock(&g_list_lock);
                node->m_buf_func.pfn_un_lock(node->argv);
                return (ERR_MBUF_ILLEGALPARAM);
            }
        }
    }
    (void)pthread_mutex_unlock(&g_list_lock);
    return (ERR_MBUF_ILLEGALPARAM);
}

/**
* @brief enable read/write ptr for MBUF according to the payload type.
* @param[in] td_void *handle  : mbuf_handle
* @param[in]  td_u8   payload_type:      payload type 0~254
* @param[in]  td_bool enable_read :      enable read ptr or not
* @param[in]  td_bool enable_write:      enable write ptr or not
* @return  TD_SUCCESS success
* @return ERR_MBUF_ILLEGALPARAM  illegal params
* @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_set_rw_enable(OT_MW_PTR h_handle, td_u8 payload_type, td_bool enable_read, td_bool enable_write)
{
    struct list_head *node_pos = NULL;
    mbufhandle_list_node *buf_node = NULL;
    ot_mbuf_payload *payload = NULL;
    mbuf_check_invalid_handle(h_handle);

    (void)pthread_mutex_lock(&g_list_lock);
    list_for_each(node_pos, &g_mbuf_handle_list)
    {
        buf_node = list_entry(node_pos, mbufhandle_list_node, list_node);
        if (buf_node == h_handle) {
            buf_node->m_buf_func.pfn_lock(buf_node->argv);
            payload = get_payload(buf_node, payload_type);
            if (payload != NULL) {
                payload->read_enable = enable_read;
                payload->write_enable = enable_write;
                (void)pthread_mutex_unlock(&g_list_lock);
                buf_node->m_buf_func.pfn_un_lock(buf_node->argv);
                return (TD_SUCCESS);
            }
            (void)pthread_mutex_unlock(&g_list_lock);
            buf_node->m_buf_func.pfn_un_lock(buf_node->argv);
            return (ERR_MBUF_ILLEGALPARAM);
        }
    }
    (void)pthread_mutex_unlock(&g_list_lock);
    return (ERR_MBUF_ILLEGALPARAM);
}

/**
* @brief get the  read/write enable prt for MBUF according to the payload type.
* @param[in] td_void *handle  : mbuf_handle
* @param[in]  td_u8   payload_type:      payload type 0~254
* @param[in]  td_bool *enable_read :      enable read ptr or not
* @param[in]  td_bool *enable_write:      enable write ptr or not
* @return  TD_SUCCESS success
* @return ERR_MBUF_ILLEGALPARAM  illegal params
* @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_get_rw_enable(OT_MW_PTR h_handle, td_u8 payload_type, td_bool *enable_read, td_bool *enable_write)
{
    struct list_head *pos = NULL;
    mbufhandle_list_node *node = NULL;
    ot_mbuf_payload *payload_info = NULL;
    mbuf_check_invalid_handle(h_handle);

    (void)pthread_mutex_lock(&g_list_lock);
    list_for_each(pos, &g_mbuf_handle_list)
    {
        node = list_entry(pos, mbufhandle_list_node, list_node);
        if (node != h_handle) {
            continue;
        }
        node->m_buf_func.pfn_lock(node->argv);
        payload_info = get_payload(node, payload_type);
        if (payload_info != NULL) {
            if (enable_read != NULL) {
                *enable_read = payload_info->read_enable;
            }

            if (enable_write != NULL) {
                *enable_write = payload_info->write_enable;
            }

            (void)pthread_mutex_unlock(&g_list_lock);
            node->m_buf_func.pfn_un_lock(node->argv);
            return (TD_SUCCESS);
        }
        (void)pthread_mutex_unlock(&g_list_lock);
        node->m_buf_func.pfn_un_lock(node->argv);
        return (ERR_MBUF_ILLEGALPARAM);
    }
    (void)pthread_mutex_unlock(&g_list_lock);
    return (ERR_MBUF_ILLEGALPARAM);
}

/**
* @brief  get slice data offset
* @param[in] td_void *handle  : mbuf_handle
* @param[in]  ot_mbuf_sliceinfo* slice_info:   mbuffer slice info
* @param[out] td_u32* offset: slice info offset
* @return  TD_SUCCESS success
* @return ERR_MBUF_ILLEGALPARAM  illegal params
* @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
* @return ERR_MBUF_FULL   MBUF is full
 */
td_s32 ot_mbuf_get_slice_data_offset(OT_MW_PTR h_handle, const ot_mbuf_sliceinfo *slice_info, td_u32 *offset)
{
    mbufhandle_list_node *node = NULL;
    ot_mbuf_payload *payload_info = NULL;
    mbuf_check_invalid_handle(h_handle);

    if ((slice_info == NULL) || (slice_info->slice_cnt <= 0) || (offset == NULL)) {
        return (ERR_MBUF_ILLEGALPARAM);
    }

    node = get_expect_list_n_node(h_handle);
    if (node == NULL || node->head == NULL) {
        return (ERR_MBUF_ILLEGALPARAM);
    }

    (void)pthread_mutex_lock(&g_list_lock);
    payload_info = get_payload(node, slice_info->payload_type);
    if (payload_info == NULL) {
        (void)pthread_mutex_unlock(&g_list_lock);
        return (ERR_MBUF_ILLEGALPARAM);
    }

    td_u32 total;
    if ((sizeof(ot_mbuf_slicenode) + slice_info->head_len + slice_info->tail_len) >
        UINT_MAX / (slice_info->slice_cnt + 1)) {
        (void)pthread_mutex_unlock(&g_list_lock);
        return (ERR_MBUF_ILLEGALPARAM);
    }
    if (check_start_slice(slice_info->slice_type)) {
        /* according the first slice of the frame,  calculate the frame size */
        /* one slice struct like this:  | slice_head---- slice struct---slice data---slice_tail | */
        total = slice_info->frame_len + (sizeof(ot_mbuf_slicenode) + slice_info->head_len +
            slice_info->tail_len) * (slice_info->slice_cnt + 1);

        if (total > get_available_space(node)) {
            (void)pthread_mutex_unlock(&g_list_lock);
            return (ERR_MBUF_FULL);
        }
    }

    td_u32 temp_write_pos;

    if (need_loop(node, slice_info)) {
        temp_write_pos = 0;
    } else {
        temp_write_pos = node->head->write_pos;
    }

    td_u8 *slice_data = (td_u8 *)(node->zero_addr) + payload_addr(temp_write_pos, slice_info->head_len);
    *offset = slice_data - *(td_u8 **)node;
    (void)pthread_mutex_unlock(&g_list_lock);

    return (TD_SUCCESS);
}

/**
* @brief write slice into MBUF
* @param[in] td_void *handle  : mbuf_handle
* @param[in]   ot_mbuf_sliceinfo *slice_info     slice info data
* @return  TD_SUCCESS success
* @return ERR_MBUF_ILLEGALPARAM  illegal params
* @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
* @return ERR_MBUF_FULL  full of MBUF
 */
td_s32 ot_mbuf_write_slice(OT_MW_PTR h_handle, const ot_mbuf_sliceinfo *slice_info)
{
    mbuf_check_invalid_handle(h_handle);
    td_u32 len = 0;
    td_u32 i;
    if (slice_info == NULL || slice_info->slice_cnt <= 0 || slice_info->block_count == 0 ||
        slice_info->block_count > OT_MBUF_FRAME_MAX_BLOCK) {
        return (ERR_MBUF_ILLEGALPARAM);
    }

    for (i = 0; i < slice_info->block_count; i++) {
        if (slice_info->au32_block_len[i] == 0 || slice_info->addr[i] == NULL) {
            return (ERR_MBUF_ILLEGALPARAM);
        }
        len += slice_info->au32_block_len[i];
        // add au32_block_len exceed MAX_INT
        if (len < slice_info->au32_block_len[i]) {
            return (ERR_MBUF_ILLEGALPARAM);
        }
    }

    if (slice_info->slice_len != len) {
        return (ERR_MBUF_ILLEGALPARAM);
    }
    return write_slice(h_handle, slice_info);
}

/**
* @brief read data from MBUF
* @param[in] td_void *handle  : mbuf_handle
* @param[in]  td_u8   payload_type    payload type
* @param[out] ot_mbuf_sliceinfo *slice_info  read out slice info
* @return  TD_SUCCESS success
* @return ERR_MBUF_ILLEGALPARAM  illegal params
* @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
* @return ERR_MBUF_NODATA  no such data of the giving payload type
 */
td_s32 ot_mbuf_read_slice(OT_MW_PTR h_handle, td_u8 payload_type, ot_mbuf_sliceinfo *slice_info)
{
    mbufhandle_list_node *node = NULL;
    ot_mbuf_payload *payload_info = NULL;
    ot_mbuf_slicenode *slice = NULL;
    mbuf_check_invalid_handle(h_handle);

    if (slice_info == NULL) {
        return (ERR_MBUF_ILLEGALPARAM);
    }
    if (!check_handle_exist(h_handle)) {
        return (ERR_MBUF_ILLEGALPARAM);
    }

    /* handle is effective or not is guaranteed by caller */
    node = (mbufhandle_list_node *)h_handle;
    if ((node == NULL) || (node->head == NULL)) {
        return (ERR_MBUF_ILLEGALPARAM);
    }
    if (payload_type == 255) { /* 255: payload type 0~254 */
        if ((node->head->read_pos >= node->head->loop_pos) &&
            (node->head->write_pos < node->head->read_pos)) {
            node->head->read_pos = 0;
        }

        if (node->head->read_pos != node->head->write_pos) {
            slice = (ot_mbuf_slicenode *)((td_u8 *)(node->zero_addr) + node->head->read_pos);
            if (memcpy_s(slice_info, sizeof(ot_mbuf_sliceinfo), &(slice->slice_info),
                sizeof(slice->slice_info)) != EOK) {
                return ERR_MBUF_ILLEGALPARAM;
            }
            /* if used over process,need to calculate the data virtual address again */
            slice_info->addr[0] = (td_u8 *)(node->zero_addr) + payload_addr(node->head->read_pos,
                slice_info->head_len);
            payload_info = get_payload(node, slice_info->payload_type);
            if (payload_info != NULL) {
                payload_info->read_pos = node->head->read_pos;
            }

            return (TD_SUCCESS);
        } else {
            return (ERR_MBUF_NODATA);
        }
    }
    return read_slice(node, payload_type, slice_info);
}

/**
* @brief  forward the read ptr for the payload type data
* @param[in] td_void *handle  : mbuf_handle
* @param[in]  td_u8   payload_type:    payload type
* @param[out] td_u32  step:  moving steps for the read ptr,1 for move one slice after
* @return  TD_SUCCESS success
* @return ERR_MBUF_ILLEGALPARAM  illegal params
* @return ERR_MBUF_UNINTIALIZE  uninit of MBUF
 */
td_s32 ot_mbuf_forward(OT_MW_PTR h_handle, td_u8 payload_type, td_u32 step)
{
    mbufhandle_list_node *node = NULL;
    ot_mbuf_slicenode *slice = NULL;
    mbuf_check_invalid_handle(h_handle);

    if (!check_handle_exist(h_handle)) {
        return (ERR_MBUF_ILLEGALPARAM);
    }

    /* handle is effective or not is guaranteed by caller */
    node = (mbufhandle_list_node *)h_handle;
    if ((node == NULL) || (node->head == NULL) || (node->argv == NULL)) {
        return ERR_MBUF_ILLEGALPARAM;
    }
    if (step == 0) {
        return TD_SUCCESS;
    } else if (step > 1) {
        return (ERR_MBUF_ILLEGALPARAM);
    }
    if (payload_type == 255) { /* 255: payload type 0~254 */
        node->m_buf_func.pfn_lock(node->argv);
        td_u32 read_pos = node->head->read_pos;

        if (read_pos == node->head->write_pos) {
            node->m_buf_func.pfn_un_lock(node->argv);
            return (ERR_MBUF_NODATA);
        }
        slice = (ot_mbuf_slicenode *)((td_u8 *)(node->zero_addr) + read_pos);
        read_pos = next_point(read_pos, slice->slice_info.head_len,
            slice->slice_info.slice_len, slice->slice_info.tail_len);
        if ((read_pos >= node->head->loop_pos) && (node->head->write_pos < read_pos)) {
            read_pos = 0;
        }
        node->head->read_pos = read_pos;
        node->m_buf_func.pfn_un_lock(node->argv);
        return (TD_SUCCESS);
    } else {
        return forward_mbuf(node, payload_type);
    }
}

/**
* @brief  print the debug info og MBUF
* @param[in] td_void *handle  : mbuf_handle
* @param[in]  td_u8   payload_type :   payload type
 */
td_void ot_mbuf_debug(OT_MW_PTR h_handle, td_u8 payload_type)
{
    mbufhandle_list_node *node = NULL;
    ot_mbuf_payload *payload_info = NULL;

    if (h_handle == TD_NULL) {
        return;
    }
    if (!check_handle_exist(h_handle)) {
        return;
    }

    /* handle is guaranteed by caller */
    node = (mbufhandle_list_node *)h_handle;
    if ((node == TD_NULL) || (node->head == TD_NULL)) {
        return;
    }
    printf("buffer_size:%u\n", node->head->buf_size);
    printf("s_readpos:%u\n", node->head->read_pos);
    printf("s_writepos:%u\n", node->head->write_pos);
    printf("looppos:%u\n", node->head->loop_pos);
    payload_info = get_payload(node, payload_type);
    if (payload_info != NULL) {
        printf("readenable:%d\n", payload_info->read_enable);
        printf("writeenable:%d\n", payload_info->write_enable);
        printf("readpos:%u\n", payload_info->read_pos);
        printf("writepos:%u\n", payload_info->write_pos);
    }
}
