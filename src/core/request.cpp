#include "request.h"

namespace kim {

Request::Request(const Request& req) : m_is_http(req.m_is_http) {
    m_fd_data = req.fd_data();
    if (m_is_http) {
        CHECK_SET(m_http_msg, HttpMsg, *req.http_msg());
    } else {
        CHECK_SET(m_msg_head, MsgHead, *req.msg_head());
        CHECK_SET(m_msg_body, MsgBody, *req.msg_body());
    }
}

Request::Request(const fd_t& f, bool is_http) : m_is_http(is_http) {
    m_fd_data = f;
    if (is_http) {
        CHECK_NEW(m_http_msg, HttpMsg);
    } else {
        CHECK_NEW(m_msg_head, MsgHead);
        CHECK_NEW(m_msg_body, MsgBody);
    }
}

Request::Request(const fd_t& f, const HttpMsg& msg) : m_is_http(true) {
    m_fd_data = f;
    CHECK_SET(m_http_msg, HttpMsg, msg);
}

Request::Request(const fd_t& f, const MsgHead& head, const MsgBody& body)
    : m_is_http(false) {
    m_fd_data = f;
    CHECK_SET(m_msg_head, MsgHead, head);
    CHECK_SET(m_msg_body, MsgBody, body);
}

Request::~Request() {
    SAFE_DELETE(m_msg_head);
    SAFE_DELETE(m_msg_body);
    SAFE_DELETE(m_http_msg);
}

};  // namespace kim