#include "module.h"

#include "../server.h"

namespace kim {

#define CMD_TIMEOUT 1.0

Module::~Module() {
    for (const auto& it : m_cmds) {
        delete it.second;
    }
    m_cmds.clear();
}

bool Module::init(Log* logger, INet* net) {
    m_net = net;
    m_logger = logger;
    return true;
}

Cmd::STATUS Module::execute_cmd(Cmd* cmd, std::shared_ptr<Request> req) {
    Cmd::STATUS status = cmd->execute(req);
    if (status == Cmd::STATUS::RUNNING) {
        auto it = m_cmds.insert({cmd->get_id(), cmd});
        if (!it.second) {
            LOG_ERROR("cmd duplicate in m_cmds!");
            return Cmd::STATUS::ERROR;
        }
        ev_timer* w = m_net->add_cmd_timer(CMD_TIMEOUT, cmd->get_timer(), cmd);
        if (w == nullptr) {
            LOG_ERROR("module add cmd(%s) timer failed!", cmd->get_cmd_name().c_str());
            return Cmd::STATUS::ERROR;
        }
        cmd->set_timer(w);
    }
    return status;
}

bool Module::del_cmd(Cmd* cmd) {
    auto it = m_cmds.find(cmd->get_id());
    if (it == m_cmds.end()) {
        return false;
    }

    m_cmds.erase(it);
    if (cmd->get_timer() != nullptr) {
        m_net->del_cmd_timer(cmd->get_timer());
        cmd->set_timer(nullptr);
        LOG_DEBUG("del timer!")
    }
    LOG_DEBUG("delete cmd index data!");
    m_net->del_cmd_index_data(cmd->get_id());
    SAFE_DELETE(cmd);
    return true;
}

Cmd::STATUS Module::on_timeout(Cmd* cmd) {
    Cmd::STATUS status = cmd->on_timeout();
    if (status != Cmd::STATUS::RUNNING) {
        del_cmd(cmd);
    }
    return status;
}

Cmd::STATUS Module::on_callback(cmd_index_data_t* index, int err, void* data) {
    LOG_DEBUG("callback, module id: %llu, cmd id: %llu, err: %d",
              index->module_id, index->cmd_id, err);
    if (index == nullptr) {
        LOG_WARN("invalid timer for cmd!");
        return Cmd::STATUS::ERROR;
    }

    auto it = m_cmds.find(index->cmd_id);
    if (it == m_cmds.end() || it->second == nullptr) {
        LOG_WARN("find cmd failed! seq: %llu", index->cmd_id);
        return Cmd::STATUS::ERROR;
    }

    Cmd* cmd = it->second;
    Cmd::STATUS status = cmd->on_callback(err, data);
    if (status != Cmd::STATUS::RUNNING) {
        del_cmd(cmd);
    }
    return Cmd::STATUS::OK;
}

Cmd::STATUS Module::response_http(std::shared_ptr<Connection> c, _cstr& data, int status_code) {
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