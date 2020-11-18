#include "connection.h"

#include <unistd.h>

#include "util/util.h"

namespace kim {

Connection::Connection(Log* logger, int fd, uint64_t id)
    : m_id(id), m_logger(logger), m_fd(fd) {
    set_active_time(time_now());
}

Connection::~Connection() {
    SAFE_FREE(m_saddr);
    SAFE_DELETE(m_codec);
    SAFE_DELETE(m_recv_buf);
    SAFE_DELETE(m_send_buf);
    SAFE_DELETE(m_wait_send_buf);
}

bool Connection::init(Codec::TYPE codec) {
    LOG_TRACE("connection init fd: %d, codec type: %d", m_fd, (int)codec);

    try {
        switch (codec) {
            case Codec::TYPE::HTTP: {
                m_codec = new CodecHttp(m_logger, codec);
                break;
            }
            case Codec::TYPE::PROTOBUF: {
                m_codec = new CodecProto(m_logger, codec);
                break;
            }
            default: {
                LOG_ERROR("invalid codec type: %d", (int)codec);
                break;
            }
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
    return (m_codec->codec() == Codec::TYPE::HTTP);
}

bool Connection::conn_read() {
    if (is_invalid()) {
        LOG_ERROR("conn is closed! fd: %d, seq: %llu", m_fd, m_id);
        return false;
    }

    if (m_recv_buf == nullptr) {
        m_recv_buf = new SocketBuffer;
        if (m_recv_buf == nullptr) {
            return false;
        }
    }

    int read_len = m_recv_buf->read_fd(m_fd, m_errno);
    LOG_TRACE("read from fd: %d, data len: %d, readed data len: %d",
              m_fd, read_len, m_recv_buf->readable_len());
    if (read_len == 0) {
        LOG_TRACE("connection is closed! fd: %d!", m_fd);
        return false;
    }

    if (read_len < 0 && errno != EAGAIN) {
        LOG_DEBUG("connection read error! fd: %d, err: %d, error: %s",
                  m_fd, errno, strerror(errno));
        return false;
    }

    if (read_len > 0) {
        /* recovery socket buffer. */
        if (m_recv_buf->capacity() > SocketBuffer::BUFFER_MAX_READ &&
            m_recv_buf->readable_len() < m_recv_buf->capacity() / 2) {
            m_recv_buf->compact(m_recv_buf->readable_len() * 2);
        }
        m_active_time = time_now();
    }

    return true;
}

Codec::STATUS Connection::decode_http(HttpMsg& msg) {
    CodecHttp* codec = dynamic_cast<CodecHttp*>(m_codec);
    if (codec == nullptr) {
        return Codec::STATUS::ERR;
    }
    return codec->decode(m_recv_buf, msg);
}

Codec::STATUS Connection::decode_proto(MsgHead& head, MsgBody& body) {
    CodecProto* codec = dynamic_cast<CodecProto*>(m_codec);
    if (codec == nullptr) {
        return Codec::STATUS::ERR;
    }
    return codec->decode(m_recv_buf, head, body);
}

Codec::STATUS Connection::conn_write() {
    SocketBuffer* sbuf = m_send_buf;

    if (is_connected()) {
        /* pls send waiting buffer firstly. */
        if (m_wait_send_buf != nullptr && m_wait_send_buf->readable_len() > 0) {
            sbuf = m_wait_send_buf;
        }
    }

    if (sbuf == nullptr || !sbuf->is_readable()) {
        LOG_TRACE("no data to send! fd: %d, seq: %llu", m_fd, m_id);
        return Codec::STATUS::OK;
    }

    int write_len = sbuf->write_fd(m_fd, m_errno);
    if (write_len < 0) {
        if (m_errno == EAGAIN) {
            m_active_time = time_now();
            return Codec::STATUS::PAUSE;
        } else {
            LOG_ERROR("send data failed! fd: %d, seq: %llu, readable len: %d",
                      m_fd, m_id, sbuf->readable_len());
            return Codec::STATUS::ERR;
        }
    }

    LOG_TRACE("send to fd: %d, conn id: %llu, write len: %d, readed data len: %d",
              m_fd, m_id, write_len, sbuf->readable_len());

    /* recovery socket buffer. */
    if (sbuf->capacity() > SocketBuffer::BUFFER_MAX_READ &&
        sbuf->readable_len() < sbuf->capacity() / 2) {
        sbuf->compact(sbuf->readable_len() * 2);
    }
    m_active_time = time_now();
    return (sbuf->readable_len() > 0) ? Codec::STATUS::PAUSE : Codec::STATUS::OK;
}

Codec::STATUS Connection::conn_read(MsgHead& head, MsgBody& body) {
    if (!conn_read()) {
        return Codec::STATUS::ERR;
    }
    return decode_proto(head, body);
}

Codec::STATUS Connection::fetch_data(MsgHead& head, MsgBody& body) {
    if (is_invalid()) {
        LOG_ERROR("conn is closed! fd: %d, seq: %llu", m_fd, m_id);
        return Codec::STATUS::ERR;
    }
    return decode_proto(head, body);
}

Codec::STATUS Connection::conn_write(const MsgHead& head, const MsgBody& body) {
    return conn_write(head, body, &m_send_buf);
}

Codec::STATUS Connection::conn_write_waiting(const MsgHead& head, const MsgBody& body) {
    return conn_write(head, body, &m_wait_send_buf, false);
}

Codec::STATUS Connection::conn_write(
    const MsgHead& head, const MsgBody& body, SocketBuffer** buf, bool is_send) {
    if (is_invalid()) {
        LOG_ERROR("conn is invalid! fd: %d, seq: %llu", m_fd, m_id);
        return Codec::STATUS::ERR;
    }

    CodecProto* codec = dynamic_cast<CodecProto*>(m_codec);
    if (codec == nullptr) {
        return Codec::STATUS::ERR;
    }

    if (*buf == nullptr) {
        *buf = new SocketBuffer;
        if (*buf == nullptr) {
            LOG_ERROR("alloc send buf failed!");
            return Codec::STATUS::ERR;
        }
    }

    Codec::STATUS status = codec->encode(head, body, *buf);
    if (status != Codec::STATUS::OK) {
        LOG_DEBUG("encode packed failed! fd: %d, seq: %llu, status: %d",
                  m_fd, m_id, (int)status);
        return status;
    }

    return is_send ? conn_write() : status;
}

Codec::STATUS Connection::conn_read(HttpMsg& msg) {
    if (!conn_read()) {
        return Codec::STATUS::ERR;
    }
    return decode_http(msg);
}

Codec::STATUS Connection::fetch_data(HttpMsg& msg) {
    if (is_invalid()) {
        LOG_ERROR("conn is closed! fd: %d, seq: %llu", m_fd, m_id);
        return Codec::STATUS::ERR;
    }
    return decode_http(msg);
}

Codec::STATUS Connection::conn_write(const HttpMsg& msg) {
    return conn_write(msg, &m_send_buf);
}

Codec::STATUS Connection::conn_write(const HttpMsg& msg, SocketBuffer** buf) {
    if (is_invalid()) {
        LOG_ERROR("conn is closed! fd: %d, seq: %llu", m_fd, m_id);
        return Codec::STATUS::ERR;
    }

    CodecHttp* codec = dynamic_cast<CodecHttp*>(m_codec);
    if (codec == nullptr) {
        return Codec::STATUS::ERR;
    }

    if (*buf == nullptr) {
        *buf = new SocketBuffer;
        if (*buf == nullptr) {
            LOG_ERROR("alloc send buf failed!");
            return Codec::STATUS::ERR;
        }
    }

    Codec::STATUS status = codec->encode(msg, *buf);
    if (status != Codec::STATUS::OK) {
        LOG_DEBUG("encode http packed failed! fd: %d, seq: %llu, status: %d",
                  m_fd, m_id, (int)status);
        return status;
    }

    return conn_write();
}

bool Connection::is_need_alive_check() {
    if (m_codec != nullptr) {
        if (m_codec->codec() == Codec::TYPE::HTTP) {
            return false;
        }
    }
    return true;
}

double Connection::keep_alive() {
    if (is_http_codec()) {
        /* http codec has it's own keep alive. */
        CodecHttp* codec = dynamic_cast<CodecHttp*>(m_codec);
        if (codec != nullptr) {
            int keep_alive = codec->keep_alive();
            if (keep_alive >= 0) {
                return keep_alive;
            }
        }
    }
    return m_keep_alive;
}

void Connection::set_addr_info(struct sockaddr* saddr, size_t saddr_len) {
    SAFE_FREE(m_saddr);
    m_saddr = (struct sockaddr*)malloc(saddr_len);
    memcpy(m_saddr, saddr, saddr_len);
    m_saddr_len = saddr_len;
}

struct sockaddr* Connection::sockaddr() {
    return m_saddr;
}

}  // namespace kim