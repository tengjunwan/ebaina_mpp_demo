#ifndef RTSP_SERVER_API_H
#define RTSP_SERVER_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 RTSP Server
 * 返回 1 成功，0 失败
 */
int rtsp_server_start(const char* ip, int port);

/* 创建 session
 * index: 0~7  -> live0 ~ live7
 * is_h265: 1 = H265, 0 = H264
 */
int rtsp_session_create(int index, int is_h265);

/* 推送一帧视频
 * is_key: 1 = IDR/I帧，0 = P帧
 */
int rtsp_session_push_frame(int index, uint8_t* data, size_t len, int is_key);

/* 停止 server（会清理所有 session） */
void rtsp_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* RTSP_SERVER_API_H */
