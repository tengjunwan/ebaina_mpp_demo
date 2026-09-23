/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef AV_LIST_H
#define AV_LIST_H

#include <pthread.h>
#include "mw_type.h"
#include "list.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#define AV_COMPARE_EQUAL     0
#define AV_COMPARE_NO_EQUAL  1

typedef struct {
    struct list_head header;
    pthread_mutex_t list_lock;
    td_u32 max_node_num;
} av_list_instance;

typedef td_s32 (*av_list_compare_callback)(const td_void *priv, const td_void *condition);

td_s32 av_list_create(av_list_instance **list_hdr, td_u32 max_list_node);

td_s32 av_list_destroy(av_list_instance *list_instance);

td_s32 av_list_add_node(av_list_instance *list_instance, const td_void *priv);

td_s32 av_list_del_node(av_list_instance *list_instance, const td_void *priv);

td_bool av_list_check_exist(av_list_instance *list_instance, const td_void *priv);

td_void *av_list_find_node_with_compare(av_list_instance *list_instance,
    av_list_compare_callback compare_cbk, const td_void *condition);

td_void *av_list_get_next(av_list_instance *list_instance, const td_void *priv);

td_s32 av_list_count(av_list_instance *list_instance, td_u32 *count);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* AV_LIST_H */
