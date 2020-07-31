#include "cmd_hello.h"

#include "util/json/CJsonObject.hpp"

namespace kim {

CmdHello::CmdHello(Log* logger, INet* net, uint64_t mid, uint64_t cid)
    : Cmd(logger, net, mid, cid) {
}

CmdHello::~CmdHello() {
    LOG_DEBUG("delete cmd hello");
}

Cmd::STATUS CmdHello::execute(std::shared_ptr<Request> req) {
    const HttpMsg* msg = req->get_http_msg();
    if (msg == nullptr) {
        return Cmd::STATUS::ERROR;
    }

    LOG_DEBUG("cmd hello, http path: %s, data: %s",
              msg->path().c_str(), msg->body().c_str());

    CJsonObject data;
    data.Add("id", "123");
    data.Add("name", "kimserver");
    return response_http(0, "success", data);
}

Cmd::STATUS CmdHello::on_timeout() {
    LOG_DEBUG("time out!");
    return Cmd::STATUS::COMPLETED;
}

}  // namespace kim