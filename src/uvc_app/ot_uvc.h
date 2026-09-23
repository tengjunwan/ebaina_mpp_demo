/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __OT_UVC_H__
#define __OT_UVC_H__

#include <stdint.h>

typedef struct ot_uvc {
    int (*init)(uint32_t dev_no);
    int (*open)(uint32_t dev_no);
    int (*close)(uint32_t dev_no);
    int (*run)(uint32_t dev_no);
} ot_uvc;

ot_uvc *get_ot_uvc(void);

#endif // __OT_UVC_H__
