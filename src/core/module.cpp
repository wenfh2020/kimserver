#include "module.h"

#include "server.h"
#include "util/util.h"

namespace kim {

Module::Module(Log* logger, INet* net, uint64_t id, const std::string& name)
    : Base(id, logger, net, name) {
}

Module::~Module() {
}

bool Module::init(Log* logger, INet* net, uint64_t id, const std::string& name) {
    set_id(id);
    set_net(net);
    set_name(name);
    set_logger(logger);
    register_handle_func();
    return true;
}

Cmd::STATUS Module::execute_cmd(Cmd* cmd, std::shared_ptr<Request> req) {
    Cmd::STATUS status = cmd->execute(req);
    if (status == Cmd::STATUS::RUNNING) {
        if (!get_net()->add_cmd(cmd)) {
            LOG_ERROR("add cmd duplicate, id: %llu!", cmd->get_id());
            return Cmd::STATUS::ERROR;
        }
        ev_timer* w = get_net()->add_cmd_timer(CMD_TIMEOUT_VAL, cmd->get_timer(), cmd);
        if (w == nullptr) {
            LOG_ERROR("module add cmd(%s) timer failed!", cmd->get_name());
            return Cmd::STATUS::ERROR;
        }
        cmd->set_timer(w);
    }
    return status;
}

Cmd::STATUS Module::on_timeout(Cmd* cmd) {
    int old;
    Cmd::STATUS status;

    old = cmd->get_cur_timeout_cnt();
    status = cmd->on_timeout();
    if (status != Cmd::STATUS::RUNNING) {
        get_net()->del_cmd(cmd);
    } else {
        if (cmd->get_req()->get_conn()->is_closed()) {
            LOG_DEBUG("connection is closed, stop timeout!");
            get_net()->del_cmd(cmd);
            return Cmd::STATUS::ERROR;
        }
        if (old == cmd->get_cur_timeout_cnt()) {
            cmd->refresh_cur_timeout_cnt();
        }
        if (cmd->get_cur_timeout_cnt() >= cmd->get_max_timeout_cnt()) {
            LOG_WARN("pls check timeout logic! %s", cmd->get_name());
            get_net()->del_cmd(cmd);
            return Cmd::STATUS::ERROR;
        }
    }
    return status;
}

Cmd::STATUS Module::on_callback(wait_cmd_info_t* index, int err, void* data) {
    LOG_DEBUG("callback, module id: %llu, cmd id: %llu, err: %d",
              index->module_id, index->cmd_id, err);
    if (index == nullptr) {
        LOG_WARN("invalid timer for cmd!");
        return Cmd::STATUS::ERROR;
    }

    Cmd* cmd = get_net()->get_cmd(index->cmd_id);
    if (cmd == nullptr) {
        LOG_WARN("find cmd failed! seq: %llu", index->cmd_id);
        return Cmd::STATUS::ERROR;
    }

    cmd->set_active_time(time_now());
    Cmd::STATUS status = cmd->on_callback(err, data);
    if (status != Cmd::STATUS::RUNNING) {
        get_net()->del_cmd(cmd);
    }

    return status;
}

Cmd::STATUS Module::response_http(std::shared_ptr<Connection> c, const std::string& data, int status_code) {
    HttpMsg msg;
    msg.set_type(HTTP_RESPONSE);
    msg.set_status_code(status_code);
    msg.set_http_major(1);
    msg.set_http_minor(1);
    msg.set_body(data);

    if (!get_net()->send_to(c, msg)) {
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

}  // namespace kim