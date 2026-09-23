/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __FRAME_CACHE_H__
#define __FRAME_CACHE_H__

#include <pthread.h>

#define MAX_UVC_CACHE_NUMBER 3
#define DEFALT_PAYLOAD_CACHE_CNT 3

typedef struct frame_node_t {
    unsigned char *mem;
    unsigned int length;
    unsigned int used;
    unsigned int index;
    struct frame_node_t *next;
} frame_node_t;

typedef struct frame_node_list_t {
    struct frame_node_t *head;
    struct frame_node_t *tail;
    unsigned int count;
} frame_node_list_t;

typedef struct frame_queue_t {
    struct frame_node_list_t *node_list;
    pthread_mutex_t locker;
    pthread_cond_t waiter;
} frame_queue_t;

typedef struct uvc_cache_t {
    frame_queue_t *ok_queue;
    frame_queue_t *free_queue;

    unsigned char debug_print;
} uvc_cache_t;

typedef struct uac_cache_t {
    frame_queue_t *ok_queue;
    frame_queue_t *free_queue;
} uac_cache_t;

int uvc_frame_nodes_alloc(uvc_cache_t* uvc_cache, unsigned int frame_size);
int uvc_frame_nodes_free(uvc_cache_t* uvc_cache);

int free_cache_node_list(frame_queue_t *queue);
int create_cache_node_list(frame_queue_t *q, unsigned int buffer_size, int cache_count);

int create_uvc_cache(unsigned int uvc_num);
void destroy_uvc_cache(void);
void clear_uvc_cache(void);
uvc_cache_t *uvc_cache_get(unsigned int dev_no);

int create_uac_cache(void);
void destroy_uac_cache(void);
void clear_uac_cache(void);
uac_cache_t *uac_cache_get(void);

int put_node_to_queue(frame_queue_t *q, frame_node_t *node);
int get_node_from_queue(frame_queue_t *q, frame_node_t **node);

#endif // __FRAME_CACHE_H__
