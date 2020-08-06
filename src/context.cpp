#include "context.h"

#include <unistd.h>

#include "util/util.h"

namespace kim {

Connection::Connection(Log* logger, int fd, uint64_t id)
    : m_id(id), m_logger(logger), m_fd(fd) {
}

Connection::~Connection() {
    LOG_DEBUG("~Connection()");
    SAFE_DELETE(m_recv_buf);
    SAFE_DELETE(m_send_buf);
    SAFE_DELETE(m_wait_send_buf);
    SAFE_DELETE(m_codec);
}

bool Connection::init(Codec::TYPE codec) {
    LOG_DEBUG("connection init fd: %d, codec type: %d", m_fd, (int)codec);

    try {
        m_recv_buf = new SocketBuffer;
        m_send_buf = new SocketBuffer;
        m_wait_send_buf = new SocketBuffer;
        switch (codec) {
            case Codec::TYPE::HTTP:
                m_codec = new CodecHttp(m_logger, codec);
                break;

            default:
                LOG_ERROR("invalid codec type: %d", (int)codec);
                break;
        }
    } catch (std::bad_alloc& e) {
        LOG_ERROR("%s", e.what());
        return false;
    }

    if (m_codec != nullptr) {
        set_active_time(time_now());
        m_codec->set_codec(codec);
    }

    return true;
}

bool Connection::is_http_codec() {
    if (m_codec == nullptr) {
        return false;
    }
    return (m_codec->get_codec() == Codec::TYPE::HTTP);
}

Codec::STATUS Connection::conn_read(HttpMsg& msg) {
    if (m_recv_buf == nullptr) {
        return Codec::STATUS::ERR;
    }

    int read_len;
    CodecHttp* codec;

    read_len = m_recv_buf->read_fd(m_fd, m_errno);
    LOG_DEBUG("read from fd: %d, data len: %d, readed data len: %d",
              m_fd, read_len, m_recv_buf->get_readable_len());
    if (read_len == 0) {
        LOG_DEBUG("connection closed! fd: %d!", m_fd);
        return Codec::STATUS::ERR;
    }

    if (read_len < 0 && errno != EAGAIN) {
        LOG_DEBUG("connection read error! fd: %d, err: %d, error: %s",
                  m_fd, errno, strerror(errno));
        return Codec::STATUS::ERR;
    }

    if (read_len > 0) {
        // recovery socket buffer.
        if (m_recv_buf->capacity() > SocketBuffer::BUFFER_MAX_READ &&
            m_recv_buf->get_readable_len() < m_recv_buf->capacity() / 2) {
            m_recv_buf->compact(m_recv_buf->get_readable_len() * 2);
        }
        m_active_time = time_now();
    }

    codec = dynamic_cast<CodecHttp*>(m_codec);
    if (codec == nullptr) {
        return Codec::STATUS::ERR;
    }
    return codec->decode(m_recv_buf, msg);
}

Codec::STATUS Connection::conn_write(const HttpMsg& msg) {
    if (is_closed()) {
        LOG_ERROR("conn is closed! fd: %d, seq: %llu", m_fd, m_id);
        return Codec::STATUS::ERR;
    }

    int write_len;
    CodecHttp* codec;
    Codec::STATUS status;

    codec = dynamic_cast<CodecHttp*>(m_codec);
    if (codec == nullptr) {
        return Codec::STATUS::ERR;
    }
    status = codec->encode(msg, m_send_buf);
    if (status != Codec::STATUS::OK) {
        LOG_DEBUG("encode http packed failed! fd: %d, seq: %llu, status: %d",
                  m_fd, m_id, (int)status);
        return status;
    }

    if (!m_send_buf->is_readable()) {
        LOG_DEBUG("no data to send! fd: %d, seq: %llu", m_fd, m_id);
        return status;
    }

    write_len = m_send_buf->write_fd(m_fd, m_errno);
    LOG_DEBUG("write to fd: %d, seq: %llu, write len: %d, readed data len: %d",
              m_fd, m_id, write_len, m_send_buf->get_readable_len());
    if (write_len >= 0) {
        // recovery socket buffer.
        if (m_send_buf->capacity() > SocketBuffer::BUFFER_MAX_READ &&
            m_send_buf->get_readable_len() < m_send_buf->capacity() / 2) {
            m_send_buf->compact(m_send_buf->get_readable_len() * 2);
        }
        m_active_time = time_now();
        return (m_send_buf->get_readable_len() > 0)
                   ? Codec::STATUS::PAUSE
                   : Codec::STATUS::OK;
    } else {
        if (m_errno == EAGAIN) {
            m_active_time = time_now();
            return Codec::STATUS::PAUSE;
        }

        LOG_ERROR("send data failed! fd: %d, seq: %llu, readable len: %d",
                  m_fd, m_id, m_send_buf->get_readable_len());
        return Codec::STATUS::ERR;
    }

    return status;
}

bool Connection::is_need_alive_check() {
    if (m_codec != nullptr) {
        if (m_codec->get_codec() == Codec::TYPE::HTTP) {
            return false;
        }
    }
    return true;
}

double Connection::get_keep_alive() {
    if (is_http_codec()) {  // http codec has it's own keep alive.
        CodecHttp* codec = dynamic_cast<CodecHttp*>(m_codec);
        if (codec != nullptr) {
            int keep_alive = codec->get_keep_alive();
            if (keep_alive >= 0) {
                return keep_alive;
            }
        }
    }
    return m_keep_alive;
}

}  // namespace kim