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

    LOG_DEBUG("cmd hello, http path: %s", msg->path().c_str());

    HttpMsg m;
    m.set_type(HTTP_RESPONSE);
    m.set_status_code(200);
    m.set_http_major(msg->http_major());
    m.set_http_minor(msg->http_minor());

    CJsonObject obj;
    obj.Add("code", 0);
    obj.Add("msg", "error");
    m.set_body(obj.ToFormattedString());
    return response(m);
}
}  // namespace kim