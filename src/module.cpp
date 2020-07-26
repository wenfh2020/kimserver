#include "module.h"

#include "../server.h"

namespace kim {

Module::~Module() {
    for (const auto& it : m_cmds) {
        delete it.second;
    }
}

bool Module::init(Log* logger, ICallback* net) {
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
        cmd_timer_data_t* data = new cmd_timer_data_t(get_id(), cmd->get_id(), m_net);
        if (data == nullptr) {
            LOG_ERROR("alloc cmd_timer_data_t failed!");
            return Cmd::STATUS::ERROR;
        }
        ev_timer* w = m_net->add_cmd_timer(5.0, cmd->get_timer(), data);
        if (w == nullptr) {
            LOG_ERROR("module add cmd(%s) timer failed!", cmd->get_cmd_name().c_str());
            return Cmd::STATUS::ERROR;
        }
        cmd->set_timer(w);
    }
    return status;
}

Cmd::STATUS Module::on_timeout(cmd_timer_data_t* ctd) {
    if (ctd == nullptr) {
        LOG_WARN("invalid timer for cmd!");
        return Cmd::STATUS::ERROR;
    }

    auto it = m_cmds.find(ctd->m_cmd_id);
    if (it == m_cmds.end() || it->second == nullptr) {
        LOG_WARN("find cmd failed! seq: %llu", ctd->m_cmd_id);
        SAFE_DELETE(ctd);
        return Cmd::STATUS::ERROR;
    }

    Cmd* cmd = it->second;
    Cmd::STATUS status = cmd->on_timeout();
    if (status != Cmd::STATUS::RUNNING) {
        SAFE_DELETE(ctd);
        m_cmds.erase(it);
        m_net->del_cmd_timer(cmd->get_timer());
        cmd->set_timer(nullptr);
        SAFE_DELETE(cmd);
        LOG_DEBUG("del timer!")
    }
    return status;
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