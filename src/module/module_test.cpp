#include "module_test.h"

#include "cmd_hello.h"
#include "cmd_test_redis.h"

namespace kim {

Cmd::STATUS MoudleTest::func_test_cmd(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdHello);
}

Cmd::STATUS MoudleTest::func_test_redis(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdTestRedis);
}

Cmd::STATUS MoudleTest::func_hello_world(std::shared_ptr<Request> req) {
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
    return response_http(req->get_conn(), obj.ToString());
}

}  // namespace kim