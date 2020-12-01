#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <ev.h>

#include <iostream>

#include "codec/codec_http.h"
#include "codec/codec_proto.h"
#include "events.h"
#include "protobuf/proto/http.pb.h"
#include "timer.h"
#include "util/log.h"
#include "util/socket_buffer.h"

namespace kim {

typedef struct fd_s {
    int fd;
    uint64_t id;
} fd_t;

class Connection : public Timer, Logger {
   public:
    enum class STATE {
        UNKOWN = 0,
        TRY_CONNECT,
        CONNECTING,
        CONNECTED,
        CLOSED,
        ERROR
    };

    Connection(Log* logger, int fd, uint64_t id);
    virtual ~Connection();

    bool init(Codec::TYPE codec);
    bool is_http();

    int fd() { return m_fd_data.fd; }
    uint64_t id() const { return m_fd_data.id; }
    const fd_t& fd_data() const { return m_fd_data; }
    void set_fd_data(int fd, uint64_t id) { m_fd_data = {fd, id}; }

    double now();
    void set_events(Events* e) { m_events = e; }

    void set_privdata(void* data) { m_privdata = data; }
    void* privdata() const { return m_privdata; }

    void set_state(STATE state) { m_state = state; }
    STATE state() const { return m_state; }
    bool is_connected() { return m_state == STATE::CONNECTED; }
    bool is_closed() { return m_state == STATE::CLOSED; }
    bool is_connecting() { return m_state == STATE::CONNECTING; }
    bool is_try_connect() { return m_state == STATE::TRY_CONNECT; }
    bool is_invalid() { return (m_state == STATE::UNKOWN || m_state == STATE::CLOSED || m_state == STATE::ERROR); }

    void set_errno(int err) { m_errno = err; }
    int get_errno() const { return m_errno; }

    void set_ev_io(ev_io* w) { m_ev_io = w; }
    ev_io* get_ev_io() const { return m_ev_io; }

    void set_addr_info(struct sockaddr* saddr, size_t saddr_len);
    struct sockaddr* sockaddr();
    size_t saddr_len() { return m_saddr_len; }

    void set_node_id(const std::string& node_id) { m_node_id = node_id; }
    const std::string& get_node_id() const { return m_node_id; }

    Codec::STATUS conn_read(HttpMsg& msg);
    Codec::STATUS conn_write(const HttpMsg& msg);
    Codec::STATUS conn_write_waiting(HttpMsg& msg);
    Codec::STATUS fetch_data(HttpMsg& msg);

    Codec::STATUS conn_read(MsgHead& head, MsgBody& body);
    Codec::STATUS fetch_data(MsgHead& head, MsgBody& body);
    Codec::STATUS conn_write(const MsgHead& head, const MsgBody& body);
    Codec::STATUS conn_write_waiting(const MsgHead& head, const MsgBody& body);
    Codec::STATUS conn_write();

    virtual bool is_need_alive_check();
    virtual double keep_alive();

    /* statistics api. */
    int write_cnt() { return m_write_cnt; }
    uint64_t write_bytes() { return m_write_bytes; }
    int read_cnt() { return m_read_cnt; }
    uint64_t read_bytes() { return m_read_bytes; }

   private:
    bool conn_read();
    Codec::STATUS decode_http(HttpMsg& msg);
    Codec::STATUS decode_proto(MsgHead& head, MsgBody& body);
    Codec::STATUS conn_write(const HttpMsg& msg, SocketBuffer** buf);
    Codec::STATUS conn_write(const MsgHead& head, const MsgBody& body, SocketBuffer** buf, bool is_send = true);

   private:
    fd_t m_fd_data;
    void* m_privdata = nullptr;  // private data.
    ev_io* m_ev_io = nullptr;    // libev io event.
    Codec* m_codec = nullptr;
    Events* m_events = nullptr;

    STATE m_state = STATE::UNKOWN;  // connection status.
    int m_errno = 0;                // error number.

    SocketBuffer* m_recv_buf = nullptr;
    SocketBuffer* m_send_buf = nullptr;
    SocketBuffer* m_wait_send_buf = nullptr;

    size_t m_saddr_len = 0;
    struct sockaddr* m_saddr = nullptr;
    std::string m_node_id; /* for nodes contact. */

    /* statistics info. */
    int m_read_cnt = 0;
    uint64_t m_read_bytes = 0;
    int m_write_cnt = 0;
    uint64_t m_write_bytes = 0;
};

}  // namespace kim

#endif  //__CONTEXT_H__