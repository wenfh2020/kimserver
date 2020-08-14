#include "cmd.h"

#include "util/util.h"

namespace kim {

Cmd::Cmd(Log* logger, INet* net, uint64_t mid, uint64_t id, const std::string& name)
    : Base(id, logger, net, name), m_module_id(mid) {
    LOG_DEBUG("%s", get_name());
    set_keep_alive(CMD_TIMEOUT_VAL);
    set_max_timeout_cnt(CMD_MAX_TIMEOUT_CNT);
}

Cmd::~Cmd() {
    LOG_DEBUG("destory %s", get_name());
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

    if (!m_net->send_to(m_req->get_conn(), msg)) {
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

Cmd::STATUS Cmd::response_http(int err, const std::string& errstr, int status_code) {
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

Cmd::STATUS Cmd::redis_send_to(const std::string& host,
                               int port, const std::vector<std::string>& rds_cmds) {
    LOG_DEBUG("redis send to host: %s, port: %d", host.c_str(), port);
    if (host.empty() || port == 0) {
        LOG_ERROR("invalid addr info: host: %s, port: %d", host.c_str(), port);
        return Cmd::STATUS::ERROR;
    }

    E_RDS_STATUS status = m_net->redis_send_to(host, port, this, rds_cmds);
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
    return execute_steps(err, data);
}

Cmd::STATUS Cmd::execute(std::shared_ptr<Request> req) {
    return execute_steps(ERR_OK, nullptr);
}

Cmd::STATUS Cmd::execute_steps(int err, void* data) {
    return Cmd::STATUS::OK;
}

Cmd::STATUS Cmd::on_callback(int err, void* data) {
    return execute_steps(err, data);
}

Cmd::STATUS Cmd::on_timeout() {
    LOG_DEBUG("time out!");
    if (++m_cur_timeout_cnt < get_max_timeout_cnt()) {
        return Cmd::STATUS::RUNNING;
    }
    return response_http(ERR_EXEC_CMD_TIMEUOT, "request handle timeout!");
}

};  // namespace kim