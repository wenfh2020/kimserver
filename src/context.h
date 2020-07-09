#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <ev.h>

#include <iostream>

#include "codec/codec.h"
#include "util/sds.h"

namespace kim {

class Connection {
   public:
    enum class CONN_STATE {
        NONE = 0,
        CONNECTING,
        ACCEPTING,
        CONNECTED,
        CLOSED,
        ERROR
    };

    Connection(int fd, uint64_t id);
    virtual ~Connection();

    void set_fd(int fd) { m_fd = fd; }
    int get_fd() { return m_fd; }

    void set_id(uint64_t id) { m_id = id; }
    uint64_t get_id() { return m_id; }

    void set_private_data(void* data) { m_private_data = data; }
    void* get_private_data() { return m_private_data; }

    void set_state(CONN_STATE state) { m_state = state; }
    CONN_STATE get_state() { return m_state; }
    bool is_active() { return m_state == CONN_STATE::CONNECTED; }
    bool is_closed() { return m_state == CONN_STATE::CLOSED; }

    void set_errno(int err) { m_errno = err; }

    void set_ev_io(ev_io* w) { m_ev_io = w; }
    ev_io* get_ev_io() { return m_ev_io; }

    void set_active_time(ev_tstamp t) { m_active_time = t; }
    ev_tstamp get_active_time() { return m_active_time; }

    int read_data();
    const char* get_query_data() { return m_query_buf; }

    // Codec& get_codec() { return m_codec; }

   private:
    int conn_read(void* buf, size_t buf_len);

   private:
    int m_fd = -1;                          // socket fd.
    uint64_t m_id = 0;                      // sequence.
    void* m_private_data = nullptr;         // private data.
    CONN_STATE m_state = CONN_STATE::NONE;  // connection status.
    int m_errno = 0;                        // error number.
    std::string m_error;                    // error string.

    ev_io* m_ev_io = nullptr;       // libev io event obj.
    ev_tstamp m_active_time = 0.0;  // connection last active (read/write) time.

    // Codec m_codec;        // proto codec.
    size_t m_qb_pos = 0;  // query buf position.
    sds m_query_buf;      // query buf.
};

}  // namespace kim

#endif  //__CONTEXT_H__