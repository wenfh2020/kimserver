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

Cmd::STATUS Module::execute_cmd(Cmd* cmd, const Request& req) {
    ev_timer* w;
    Cmd::STATUS ret;

    ret = cmd->execute(req);
    if (ret != Cmd::STATUS::RUNNING) {
        SAFE_DELETE(cmd);
        return ret;
    }

    if (!net()->add_cmd(cmd)) {
        LOG_ERROR("add cmd duplicate, id: %llu!", cmd->id());
        SAFE_DELETE(cmd);
        return Cmd::STATUS::ERROR;
    }

    w = net()->add_cmd_timer(CMD_TIMEOUT_VAL, cmd->timer(), (void*)cmd);
    if (w == nullptr) {
        LOG_ERROR("module add cmd(%s) timer failed!", cmd->name());
        net()->del_cmd(cmd);
        return Cmd::STATUS::ERROR;
    }

    cmd->set_timer(w);
    return ret;
}

Cmd::STATUS Module::response_http(const fd_t& f, const std::string& data, int status_code) {
    HttpMsg msg;
    msg.set_type(HTTP_RESPONSE);
    msg.set_status_code(status_code);
    msg.set_http_major(1);
    msg.set_http_minor(1);
    msg.set_body(data);

    if (!net()->send_to(f, msg)) {
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

}  // namespace kim