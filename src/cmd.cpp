#include "cmd.h"

namespace kim {

Cmd::Cmd(Log* logger, ICallback* cb, uint64_t mid, uint64_t cid)
    : m_id(cid), m_module_id(mid), m_logger(logger), m_callback(cb) {
}

Cmd::~Cmd() {
}

Cmd::STATUS Cmd::response_http(const std::string& data, int status_code) {
    const HttpMsg* req_msg = m_req->get_http_msg();
    if (req_msg == nullptr) {
        LOG_ERROR("http msg is null! pls alloc!");
        return Cmd::STATUS::ERROR;
    }

    HttpMsg msg;
    msg.set_type(HTTP_RESPONSE);
    msg.set_status_code(status_code);
    msg.set_http_major(req_msg->http_major());
    msg.set_http_minor(req_msg->http_minor());
    msg.set_body(data);

    if (!m_callback->send_to(m_req->get_conn(), msg)) {
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

Cmd::STATUS Cmd::response_http(int err, const std::string& errstr,
                               int status_code) {
    CJsonObject obj;
    obj.Add("code", err);
    obj.Add("msg", errstr);
    return response_http(obj.ToString(), status_code);
}

Cmd::STATUS Cmd::response_http(int err, const std::string& errstr,
                               const CJsonObject& data, int status_code) {
    CJsonObject obj;
    obj.Add("code", err);
    obj.Add("msg", errstr);
    obj.Add("data", data);
    return response_http(obj.ToString(), status_code);
}

Cmd::STATUS Cmd::redis_send_to(const std::string& host, int port, const std::string& data) {
    LOG_DEBUG("redis send to host: %s, port: %d, data: %s",
              host.c_str(), port, data.c_str());
    if (host.empty() || port == 0) {
        LOG_ERROR("invalid addr info: host: %s, port: %d", host.c_str(), port);
        return Cmd::STATUS::ERROR;
    }
    cmd_index_data_t* d = m_callback->add_cmd_index_data(m_id, m_module_id);
    if (d == nullptr) {
        LOG_ERROR("add cmd index data failed! cmd id: %llu, module id: %llu",
                  m_id, m_module_id);
        return Cmd::STATUS::ERROR;
    }

    E_RDS_STATUS status = m_callback->redis_send_to(host, port, data, d);
    if (status == E_RDS_STATUS::OK) {
        set_next_step();
        return Cmd::STATUS::RUNNING;
    } else if (status == E_RDS_STATUS::WAITING) {
        return Cmd::STATUS::RUNNING;
    } else {
        return Cmd::STATUS::ERROR;
    }
}

Cmd::STATUS Cmd::execute_next_step(int err, void* data) {
    set_next_step();
    return execute(err, data);
}

Cmd::STATUS Cmd::execute(int err, void* data) {
    return Cmd::STATUS::OK;
}

};  // namespace kim