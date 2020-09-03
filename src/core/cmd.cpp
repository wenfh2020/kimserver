#include "cmd.h"

#include "util/util.h"

namespace kim {

Cmd::Cmd(Log* logger, INet* net, uint64_t mid, uint64_t id, const std::string& name)
    : Base(id, logger, net, name), m_module_id(mid) {
    set_keep_alive(CMD_TIMEOUT_VAL);
    set_max_timeout_cnt(CMD_MAX_TIMEOUT_CNT);
    set_active_time(get_net()->now());
}

Cmd::~Cmd() {}

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

Cmd::STATUS Cmd::redis_send_to(const char* node, const std::vector<std::string>& rds_cmds) {
    if (node == nullptr) {
        LOG_ERROR("invalid addr info, node: %s.", node);
        return Cmd::STATUS::ERROR;
    }

    LOG_DEBUG("redis send to node: %s", node);

    return m_net->redis_send_to(node, this, rds_cmds)
               ? Cmd::STATUS::RUNNING
               : Cmd::STATUS::ERROR;
}

Cmd::STATUS Cmd::db_exec(const char* node, const char* sql) {
    if (node == nullptr || sql == nullptr) {
        LOG_ERROR("invalid param!");
        return Cmd::STATUS::ERROR;
    }
    LOG_DEBUG("db exec, node: %s, sql: %s", node, sql);
    if (!get_net()->db_exec(node, sql, this)) {
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
    if (!get_net()->db_query(node, sql, this)) {
        LOG_ERROR("database query failed! node: %s, sql: %s", node, sql);
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::RUNNING;
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

Cmd::STATUS Cmd::execute_cur_step(int step, int err, void* data) {
    m_step = step;
    return execute_steps(err, data);
}

Cmd::STATUS Cmd::on_callback(int err, void* data) {
    return execute_steps(err, data);
}

Cmd::STATUS Cmd::on_timeout() {
    LOG_DEBUG("time out!");
    if (++m_cur_timeout_cnt < get_max_timeout_cnt()) {
        return Cmd::STATUS::RUNNING;
    }
    return Cmd::STATUS::ERROR;
    // return response_http(ERR_EXEC_CMD_TIMEUOT, "request handle timeout!");
}

}  // namespace kim