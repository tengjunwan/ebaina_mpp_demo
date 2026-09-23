/*
  Copyright (c), 2001-2025, Shenshu Tech. Co., Ltd.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include "kol_timestamp.h"

typedef struct {
    td_u32 restart_times;
    td_u32 standby_times;
} kol_sensor_statistics;

static kol_timestamp g_timestamp;
static td_bool g_switch_open = TD_FALSE;
static kol_sensor_statistics g_sensor_statistics[MAX_ROUTE_CNT];

kol_timestamp *kol_get_timestamp(td_void)
{
    return &g_timestamp;
}

td_void set_statistics_switch(td_bool is_open)
{
    g_switch_open = is_open;
}

td_bool get_statistics_switch(td_void)
{
    return g_switch_open;
}

static td_void print_timestamp_by_route(td_s32 fd, td_u32 route_id)
{
    dprintf(fd, "[%u]restart %u times\n", route_id, g_sensor_statistics[route_id].restart_times);
    dprintf(fd, "[%u]standby %u times\n", route_id, g_sensor_statistics[route_id].standby_times);
    dprintf(fd, "[%u]%llu before_osd\n", route_id, g_timestamp.before_osd[route_id]);
    dprintf(fd, "[%u]%llu after_osd\n", route_id, g_timestamp.after_osd[route_id]);
    dprintf(fd, "[%u]%llu sensor_restart\n", route_id, g_timestamp.sensor_restart[route_id]);
    dprintf(fd, "[%u]%llu frame_pts\n", route_id, g_timestamp.frame_pts[route_id]);
    dprintf(fd, "[%u]%llu run_end_pts\n", route_id, g_timestamp.run_end_pts[route_id]);
    dprintf(fd, "[%u]%llu before_aidetect\n", route_id, g_timestamp.before_aidetect[route_id]);
    dprintf(fd, "[%u]%llu after_aidetect\n", route_id, g_timestamp.after_aidetect[route_id]);
    dprintf(fd, "[%u]%llu before_wait_venc\n", route_id, g_timestamp.before_wait_venc[route_id]);
    dprintf(fd, "[%u]%llu after_wait_venc\n", route_id, g_timestamp.after_wait_venc[route_id]);

    dprintf(fd, "[%u]before_osd to after_osd: %llu\n",
        route_id, g_timestamp.after_osd[route_id] - g_timestamp.before_osd[route_id]);
    if (g_timestamp.sensor_restart[route_id] != 0) {
        dprintf(fd, "[%u]sensor_restart to frame_pts: %llu\n",
            route_id, g_timestamp.frame_pts[route_id] - g_timestamp.sensor_restart[route_id]);
    } else {
        dprintf(fd, "[%u]after_suspend to frame_pts: %d\n",
            route_id, (td_s32)(g_timestamp.frame_pts[route_id] - g_timestamp.after_suspend));
    }
    dprintf(fd, "[%u]frame_pts to run_end_pts: %llu\n",
        route_id, g_timestamp.run_end_pts[route_id] - g_timestamp.frame_pts[route_id]);
    if (g_timestamp.before_aidetect[route_id] != 0) {
        dprintf(fd, "[%u]run_end_pts to before_aidetect: %llu\n",
            route_id, g_timestamp.before_aidetect[route_id] - g_timestamp.run_end_pts[route_id]);
    }
    if (g_timestamp.before_aidetect[route_id] != 0 && g_timestamp.after_aidetect[route_id] != 0) {
        dprintf(fd, "[%u]before_aidetect to after_aidetect: %llu\n",
            route_id, g_timestamp.after_aidetect[route_id] - g_timestamp.before_aidetect[route_id]);
    }
    if (g_timestamp.after_aidetect[route_id] != 0) {
        dprintf(fd, "[%u]after_aidetect to before_wait_venc: %llu\n",
            route_id, g_timestamp.before_wait_venc[route_id] - g_timestamp.after_aidetect[route_id]);
    }
    dprintf(fd, "[%u]before_wait_venc to after_wait_venc: %llu\n",
        route_id, g_timestamp.after_wait_venc[route_id] - g_timestamp.before_wait_venc[route_id]);
    dprintf(fd, "[%u]after_wait_venc to before_suspend: %llu\n",
        route_id, g_timestamp.before_suspend - g_timestamp.after_wait_venc[route_id]);
}

td_void kol_show_timestamp(td_u32 route_num)
{
    if (!g_switch_open) {
        return;
    }
    td_s32 fd = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        return;
    }
    dprintf(fd, "after_suspend %llu\n", g_timestamp.after_suspend);
    for (td_u32 i = 0; i < route_num; i++) {
        print_timestamp_by_route(fd, i);
    }
    dprintf(fd, "before_suspend %llu\n", g_timestamp.before_suspend);
    dprintf(fd, "total: %llu\n", g_timestamp.before_suspend - g_timestamp.after_suspend);
    tcdrain(fd);
    close(fd);
}

td_void update_sensor_restart_times(td_u32 route_id)
{
    if (route_id < MAX_ROUTE_CNT) {
        g_sensor_statistics[route_id].restart_times++;
    }
}

td_void update_sensor_standby_times(td_u32 route_id)
{
    if (route_id < MAX_ROUTE_CNT) {
        g_sensor_statistics[route_id].standby_times++;
    }
}
