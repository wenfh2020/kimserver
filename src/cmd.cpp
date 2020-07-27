#include "cmd.h"

namespace kim {

Cmd::Cmd(Log* logger, ICallback* cb, uint64_t id)
    : m_id(id), m_logger(logger), m_callback(cb) {}

Cmd::STATUS Cmd::response_http(const std::string& data, int status_code) {
    HttpMsg msg;
    msg.set_type(HTTP_RESPONSE);
    msg.set_status_code(status_code);
    msg.set_http_major(m_req->get_http_msg()->http_major());
    msg.set_http_minor(m_req->get_http_msg()->http_minor());
    msg.set_body(data);

    if (!m_callback->send_to(m_req->get_conn(), msg)) {
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

};  // namespace kim