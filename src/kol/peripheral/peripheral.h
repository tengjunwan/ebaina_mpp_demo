/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#ifndef __PERIPHERAL_H__
#define __PERIPHERAL_H__

#include "ot_type.h"

typedef enum {
    WAKEUP_TIMER_MODE = 0,
    WAKEUP_4G_MODE,
} wakeup_mode;

typedef enum {
    MCU_MODE_KOL = 0,
    MCU_MODE_NORMAL,
} mcu_mode;

typedef enum {
    MCU_SWITCH_STATE_NONE = 0,
    MCU_SWITCH_STATE_ENTER_SINGLE,
    MCU_SWITCH_STATE_EXIT_SINGLE,
} mcu_switch_state;

typedef struct {
    wakeup_mode wakeup;
    mcu_switch_state switch_state;
    mcu_mode mode;
    td_bool has_power_test_data;
} mcu_notify_info;

td_bool peripheral_mode_flag1(td_void);
td_void peripheral_get_mcu_notify(mcu_notify_info *info);
td_s32 peripheral_set_uart_mode(mcu_mode mode);
td_s32 peripheral_set_sleep_time(td_u16 milli_sec);
td_void peripheral_get_power_test(td_void);
td_s32 peripheral_mcu_init(td_void);
td_void peripheral_mcu_deinit(td_void);

td_void kol_set_wakeup_record(td_bool is_record);
td_bool kol_get_wakeup_record(td_void);

td_s32 kol_pwm_export(td_u32 pwm_num);
td_s32 kol_pwm_unexport(td_u32 pwm_num);
td_s32 kol_pwm_set_period(td_u32 pwm_num, td_u32 period);
td_s32 kol_pwm_set_duty_cycle(td_u32 pwm_num, td_u32 duty_cycle);
td_s32 kol_pwm_set_polarity(td_u32 pwm_num, const td_char *polarity);
td_s32 kol_pwm_enable(td_u32 pwm_num);
td_s32 kol_pwm_disable(td_u32 pwm_num);

td_s32 kol_gpio_export(td_u32 gpio_num);
td_s32 kol_gpio_unexport(td_u32 gpio_num);
td_s32 kol_set_gpio_direction(td_u32 gpio_num, const td_char *direction);
td_s32 kol_set_gpio_edge(td_u32 gpio_num, const td_char *edge);
td_s32 kol_read_gpio_value(td_u32 gpio_num);
td_s32 kol_write_gpio_value(td_u32 gpio_num, td_u32 value);

td_void register_wakeup_trigger(td_void);
td_void unregister_wakeup_trigger(td_void);
td_void wakeup_timer_start(td_s32 ms);
td_void wakeup_timer_stop(td_void);

#endif
