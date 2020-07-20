#include "request.h"

namespace kim {

Request::Request(std::shared_ptr<Connection> c) : m_conn(c) {
}

Request::~Request() {
    SAFE_DELETE(m_msg_head);
    SAFE_DELETE(m_msg_body);
    SAFE_DELETE(m_http_msg);
}

HttpMsg* Request::get_http_msg() {
    if (m_http_msg == nullptr) {
        m_http_msg = new HttpMsg;
    }
    return m_http_msg;
}

MsgHead* Request::get_msg_head() {
    if (m_msg_head == nullptr) {
        m_msg_head = new MsgHead;
    }
    return m_msg_head;
}

MsgBody* Request::get_msg_body() {
    if (m_msg_body == nullptr) {
        m_msg_body = new MsgBody;
    }
    return m_msg_body;
}

};  // namespace kim