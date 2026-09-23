#include "rtsp_manager.h"

RtspManager& RtspManager::Instance()
{
    static RtspManager inst;
    return inst;
}

bool RtspManager::StartServer(const std::string& ip, int port)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_server) return true;

    m_loop = std::make_shared<xop::EventLoop>();
    m_server = xop::RtspServer::Create(m_loop.get());

    // std::string local_ip = xop::NetInterface::GetLocalIPAddress();
    m_server_ip = ip;
    m_port = port;

    if (!m_server->Start(m_server_ip, m_port)) {
        printf("RTSP server start failed\n");
        return false;
    }
    return true;
}

bool RtspManager::CreateSession(int index, bool is_h265)
{
    if (index < 0 || index >= MAX_RTSP_SESSION) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_server || m_sessions[index].used) {
        return false;
    }

    char suffix[16];
    snprintf(suffix, sizeof(suffix), "live%d", index);

    auto session = xop::MediaSession::CreateNew(suffix);

    if (is_h265) {
        session->AddSource(xop::channel_0, xop::H265Source::CreateNew());
    } else {
        session->AddSource(xop::channel_0, xop::H264Source::CreateNew());
    }

    session->AddNotifyConnectedCallback(
        [](xop::MediaSessionId, std::string ip, uint16_t port) {
            printf("RTSP client connected %s:%d\n", ip.c_str(), port);
        });

    session->AddNotifyDisconnectedCallback(
        [](xop::MediaSessionId, std::string ip, uint16_t port) {
            printf("RTSP client disconnected %s:%d\n", ip.c_str(), port);
        });

    auto sid = m_server->AddSession(session);

    auto& ctx = m_sessions[index];
    ctx.used = true;
    ctx.is_h265 = is_h265;
    ctx.session_id = sid;

    printf("RTSP session created: rtsp://%s:%d/%s\n", m_server_ip.c_str(), m_port, suffix);
    return true;
}

bool RtspManager::PushFrame(int index, uint8_t* data, size_t len, bool is_key)
{
    if (index < 0 || index >= MAX_RTSP_SESSION || !data || len == 0) {
        return false;
    }

    auto& ctx = m_sessions[index];
    if (!ctx.used) {
        return false;
    }

    xop::AVFrame frame;
    frame.size = len;
    frame.buffer.reset(new uint8_t[len]);
    memcpy(frame.buffer.get(), data, len);
    frame.type = is_key ? xop::VIDEO_FRAME_I : xop::VIDEO_FRAME_P;
    frame.timestamp = GetTimestamp(ctx.is_h265);

    return m_server->PushFrame(ctx.session_id, xop::channel_0, frame);
}

void RtspManager::StopServer()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_server) {
        return;
    }

    for (int i = 0; i < MAX_RTSP_SESSION; ++i) {
        if (m_sessions[i].used) {
            m_server->RemoveSession(m_sessions[i].session_id);
            m_sessions[i].used = false;
            m_sessions[i].session_id = 0;
        }
    }

    m_server.reset();
    m_loop.reset();
}


uint32_t RtspManager::GetTimestamp(bool is_h265)
{
    return is_h265 ? xop::H265Source::GetTimestamp() : xop::H264Source::GetTimestamp();
}
