/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
 
#include "av_list.h"

#include "securec.h"

#include "avplay_common.h"
#include "sample_comm.h"

typedef struct {
    struct list_head list_ptr;
    td_void *priv;
} av_list_node;

static td_void list_lock(pthread_mutex_t mutex)
{
    (td_void)pthread_mutex_lock(&(mutex));
}

static td_void list_unlock(pthread_mutex_t mutex)
{
    (td_void)pthread_mutex_unlock(&(mutex));
}

static td_void list_log_and_return_if_null(td_void *condition)
{
    if ((condition) == NULL) {
        AV_PRINTF("NULL error %s %d", __FUNCTION__, __LINE__);
    }
}

td_s32 av_list_create(av_list_instance **list_hdr, td_u32 max_list_node)
{
    list_log_and_return_if_null(list_hdr);
    if (max_list_node == 0) {
        sample_print("av_list_create max_list_node 0 error\n");
        return TD_FAILURE;
    }
    av_list_instance *list_instance = malloc(sizeof(av_list_instance));
    if (list_instance == NULL) {
        sample_print("malloc  av_list_instance failed\n");
        return TD_FAILURE;
    }
    if (memset_s(list_instance, sizeof(av_list_instance), 0x00, sizeof(av_list_instance)) != EOK) {
        sample_print("set  list_instance failed\n");
        free(list_instance);
        return TD_FAILURE;
    }
    INIT_LIST_HEAD(&(list_instance->header));
    (td_void)pthread_mutex_init(&list_instance->list_lock, NULL);
    list_instance->max_node_num = max_list_node;
    *list_hdr = list_instance;
    return TD_SUCCESS;
}

static td_void av_list_del_all_node(av_list_instance *list_instance)
{
    struct list_head *pos = NULL;
    struct list_head *n = NULL;
    av_list_node *list_node = NULL;

    list_lock(list_instance->list_lock);

    list_for_each_safe(pos, n, &list_instance->header)
    {
        list_node = list_entry(pos, av_list_node, list_ptr);
        list_del(&list_node->list_ptr);
        if (memset_s(list_node, sizeof(av_list_node), 0x00, sizeof(av_list_node)) != EOK) {
            sample_print("set list_node failed\n");
        }
        free(list_node);
        list_node = NULL;
    }
    list_unlock(list_instance->list_lock);
}

td_s32 av_list_destroy(av_list_instance *list_instance)
{
    list_log_and_return_if_null(list_instance);

    av_list_del_all_node(list_instance);
    pthread_mutex_destroy(&list_instance->list_lock);

    free(list_instance);

    return TD_SUCCESS;
}

td_s32 av_list_count(av_list_instance *list_instance, td_u32 *count)
{
    list_log_and_return_if_null(list_instance);
    list_log_and_return_if_null(count);
    struct list_head *pos = NULL;
    td_u32 num = 0;

    list_lock(list_instance->list_lock);

    list_for_each(pos, &list_instance->header)
    {
        num++;
    }

    list_unlock(list_instance->list_lock);
    *count = num;
    return TD_SUCCESS;
}

td_s32 av_list_add_node(av_list_instance *list_instance, const td_void *priv)
{
    list_log_and_return_if_null(list_instance);

    td_u32 node_cnt = 0;
    if (av_list_count(list_instance, &node_cnt) != TD_SUCCESS) {
        return TD_FAILURE;
    }

    if (node_cnt >= list_instance->max_node_num) {
        sample_print("list node num reach max: %d failed\n", list_instance->max_node_num);
        return TD_FAILURE;
    }

    av_list_node *node = malloc(sizeof(av_list_node));
    if (node == TD_NULL) {
        sample_print("malloc av_list_node failed\n");
        return TD_FAILURE;
    }
    if (memset_s(node, sizeof(av_list_node), 0x00, sizeof(av_list_node)) != EOK) {
        sample_print("set av_list_node failed\n");
        free(node);
        return TD_FAILURE;
    }
    node->priv = (td_void *)priv;

    list_lock(list_instance->list_lock);
    list_add_tail(&(node->list_ptr), &(list_instance->header));
    list_unlock(list_instance->list_lock);
    return TD_SUCCESS;
}

td_s32 av_list_del_node(av_list_instance *list_instance, const td_void *priv)
{
    list_log_and_return_if_null(list_instance);
    struct list_head *pos = NULL;
    av_list_node *list_node = NULL;
    td_bool is_found = TD_FALSE;

    list_lock(list_instance->list_lock);

    list_for_each(pos, &list_instance->header)
    {
        list_node = list_entry(pos, av_list_node, list_ptr);
        if (list_node->priv == priv) {
            list_del(&list_node->list_ptr);
            free(list_node);
            is_found = TD_TRUE;
            break;
        }
    }

    if (is_found == TD_FALSE) {
        sample_print("input list node :%p not exist\n", priv);
        list_unlock(list_instance->list_lock);
        return TD_FAILURE;
    }

    list_unlock(list_instance->list_lock);
    return TD_SUCCESS;
}

td_bool av_list_check_exist(av_list_instance *list_instance, const td_void *priv)
{
    list_log_and_return_if_null(list_instance);
    struct list_head *pos = NULL;
    av_list_node *list_node = NULL;

    list_lock(list_instance->list_lock);

    list_for_each(pos, &list_instance->header)
    {
        list_node = list_entry(pos, av_list_node, list_ptr);
        if (list_node->priv == priv) {
            list_unlock(list_instance->list_lock);
            return TD_TRUE;
        }
    }
    list_unlock(list_instance->list_lock);
    return TD_FALSE;
}

td_void *av_list_find_node_with_compare(av_list_instance *list_instance,
    av_list_compare_callback compare_cbk, const td_void *condition)
{
    struct list_head *pos = NULL;
    av_list_node *list_node = NULL;

    if (list_instance == TD_NULL) {
        sample_print("input list handle NULL error\n");
        return NULL;
    }
    if (compare_cbk == TD_NULL) {
        sample_print("input compare_cbk NULL error\n");
        return NULL;
    }

    list_lock(list_instance->list_lock);

    list_for_each(pos, &list_instance->header)
    {
        list_node = list_entry(pos, av_list_node, list_ptr);
        if (compare_cbk(list_node->priv, condition) == AV_COMPARE_EQUAL) {
            list_unlock(list_instance->list_lock);
            return list_node->priv;
        }
    }

    list_unlock(list_instance->list_lock);
    return NULL;
}

td_void *av_list_get_next(av_list_instance *list_instance, const td_void *priv)
{
    struct list_head *pos = NULL;
    av_list_node *list_node = NULL;
    td_bool is_found = TD_FALSE;

    if (list_instance == NULL) {
        sample_print("input list handle NULL error\n");
        return NULL;
    }

    list_lock(list_instance->list_lock);

    list_for_each(pos, &list_instance->header)
    {
        list_node = list_entry(pos, av_list_node, list_ptr);
        if (priv == NULL) {
            list_unlock(list_instance->list_lock);
            return list_node->priv;
        }

        if (is_found == TD_TRUE) {
            // return priv after one loop of found
            list_unlock(list_instance->list_lock);
            return list_node->priv;
        }

        if (list_node->priv == priv) {
            is_found = TD_TRUE;
        }
    }

    list_unlock(list_instance->list_lock);
    return TD_NULL;
}
