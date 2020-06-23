#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <iostream>

namespace kim {

class Connection {
   public:
    typedef enum {
        CONN_STATE_NONE = 0,
        CONN_STATE_CONNECTING,
        CONN_STATE_ACCEPTING,
        CONN_STATE_CONNECTED,
        CONN_STATE_CLOSED,
        CONN_STATE_ERROR
    } CONN_STATE;

    Connection(int fd = -1, uint64_t id = 0);
    virtual ~Connection() {}

    void set_fd(int fd) { m_fd = fd; }
    int get_fd() { return m_fd; }

    void set_id(uint64_t id) { m_id = id; }
    uint64_t get_id() { return m_id; }

    void set_private_data(void* data) { m_private_data = data; }
    void* get_private_data() { return m_private_data; }

    void set_state(CONN_STATE state) { m_state = state; }
    CONN_STATE get_state() { return m_state; }

    void set_errno(int err) { m_errno = err; }

   private:
    int m_fd;
    uint64_t m_id;
    void* m_private_data;
    CONN_STATE m_state;
    int m_errno;
    std::string m_error;
};

}  // namespace kim

#endif  //__CONTEXT_H__