# Notice
1. Change Makefile `AIDETECT_ENABLE ?= n` to `AIDETECT_ENABLE ?= y` to use AIDETECT, depends on AiComponent package
2. Change Makefile `LIGHT_ENABLE ?= n` to `LIGHT_ENABLE ?= y` to use LIGHT control, depends on pwm driver is open and change the pin mux, change maco define `KOL_WHITE_LIGHT_xxx` to use ohter pin or param
default:
```bash
bspmm 0x17940074 0x1105 # use PWM1_OUT1 for white light
bspmm 0x1794007c 0x1105 # use PWM1_OUT3 for ir light
```
3. Change Makefile `IR_CUT_ENABLE ?= n` to `IR_CUT_ENABLE ?= y` to use ir cut control, depends on gpio driver is open and change the pin mux
Use `-ir_auto 1` when load35xxx to use default ir cut and change Makefile.param `BOARD_TYPE` to change board, or change state_machine_action.c `g_ir_cut_gpio` array to use other gpio
4. Only support one route for now
