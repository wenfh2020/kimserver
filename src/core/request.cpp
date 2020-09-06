#include "request.h"

namespace kim {

Request::Request(std::shared_ptr<Connection> c, const HttpMsg& msg)
    : m_conn(c), m_is_http(true) {
    set_http_msg_info(msg);
}

Request::Request(std::shared_ptr<Connection> c, const MsgHead& head, const MsgBody& body)
    : m_conn(c), m_is_http(false) {
    set_msg_info(head, body);
}

Request::~Request() {
    SAFE_DELETE(m_msg_head);
    SAFE_DELETE(m_msg_body);
    SAFE_DELETE(m_http_msg);
}

void Request::set_http_msg_info(const HttpMsg& msg) {
    if (m_http_msg == nullptr) {
        m_http_msg = new HttpMsg;
    }
    *m_http_msg = msg;
}

void Request::set_msg_info(const MsgHead& head, const MsgBody& body) {
    if (m_msg_head == nullptr) {
        m_msg_head = new MsgHead;
    }
    *m_msg_head = head;

    if (m_msg_body == nullptr) {
        m_msg_body = new MsgBody;
    }
    *m_msg_body = body;
}

HttpMsg* Request::get_http_msg() {
    return m_http_msg;
}

MsgHead* Request::get_msg_head() {
    return m_msg_head;
}

MsgBody* Request::get_msg_body() {
    return m_msg_body;
}

};  // namespace kim