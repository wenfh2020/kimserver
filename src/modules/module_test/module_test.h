#ifndef __MODULE_TEST_H__
#define __MODULE_TEST_H__

#include "module.h"
#include "protocol.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class MoudleTest : public Module {
    REGISTER_HANDLER(MoudleTest)

   public:
    void register_handle_func() {
        // http
        HANDLE_HTTP_FUNC("/kim/test_cmd/", MoudleTest::test_cmd);
        HANDLE_HTTP_FUNC("/kim/test_redis/", MoudleTest::test_redis);
        HANDLE_HTTP_FUNC("/kim/test_mysql/", MoudleTest::test_mysql);
        HANDLE_HTTP_FUNC("/kim/helloworld/", MoudleTest::hello_world);
        HANDLE_HTTP_FUNC("/kim/test_timeout/", MoudleTest::test_timeout);

        // protobuf
        HANDLE_PROTO_FUNC(KP_REQ_TEST_PROTO, MoudleTest::test_proto);
        HANDLE_PROTO_FUNC(KP_REQ_TEST_AUTO_SEND, MoudleTest::test_auto_send);
    }

   private:
    // protobuf.
    Cmd::STATUS test_proto(std::shared_ptr<Request> req);
    Cmd::STATUS test_auto_send(std::shared_ptr<Request> req);

    // http.
    Cmd::STATUS test_cmd(std::shared_ptr<Request> req);
    Cmd::STATUS test_redis(std::shared_ptr<Request> req);
    Cmd::STATUS test_mysql(std::shared_ptr<Request> req);
    Cmd::STATUS hello_world(std::shared_ptr<Request> req);
    Cmd::STATUS test_timeout(std::shared_ptr<Request> req);
};

}  // namespace kim

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
kim::Module* create() {
    return (new kim::MoudleTest());
}
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  //__MODULE_TEST_H__