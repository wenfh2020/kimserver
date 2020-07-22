#include "cmd_hello.h"

#include "util/json/CJsonObject.hpp"

namespace kim {

CmdHello::~CmdHello() {
    LOG_DEBUG("delete cmd hello");
}

Cmd::STATUS CmdHello::call_back(std::shared_ptr<Request> req) {
    const HttpMsg* msg = req->get_http_msg();
    if (msg == nullptr) {
        return Cmd::STATUS::ERROR;
    }

    LOG_DEBUG("cmd hello, http path: %s, data: %s",
              msg->path().c_str(), msg->body().c_str());

    CJsonObject data;
    data.Add("id", "123");
    data.Add("name", "kimserver");

    CJsonObject obj;
    obj.Add("code", 0);
    obj.Add("msg", "success");
    obj.Add("data", data);
    return response_http(obj.ToString());
}

}  // namespace kim