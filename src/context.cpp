#include "context.h"

#include <unistd.h>

namespace kim {

#define PROTO_IOBUF_LEN (1024 * 16) /* Generic I/O buffer size */

Connection::Connection(int fd, uint64_t id) : m_fd(fd),
                                              m_id(id),
                                              m_private_data(NULL),
                                              m_ev_io(NULL),
                                              m_qb_pos(0) {
    m_query_buf = sdsempty();
    m_qb_pos = 0;
}

Connection::~Connection() {
    sdsfree(m_query_buf);
}

int Connection::conn_read(void *buf, size_t buf_len) {
    int ret = read(m_fd, buf, buf_len);
    if (!ret) {
        m_state = CONN_STATE_CLOSED;
    } else if (ret < 0 && errno != EAGAIN) {
        m_errno = errno;
        m_state = CONN_STATE_ERROR;
    }

    return ret;
}

int Connection::read_data() {
    int nread, readlen;
    size_t qblen;

    readlen = PROTO_IOBUF_LEN;
    qblen = sdslen(m_query_buf);
    m_query_buf = sdsMakeRoomFor(m_query_buf, readlen);
    nread = conn_read(m_query_buf + qblen, readlen);
    if (nread > 0) {
        sdsIncrLen(m_query_buf, nread);
    }
    return nread;
}

}  // namespace kim