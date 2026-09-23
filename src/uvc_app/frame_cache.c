/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include "log.h"
#include "frame_cache.h"

/******************************************
 * frame_cache
 *    |--> frame_queue_free
 *          |--> node_list
 *                  |--> frame_node[0..n]
 *    |--> ok_queue
 *          |--> node_list
 *                  |--> frame_node[0..n]
 *
 *****************************************/
static int frame_node_list_push_node(frame_node_list_t *node_list, frame_node_t *node)
{
    if (node_list == NULL || node == NULL) {
        log("push node_list or node is NULL\n");
        return -1;
    }
    node->next = NULL;
    node_list->count++;

    // first time
    if (node_list && (node_list->tail == 0)) {
        node_list->head = node;
        node_list->tail = node;
    } else if (node_list->head != 0) {
        node_list->head->next = node;
        node_list->head = node;
    }

    return 0;
}

static int frame_node_list_pop_node(frame_node_list_t *node_list, frame_node_t **node)
{
    if (node_list == NULL || node == NULL) {
        log("pop node_list or node is NULL\n");
        return -1;
    }
    *node = node_list->tail;

    if ((node_list->tail != 0) && (node_list->tail->next != 0)) {
        node_list->tail = node_list->tail->next;
        node_list->count--;
    } else if (node_list->tail == node_list->head) {
        node_list->tail = NULL;
        node_list->head = NULL;
        node_list->count = 0;
    }

    return (*node != NULL) ? 0 : -1;
}

static void frame_node_list_init(frame_node_list_t *node_list)
{
    if (node_list == NULL) {
        log("init node_list NULL\n");
        return;
    }
    node_list->tail = NULL;
    node_list->head = NULL;
    node_list->count = 0;
}

static frame_node_list_t* frame_node_list_alloc(void)
{
    frame_node_list_t* node_list;

    node_list = (frame_node_list_t *)malloc(sizeof(frame_node_list_t));
    if (node_list == NULL) {
        log("malloc frame cache failure\n");
        return NULL;
    }

    frame_node_list_init(node_list);
    return node_list;
}

static void frame_node_list_free(frame_node_list_t *node_list)
{
    while (node_list->tail != NULL) {
        frame_node_t *tmp = node_list->tail;
        node_list->tail = tmp->next;
        free(tmp);
    }
}

static int frame_queue_free(frame_queue_t *queue)
{
    frame_node_list_t *node_list;

    if (queue == NULL) {
        return -1;
    }
    node_list = queue->node_list;
    if (node_list != NULL) {
        frame_node_list_free(node_list);
        free(node_list);
        queue->node_list = NULL;
    }

    pthread_mutex_destroy(&(queue->locker));
    pthread_cond_destroy(&(queue->waiter));

    free(queue);
    return 0;
}

static frame_queue_t* frame_queue_alloc(void)
{
    frame_queue_t* queue;

    queue = (frame_queue_t *)malloc(sizeof(frame_queue_t));
    if (queue == NULL) {
        log("malloc frame queue failure\n");
        return NULL;
    }

    queue->node_list = frame_node_list_alloc();
    if (queue->node_list == NULL) {
        log("malloc frame node list failure\n");
        frame_queue_free(queue);
        return NULL;
    }

    /* Notice, recursive locker.... */
    pthread_mutex_init(&(queue->locker), 0);
    pthread_cond_init(&(queue->waiter), 0);

    return queue;
}

static void clear_queue(frame_queue_t *q)
{
    if (pthread_mutex_lock(&(q->locker)) != 0) {
        log("failed to lock frame cache\n");

        goto err;
    }

    frame_node_list_free(q->node_list);

    pthread_mutex_unlock(&(q->locker));

err:
    return;
}

int free_cache_node_list(frame_queue_t *queue)
{
    unsigned int i;
    unsigned int list_count = 0;
    frame_node_t *node = NULL;

    list_count = queue->node_list->count;
    for (i = 0; i < list_count; i++) {
        get_node_from_queue(queue, &node);
        if (node == NULL) {
            break;
        }
        free(node->mem);
        free(node);
        node = NULL;
    }
    return 0;
}

