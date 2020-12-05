#include "module_test.h"

#include "cmd_hello.h"
#include "cmd_test_auto_send.h"
#include "cmd_test_mysql.h"
#include "cmd_test_redis.h"
#include "cmd_test_timeout.h"

MUDULE_CREATE(MoudleTest)

namespace kim {

Cmd::STATUS MoudleTest::hello_world(const Request& req) {
    const HttpMsg* msg = req.http_msg();
    LOG_DEBUG("cmd hello, http path: %s, data: %s",
              msg->path().c_str(), msg->body().c_str());

    CJsonObject data;
    data.Add("id", "123");
    data.Add("name", "kimserver");

    CJsonObject obj;
    obj.Add("code", 0);
    obj.Add("msg", "ok");
    obj.Add("data", data);
    return response_http(req.fd_data(), obj.ToString());
}

Cmd::STATUS MoudleTest::test_proto(const Request& req) {
    const MsgHead* head = req.msg_head();
    LOG_DEBUG("cmd: %d, seq: %u, len: %d",
              head->cmd(), head->seq(), head->len());

    const MsgBody* body = req.msg_body();
    LOG_DEBUG("body data: <%s>", body->SerializeAsString().c_str());

    return net()->send_ack(req, ERR_OK, "ok", "good job!")
               ? Cmd::STATUS::OK
               : Cmd::STATUS::ERROR;
}

Cmd::STATUS MoudleTest::test_auto_send(const Request& req) {
    HANDLE_CMD(CmdAutoSend);
}

Cmd::STATUS MoudleTest::test_cmd(const Request& req) {
    HANDLE_CMD(CmdHello);
}

Cmd::STATUS MoudleTest::test_redis(const Request& req) {
    HANDLE_CMD(CmdTestRedis);
}

Cmd::STATUS MoudleTest::test_mysql(const Request& req) {
    HANDLE_CMD(CmdTestMysql);
}

Cmd::STATUS MoudleTest::test_timeout(const Request& req) {
    HANDLE_CMD(CmdTestTimeout);
}

}  // namespace kim