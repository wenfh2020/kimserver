#include "module_test.h"

#ifdef __cplusplus
extern "C" {
#endif
kim::Module* create() {
    return (new kim::MoudleTest());
}
#ifdef __cplusplus
}
#endif

namespace kim {

Cmd::STATUS MoudleTest::func_test_cmd(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdHello);
}

Cmd::STATUS MoudleTest::func_test_redis(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdTestRedis);
}

Cmd::STATUS MoudleTest::func_test_timeout(std::shared_ptr<Request> req) {
    HANDLE_CMD(CmdTestTimeout);
}

Cmd::STATUS MoudleTest::func_hello_world(std::shared_ptr<Request> req) {
    const HttpMsg* msg = req->get_http_msg();
    LOG_DEBUG("cmd hello, http path: %s, data: %s",
              msg->path().c_str(), msg->body().c_str());

    CJsonObject data;
    CJsonObject data2;
    data.Add("id", "123");
    data.Add("name", "kimserver");

    CJsonObject obj;
    obj.Add("code", 0);
    obj.Add("msg", "success");
    obj.Add("data", data);
    return response_http(req->get_conn(), obj.ToString());
}

Cmd::STATUS MoudleTest::func_test_proto(std::shared_ptr<Request> req) {
    MsgHead* head = req->get_msg_head();
    LOG_DEBUG("cmd: %d, seq: %d, len: %d",
              head->cmd(), head->seq(), head->len());

    MsgBody* body = req->get_msg_body();
    LOG_DEBUG("body len: %d, data: %s",
              body->ByteSizeLong(), body->SerializeAsString().c_str());

    MsgHead rsp_head;
    rsp_head.set_cmd(head->cmd() + 1);
    rsp_head.set_seq(head->seq());

    MsgBody rsp_body;
    rsp_body.set_data("good job!");
    rsp_head.set_len(rsp_body.ByteSizeLong());

    if (!m_net->send_to(req->get_conn(), rsp_head, rsp_body)) {
        return Cmd::STATUS::ERROR;
    } else {
        return Cmd::STATUS::OK;
    }
}

}  // namespace kim