int create_cache_node_list(frame_queue_t *q, unsigned int buffer_size, int cache_count)
{
    frame_node_t *n = 0;
    int i = 0;
    unsigned int page_size;
    unsigned int aligned_buffer_size;

    page_size = (unsigned int)getpagesize();

    aligned_buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

    for (; i < cache_count; ++i) {
        n = (frame_node_t *)malloc(sizeof(frame_node_t));
        if (n == NULL) {
            log("failed to malloc frame_node_t\n");
            goto err;
        }

        n->mem = (unsigned char *)malloc(aligned_buffer_size);
        if (n->mem == NULL) {
            log("failed to malloc frame_node_t->mem field\n");
            goto err;
        }

        n->length = aligned_buffer_size;
        n->used = 0;
        n->next = 0;

        put_node_to_queue(q, n);
    }

    printf("%s bufsz:%d  count:%d\n", __func__, buffer_size, cache_count);
    return 0;

err:

    if (n != NULL) {
        free(n);
    }

    return -1;
}

int uvc_frame_nodes_alloc(uvc_cache_t* uvc_cache, unsigned int frame_size)
{
    /* All frame nodes maintained by free_queue. when mpp streaming data is ready,
    *  mpp streaming thread fetch a free frame_node from free_queue, then push into
    *  ok_queue. After frame_node (in ok_queue) really transfer to uvc driver,
    *  application need to pop this frame_node frome ok_queue, and push into free_queue again.
    */
    create_cache_node_list(uvc_cache->free_queue, frame_size, DEFALT_PAYLOAD_CACHE_CNT);
    return 0;
}

int uvc_frame_nodes_free(uvc_cache_t* uvc_cache)
{
    unsigned int i;
    frame_node_t *node = NULL;
    frame_queue_t *ok_queue = uvc_cache->ok_queue;
    frame_queue_t *free_queue = uvc_cache->free_queue;

    for (i = 0; i < ok_queue->node_list->count; i++) {
        get_node_from_queue(ok_queue, &node);
        if (node == NULL) {
            break;
        }
        put_node_to_queue(free_queue, node);
        node = NULL;
    }
    free_cache_node_list(uvc_cache->free_queue);
    return 0;
}

// only one instance for each process.
static uvc_cache_t *g_uvc_cache[MAX_UVC_CACHE_NUMBER];
static unsigned int g_uvc_cache_num = 0;
uvc_cache_t *uvc_cache_get(unsigned int dev_no)
{
    if (dev_no >= MAX_UVC_CACHE_NUMBER) {
        return NULL;
    }
    return g_uvc_cache[dev_no];
}

int create_uvc_cache(unsigned int uvc_num)
{
    unsigned int i;
    uvc_cache_t* uvc_cache = NULL;

    if (uvc_num < 1 || uvc_num > MAX_UVC_CACHE_NUMBER) {
        log("%s failed, invalid uvc_num %u\n", __func__, uvc_num);
        return -1;
    }

    for (i = 0; i < uvc_num; i++) {
        uvc_cache = (uvc_cache_t *)malloc(sizeof(uvc_cache_t));
        if (uvc_cache == NULL) {
            log("malloc uvc_cache failure\n");
            goto err;
        }

        uvc_cache->ok_queue = frame_queue_alloc();
        if (uvc_cache->ok_queue == 0) {
            log("malloc ok_queue failure \n");
            goto err;
        }
        uvc_cache->free_queue = frame_queue_alloc();
        if (uvc_cache->free_queue == 0) {
            log("malloc free_queue failure\n");
            goto err;
        }
        g_uvc_cache[i] = uvc_cache;
        g_uvc_cache_num++;
    }

    return 0;

err:
    for (i = 0; i < g_uvc_cache_num; i++) {
        uvc_cache = g_uvc_cache[i];
        if (uvc_cache->ok_queue != 0) {
            frame_queue_free(uvc_cache->ok_queue);
            uvc_cache->ok_queue = NULL;
        }

        if (uvc_cache->free_queue != 0) {
            frame_queue_free(uvc_cache->free_queue);
            uvc_cache->free_queue = NULL;
        }

        if (uvc_cache != 0) {
            free(uvc_cache);
        }
        g_uvc_cache[i] = NULL;
    }

    g_uvc_cache_num = 0;
    return -1;
}

