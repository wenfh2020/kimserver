#include "module_test.h"

#include "cmd_hello.h"
#include "cmd_test_auto_send.h"
#include "cmd_test_mysql.h"
#include "cmd_test_redis.h"
#include "cmd_test_timeout.h"

MUDULE_CREATE(MoudleTest)

namespace kim {

Cmd::STATUS MoudleTest::hello_world(std::shared_ptr<Request> req) {
    const HttpMsg* msg = req->http_msg();
    LOG_DEBUG("cmd hello, http path: %s, data: %s",
              msg->path().c_str(), msg->body().c_str());

    CJsonObject data;
    data.Add("id", "123");
    data.Add("name", "kimserver");

    CJsonObject obj;
    obj.Add("code", 0);
    obj.Add("msg", "ok");
    obj.Add("data", data);
    return response_http(req->conn(), obj.ToString());
}

Cmd::STATUS MoudleTest::test_proto(std::shared_ptr<Request> req) {
    MsgHead* head = req->msg_head();
    LOG_DEBUG("cmd: %d, seq: %u, len: %d",
              head->cmd(), head->seq(), head->len());

    MsgBody* body = req->msg_body();
    LOG_DEBUG("body len: %d, data: <%s>",
              body->ByteSizeLong(),
              body->SerializeAsString().c_str());

    MsgHead rsp_head;
    rsp_head.set_cmd(head->cmd() + 1);
    rsp_head.set_seq(head->seq());

    MsgBody rsp_body;
    rsp_body.set_data("good job!");
    rsp_head.set_len(rsp_body.ByteSizeLong());

    return net()->send_to(req->conn(), rsp_head, rsp_body)
               ? Cmd::STATUS::OK
               : Cmd::STATUS::ERROR;
}

Cmd::STATUS MoudleTest::test_auto_send(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdAutoSend);
}

Cmd::STATUS MoudleTest::test_cmd(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdHello);
}

Cmd::STATUS MoudleTest::test_redis(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdTestRedis);
}

Cmd::STATUS MoudleTest::test_mysql(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdTestMysql);
}

Cmd::STATUS MoudleTest::test_timeout(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdTestTimeout);
}

}  // namespace kim