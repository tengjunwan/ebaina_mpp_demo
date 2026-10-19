/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */
#ifndef SAMPLE_SVP_SVC_PROCESS_H
#define SAMPLE_SVP_SVC_PROCESS_H

#include "ot_type.h"

/* function : show the sample of acl resnet50 */
td_void sample_svp_npu_acl_resnet50(td_void);

/* function : show the sample of acl resnet50_multithread */
td_void sample_svp_npu_acl_resnet50_multi_thread(td_void);

/* function : show the sample of resnet50 dyanamic batch with mem cached */
td_void sample_svp_npu_acl_resnet50_dynamic_batch(td_void);

/* function : show the sample of lstm */
td_void sample_svp_npu_acl_lstm(td_void);

/* function : show the sample of preemption */
td_void sample_svp_npu_acl_preemption(td_void);

/* function : show the sample of aipp */
td_void sample_svp_npu_acl_aipp(td_void);

/* function : show the sample of e2e yolo */
td_void sample_svp_npu_acl_e2e_yolo(td_u32 index);

/* function : show the sample of e2e hrnet */
td_void sample_svp_npu_acl_e2e_hrnet(td_void);

/* function : show the sample of sign handle */
td_void sample_svp_npu_acl_handle_sig(td_void);

#endif
