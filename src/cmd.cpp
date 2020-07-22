#include "cmd.h"

namespace kim {

Cmd::~Cmd() {
}

void Cmd::init(Log* logger, INet* net) {
    m_net = net;
    m_logger = logger;
}

void Cmd::set_net(INet* net) {
    m_net = net;
}

Cmd::STATUS Cmd::response(const HttpMsg& msg) {
    if (!m_net->send_to(m_req->get_conn(), msg)) {
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

Cmd::STATUS Cmd::response_http(const std::string& data) {
    const HttpMsg* msg = m_req->get_http_msg();
    if (msg == nullptr) {
        return Cmd::STATUS::ERROR;
    }
    LOG_DEBUG("cmd hello, http path: %s", msg->path().c_str());

    HttpMsg m;
    m.set_type(HTTP_RESPONSE);
    m.set_status_code(200);
    m.set_http_major(msg->http_major());
    m.set_http_minor(msg->http_minor());

    m.set_body(data);
    if (!m_net->send_to(m_req->get_conn(), m)) {
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

};  // namespace kim