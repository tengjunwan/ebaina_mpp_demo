#include "sns_type.h"

static sample_sns_type g_sns_type[SAMPLE_SENSOR_MAX_NUM] = {
    SENSOR0_TYPE,
    SENSOR1_TYPE
};

td_bool sample_set_sns_type(sample_sns_type sns_type, td_u32 index)
{
    if (index >= SAMPLE_SENSOR_MAX_NUM) {
        return TD_FALSE;
    }

    g_sns_type[index] = sns_type;
    return TD_TRUE;
}

void sample_set_all_sns_type(sample_sns_type type)
{
    td_u32 i;
    printf("[wtjdebug][sns_type][sample_set_all_sns_type]: SAMPLE_SENSOR_MAX_NUM = %d\n", SAMPLE_SENSOR_MAX_NUM);
    for (i = 0; i < SAMPLE_SENSOR_MAX_NUM; i++) {
        g_sns_type[i] = type;
    }
}

sample_sns_type sample_get_sns_type(td_u32 index)
{
    if (index >= SAMPLE_SENSOR_MAX_NUM) {
        return SC4336P_MIPI_4M_30FPS_10BIT;
    }

    return g_sns_type[index];
}