#include "request.h"

namespace kim {

Request::Request(std::shared_ptr<Connection> c, bool is_http)
    : m_conn(c), m_is_http(is_http) {
}

Request::~Request() {
    SAFE_DELETE(m_msg_head);
    SAFE_DELETE(m_msg_body);
    SAFE_DELETE(m_http_msg);
}

HttpMsg* Request::get_http_msg() {
    return m_http_msg;
}

HttpMsg* Request::http_msg_alloc() {
    if (m_http_msg == nullptr) {
        m_http_msg = new HttpMsg;
    }
    return m_http_msg;
}

MsgHead* Request::get_msg_head() {
    return m_msg_head;
}

MsgHead* Request::msg_head_alloc() {
    if (m_msg_head == nullptr) {
        m_msg_head = new MsgHead;
    }
    return m_msg_head;
}

MsgBody* Request::get_msg_body() {
    return m_msg_body;
}

MsgBody* Request::msg_body_alloc() {
    if (m_msg_body == nullptr) {
        m_msg_body = new MsgBody;
    }
    return m_msg_body;
}

};  // namespace kim