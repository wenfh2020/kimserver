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

Cmd::STATUS Cmd::response_http(const std::string& data, int status_code) {
    HttpMsg msg;
    msg.set_type(HTTP_RESPONSE);
    msg.set_status_code(status_code);
    msg.set_http_major(m_req->get_http_msg()->http_major());
    msg.set_http_minor(m_req->get_http_msg()->http_minor());
    msg.set_body(data);

    if (!m_net->send_to(m_req->get_conn(), msg)) {
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

};  // namespace kim