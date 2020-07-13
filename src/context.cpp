#include "context.h"

#include <unistd.h>

#include "util/util.h"

namespace kim {

#define PROTO_IOBUF_LEN (1024 * 16) /* Generic I/O buffer size */

Connection::Connection(Log* logger, int fd, uint64_t id)
    : m_id(id), m_logger(logger), m_fd(fd) {
    m_query_buf = sdsempty();
    m_qb_pos = 0;
}

Connection::~Connection() {
    sdsfree(m_query_buf);
}

bool Connection::init(Codec::TYPE code_type) {
    LOG_DEBUG("connection init fd: %d, codec type: %d", m_fd, (int)code_type);

    try {
        m_recv_buf = new SocketBuffer;
        m_send_buf = new SocketBuffer;
        m_wait_send_buf = new SocketBuffer;
        switch (code_type) {
            case Codec::TYPE::HTTP:
                m_codec = new CodecHttp(m_logger, code_type);
                break;

            default:
                LOG_ERROR("invalid codec type: %d", (int)code_type);
                break;
        }
    } catch (std::bad_alloc& e) {
        LOG_ERROR("%s", e.what());
        return false;
    }

    if (m_codec != nullptr) {
        m_codec->set_codec_type(code_type);
    }
    return true;
}

// int Connection::conn_read(void* buf, size_t buf_len) {
//     int ret = read(m_fd, buf, buf_len);
//     if (ret == 0) {
//         m_state = STATE::CLOSED;
//     } else if (ret < 0 && errno != EAGAIN) {
//         m_state = STATE::ERROR;
//     }
//     m_errno = errno;
//     return ret;
// }

int Connection::read_data() {
    // size_t qblen;
    // int nread, readlen;

    // readlen = PROTO_IOBUF_LEN;
    // qblen = sdslen(m_query_buf);
    // m_query_buf = sdsMakeRoomFor(m_query_buf, readlen);
    // nread = conn_read(m_query_buf + qblen, readlen);
    // if (nread > 0) {
    //     sdsIncrLen(m_query_buf, nread);
    // }
    // return nread;
    return 0;
}

bool Connection::is_http_codec() {
    if (m_codec == nullptr) {
        return false;
    }
    return m_codec->get_codec_type() == Codec::TYPE::HTTP;
}

int Connection::conn_read() {
    if (m_recv_buf == nullptr) {
        return 0;
    }

    int ret = 0;

    ret = m_recv_buf->read_fd(m_fd, m_errno);
    m_errno = errno;
    LOG_DEBUG("read from fd: %d, data len: %d, readed data len: %d",
              m_fd, ret, m_recv_buf->get_readable_len());
    if (ret > 0) {
        if (m_recv_buf->capacity() > SocketBuffer::BUFFER_MAX_READ &&
            m_recv_buf->get_readable_len() < m_recv_buf->capacity() / 2) {
            m_recv_buf->compact(m_recv_buf->get_readable_len() * 2);
        }
        m_active_time = mstime();
    }
    return ret;
}

Codec::STATUS Connection::decode(HttpMsg* msg) {
    CodecHttp* codec = dynamic_cast<CodecHttp*>(m_codec);
    if (codec == nullptr) {
        return Codec::STATUS::ERR;
    }
    return codec->decode(m_recv_buf, *msg);
}

}  // namespace kim