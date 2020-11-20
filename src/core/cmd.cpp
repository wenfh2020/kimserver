#include "cmd.h"

#include "util/util.h"

namespace kim {

Cmd::Cmd(Log* logger, INet* n, uint64_t mid, uint64_t id, const std::string& name)
    : Base(id, logger, n, name), m_module_id(mid) {
    set_keep_alive(CMD_TIMEOUT_VAL);
    set_max_timeout_cnt(CMD_MAX_TIMEOUT_CNT);
    set_active_time(net()->now());
}

Cmd::~Cmd() {}

bool Cmd::response_http(const std::string& data, int status_code) {
    const HttpMsg* req_msg = m_req->http_msg();
    if (req_msg == nullptr) {
        LOG_ERROR("http msg is null! pls alloc!");
        return false;
    }

    HttpMsg msg;
    msg.set_type(HTTP_RESPONSE);
    msg.set_status_code(status_code);
    msg.set_http_major(req_msg->http_major());
    msg.set_http_minor(req_msg->http_minor());
    msg.set_body(data);

    if (!m_net->send_to(m_req->conn(), msg)) {
        return false;
    }
    return true;
}

bool Cmd::response_http(int err, const std::string& errstr, int status_code) {
    CJsonObject obj;
    obj.Add("code", err);
    obj.Add("msg", errstr);
    return response_http(obj.ToString(), status_code);
}

bool Cmd::response_http(int err, const std::string& errstr,
                        const CJsonObject& data, int status_code) {
    CJsonObject obj;
    obj.Add("code", err);
    obj.Add("msg", errstr);
    obj.Add("data", data);
    return response_http(obj.ToString(), status_code);
}

bool Cmd::response_tcp(int err, const std::string& errstr, const std::string& data) {
    MsgHead head;
    MsgBody body;

    body.set_data(data);
    body.mutable_rsp_result()->set_code(err);
    body.mutable_rsp_result()->set_msg(errstr);

    head.set_cmd(m_req->msg_head()->cmd() + 1);
    head.set_seq(m_req->msg_head()->seq());
    head.set_len(body.ByteSizeLong());

    return net()->send_to(m_req->conn(), head, body);
}

Cmd::STATUS Cmd::redis_send_to(const char* node, const std::vector<std::string>& argv) {
    if (node == nullptr) {
        LOG_ERROR("invalid addr info, node: %s.", node);
        return Cmd::STATUS::ERROR;
    }

    return m_net->redis_send_to(node, this, argv)
               ? Cmd::STATUS::RUNNING
               : Cmd::STATUS::ERROR;
}

Cmd::STATUS Cmd::db_exec(const char* node, const char* sql) {
    if (node == nullptr || sql == nullptr) {
        LOG_ERROR("invalid param!");
        return Cmd::STATUS::ERROR;
    }
    LOG_DEBUG("db exec, node: %s, sql: %s", node, sql);
    if (!net()->db_exec(node, sql, this)) {
        LOG_ERROR("database exec failed! node: %s, sql: %s", node, sql);
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::RUNNING;
}

Cmd::STATUS Cmd::db_query(const char* node, const char* sql) {
    if (node == nullptr || sql == nullptr) {
        LOG_ERROR("invalid param!");
        return Cmd::STATUS::ERROR;
    }
    LOG_DEBUG("db exec, node: %s, sql: %s", node, sql);
    if (!net()->db_query(node, sql, this)) {
        LOG_ERROR("database query failed! node: %s, sql: %s", node, sql);
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::RUNNING;
}

Cmd::STATUS Cmd::execute_next_step(int err, void* data, int step) {
    set_next_step(step);
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
    LOG_TRACE("time out!");
    if (++m_cur_timeout_cnt < max_timeout_cnt()) {
        return Cmd::STATUS::RUNNING;
    }
    return Cmd::STATUS::ERROR;
    // return response_http(ERR_EXEC_CMD_TIMEUOT, "request handle timeout!");
}

}  // namespace kim