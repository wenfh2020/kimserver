#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <ev.h>

#include <iostream>

#include "codec/codec_http.h"
#include "proto/http.pb.h"
#include "util/log.h"
#include "util/sds.h"
#include "util/socket_buffer.h"

namespace kim {

class Connection {
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

    bool init(Codec::TYPE code_type);

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

    void set_active_time(long long t) { m_active_time = t; }
    long long get_active_time() const { return m_active_time; }

    bool is_http_codec();

    Codec::STATUS conn_read(HttpMsg& msg);
    Codec::STATUS conn_write(const HttpMsg& msg);

   private:
    uint64_t m_id = 0;               // sequence.
    Log* m_logger = nullptr;         // logger.
    void* m_private_data = nullptr;  // private data.

    int m_fd = -1;                // socket fd.
    STATE m_state = STATE::NONE;  // connection status.
    int m_errno = 0;              // error number.

    ev_io* m_ev_io = nullptr;     // libev io event obj.
    long long m_active_time = 0;  // connection last active (read/write) time.

    SocketBuffer* m_recv_buf;
    SocketBuffer* m_send_buf;
    SocketBuffer* m_wait_send_buf;
    Codec* m_codec = nullptr;
};

}  // namespace kim

#endif  //__CONTEXT_H__