#include "request.h"

namespace kim {

Request::Request(std::shared_ptr<Connection> c, MsgHead* head, MsgBody* body)
    : m_conn(c), m_msg_head(head), m_msg_body(body) {
}

Request::Request(std::shared_ptr<Connection> c, HttpMsg* msg) : m_conn(c), m_http_msg(msg) {
}

};  // namespace kim