static int free_uvc_cache(uvc_cache_t *uvc_cache)
{
    if (uvc_cache == NULL) {
        return -1;
    }

    /* free all frame node */
    uvc_frame_nodes_free(uvc_cache);

    /* free queue */
    frame_queue_free(uvc_cache->ok_queue);
    frame_queue_free(uvc_cache->free_queue);

    uvc_cache->ok_queue = NULL;
    uvc_cache->free_queue = NULL;

    free(uvc_cache);
    return 0;
}

void destroy_uvc_cache(void)
{
    unsigned int i = 0;

    for (i = 0; i < g_uvc_cache_num; i++) {
        free_uvc_cache(g_uvc_cache[i]);
        g_uvc_cache[i] = NULL;
    }
}

static int free_uac_cache(uac_cache_t *uac_cache)
{
    /* free queue */
    frame_queue_free(uac_cache->ok_queue);
    frame_queue_free(uac_cache->free_queue);

    uac_cache->ok_queue = NULL;
    uac_cache->free_queue = NULL;

    free(uac_cache);
    return 0;
}

// only one instance for each process.
static uac_cache_t *g_uac_cache = 0;

uac_cache_t *uac_cache_get(void)
{
    return g_uac_cache;
}

int create_uac_cache(void)
{
    uac_cache_t* uac_cache = NULL;
    const int cache_count = 6;
    const unsigned int buffer_size = 640 * cache_count; /* buffer_size = 640 * cache_count */

    uac_cache = (uac_cache_t *)calloc(1, sizeof(uac_cache_t));
    if (uac_cache == 0) {
        log("malloc uac_cache failure\n");
        return -1;
    }

    uac_cache->ok_queue = frame_queue_alloc();
    if (uac_cache->ok_queue == 0) {
        log("malloc ok_queue failure \n");
        goto err;
    }
    uac_cache->free_queue = frame_queue_alloc();
    if (uac_cache->free_queue == 0) {
        log("malloc free_queue failure\n");
        goto err;
    }

    create_cache_node_list(uac_cache->free_queue, buffer_size, cache_count);
    g_uac_cache = uac_cache;
    return 0;

err:
    if (uac_cache->ok_queue != 0) {
        frame_queue_free(uac_cache->ok_queue);
        uac_cache->ok_queue = NULL;
    }

    if (uac_cache->free_queue != 0) {
        frame_queue_free(uac_cache->free_queue);
        uac_cache->free_queue = NULL;
    }

    if (uac_cache != 0) {
        free(uac_cache);
    }

    g_uac_cache = NULL;
    return -1;
}

void destroy_uac_cache(void)
{
    if (uac_cache_get() != 0) {
        free_uac_cache(uac_cache_get());

        g_uac_cache = 0;
    }
}

void clear_uac_cache(void)
{
    if (uac_cache_get() != 0) {
        uac_cache_t *uac_cache = uac_cache_get();

        clear_queue(uac_cache->free_queue);
        clear_queue(uac_cache->ok_queue);
    }
}

int put_node_to_queue(frame_queue_t *q, frame_node_t *node)
{
    if ((q == 0) || (node == 0)) {
        goto err;
    }

    if (pthread_mutex_lock(&(q->locker)) != 0) {
        log("failed to lock frame cache\n");

        goto err;
    }

    if (frame_node_list_push_node(q->node_list, node) != 0) {
        pthread_mutex_unlock(&(q->locker));

        goto err;
    }

    pthread_mutex_unlock(&(q->locker));

    return 0;

err:
    return -1;
}

int get_node_from_queue(frame_queue_t *q, frame_node_t **node)
{
    if ((q == 0) || (node == 0)) {
        goto err;
    }

    if (pthread_mutex_lock(&(q->locker)) != 0) {
        log("failed to lock frame cache\n");

        goto err;
    }

    if (frame_node_list_pop_node(q->node_list, node) != 0) {
        pthread_mutex_unlock(&(q->locker));

        goto err;
    }

    pthread_mutex_unlock(&(q->locker));

    return 0;
err:
    return -1;
}
