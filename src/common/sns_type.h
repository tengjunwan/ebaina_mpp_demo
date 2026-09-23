#ifndef __SNS_TYPE_H__
#define __SNS_TYPE_H__

#include "sample_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAMPLE_SENSOR_MAX_NUM 2

td_bool sample_set_sns_type(sample_sns_type sns_type, td_u32 index);
void sample_set_all_sns_type(sample_sns_type type);
sample_sns_type sample_get_sns_type(td_u32 index);

#ifdef __cplusplus
}
#endif

#endif /* __SNS_TYPE_H__ */