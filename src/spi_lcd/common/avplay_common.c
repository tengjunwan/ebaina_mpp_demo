/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
 
#include "avplay_common.h"
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include "securec.h"


#define AVPLAY_MAX_PRINT_STR_LEN  1024
#define SS2US  1000000
#define US2NS  1000
#define US2MS  1000
#define US2MS_HALF  500
#define S2MS  1000

td_void AVPLAY_RETURN_VAL_IF_NULL(const td_void *condition)
{
    if ((condition) == TD_NULL) {
        AV_PRINTF("NULL error %s %d\n", __FUNCTION__, __LINE__);
    }
}

td_void AVPLAY_LOCK(pthread_mutex_t mutex)
{
    (td_void) pthread_mutex_lock(&(mutex));
}

td_void AVPLAY_UNLOCK(pthread_mutex_t mutex)
{
    (td_void) pthread_mutex_unlock(&(mutex));
}

td_void AV_TRACE(const td_char *fmt, ...)
{
    td_char print_str[AVPLAY_MAX_PRINT_STR_LEN] = { 0 };

    if (fmt == NULL) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    if (vsnprintf_s(print_str, AVPLAY_MAX_PRINT_STR_LEN, AVPLAY_MAX_PRINT_STR_LEN - 1, fmt, args) < 0) {
        va_end(args);
        return;
    }
    va_end(args);
    AV_PRINTF("[avplay] %s", print_str);
}
