#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <ev.h>

#include <iostream>

#include "codec/codec_http.h"
#include "codec/codec_proto.h"
#include "protobuf/proto/http.pb.h"
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
        CONNECTING = 0,
        ACCEPTING,
        CONNECTED,
        CLOSED,
        ERROR
    };

    Connection(Log* logger, int fd, uint64_t id);
    virtual ~Connection();

    bool init(Codec::TYPE codec);

    void set_fd(int fd) { m_fd = fd; }
    int fd() const { return m_fd; }

    uint64_t id() const { return m_id; }

    void set_privdata(void* data) { m_privdata = data; }
    void* privdata() const { return m_privdata; }

    void set_addr_info(struct sockaddr* saddr, size_t saddr_len);
    struct sockaddr* sockaddr();
    size_t saddr_len() { return m_saddr_len; }

    void set_state(STATE state) { m_state = state; }
    STATE state() const { return m_state; }
    bool is_connected() { return m_state == STATE::CONNECTED; }
    bool is_closed() { return m_state == STATE::CLOSED; }
    bool is_connecting() { return m_state == STATE::CONNECTING; }

    void set_errno(int err) { m_errno = err; }
    int get_errno() const { return m_errno; }

    void set_ev_io(ev_io* w) { m_ev_io = w; }
    ev_io* get_ev_io() const { return m_ev_io; }

    bool is_http_codec();

    Codec::STATUS conn_read(HttpMsg& msg);
    Codec::STATUS conn_write(const HttpMsg& msg);
    Codec::STATUS fetch_data(HttpMsg& msg);

    Codec::STATUS conn_read(MsgHead& head, MsgBody& body);
    Codec::STATUS fetch_data(MsgHead& head, MsgBody& body);
    Codec::STATUS conn_write(const MsgHead& head, const MsgBody& body);
    Codec::STATUS conn_write();

    virtual bool is_need_alive_check();
    virtual double keep_alive();

   protected:
    bool conn_read();
    Codec::STATUS decode_http(HttpMsg& msg);
    Codec::STATUS decode_proto(MsgHead& head, MsgBody& body);

   private:
    uint64_t m_id = 0;           // sequence.
    Log* m_logger = nullptr;     // logger.]
    void* m_privdata = nullptr;  // private data.
    ev_io* m_ev_io = nullptr;    // libev io event obj.
    Codec* m_codec = nullptr;

    int m_fd = -1;                     // socket fd.
    STATE m_state = STATE::CONNECTED;  // connection status.
    int m_errno = 0;                   // error number.

    SocketBuffer* m_recv_buf = nullptr;
    SocketBuffer* m_send_buf = nullptr;
    SocketBuffer* m_wait_send_buf = nullptr;

    size_t m_saddr_len = 0;
    struct sockaddr* m_saddr = nullptr;
};

}  // namespace kim

#endif  //__CONTEXT_H__