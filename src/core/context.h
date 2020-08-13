#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <ev.h>

#include <iostream>

#include "codec/codec_http.h"
#include "codec/codec_proto.h"
#include "proto/http.pb.h"
#include "timer.h"
#include "util/log.h"
#include "util/socket_buffer.h"

namespace kim {

class Connection;

class ConnectionData {
   public:
    std::shared_ptr<Connection> m_conn = nullptr;
};

class Connection : public Timer {
   public:
    enum class STATE {
        NONE = 0,
        CONNECTING,
        ACCEPTING,
        CONNECTED,
        CLOSED,
        ERROR
    };

    Connection(Log* logger, int fd, uint64_t id);
    virtual ~Connection();

    bool init(Codec::TYPE codec);

    void set_fd(int fd) { m_fd = fd; }
    int get_fd() const { return m_fd; }

    uint64_t get_id() const { return m_id; }

    void set_private_data(void* data) { m_private_data = data; }
    void* get_private_data() const { return m_private_data; }

    void set_state(STATE state) { m_state = state; }
    STATE get_state() const { return m_state; }
    bool is_active() { return m_state == STATE::CONNECTED; }
    bool is_closed() { return m_state == STATE::CLOSED; }

    void set_errno(int err) { m_errno = err; }
    int get_errno() const { return m_errno; }

    void set_ev_io(ev_io* w) { m_ev_io = w; }
    ev_io* get_ev_io() const { return m_ev_io; }

    bool is_http_codec();

    Codec::STATUS conn_read(HttpMsg& msg);
    Codec::STATUS conn_write(const HttpMsg& msg);

    Codec::STATUS conn_read(MsgHead& head, MsgBody& body);
    Codec::STATUS conn_write(const MsgHead& head, const MsgBody& body);

    virtual bool is_need_alive_check();
    virtual double get_keep_alive();

   protected:
    bool conn_read();
    Codec::STATUS conn_write();
    Codec::STATUS decode_http(HttpMsg& msg);
    Codec::STATUS decode_proto(MsgHead& head, MsgBody& body);

   private:
    uint64_t m_id = 0;               // sequence.
    Log* m_logger = nullptr;         // logger.
    void* m_private_data = nullptr;  // private data.
    ev_io* m_ev_io = nullptr;        // libev io event obj.
    Codec* m_codec = nullptr;

    int m_fd = -1;                // socket fd.
    STATE m_state = STATE::NONE;  // connection status.
    int m_errno = 0;              // error number.

    SocketBuffer* m_recv_buf = nullptr;
    SocketBuffer* m_send_buf = nullptr;
    SocketBuffer* m_wait_send_buf = nullptr;
};

}  // namespace kim

#endif  //__CONTEXT_H__