#include "module.h"

#include "../server.h"

namespace kim {

Module::~Module() {
    for (const auto& it : m_cmds) {
        delete it.second;
    }
}

bool Module::init(Log* logger, INet* net) {
    m_net = net;
    m_logger = logger;
    return true;
}

Cmd::STATUS Module::response_http(
    std::shared_ptr<Connection> c, const std::string& data, int status_code) {
    HttpMsg msg;
    msg.set_type(HTTP_RESPONSE);
    msg.set_status_code(status_code);
    msg.set_http_major(1);
    msg.set_http_minor(1);
    msg.set_body(data);

    if (!m_net->send_to(c, msg)) {
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

}  // namespace kim