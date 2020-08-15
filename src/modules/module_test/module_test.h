#ifndef __MODULE_TEST_H__
#define __MODULE_TEST_H__

#include "cmd_hello.h"
#include "cmd_test_redis.h"
#include "cmd_test_timeout.h"
#include "module.h"
#include "protocol.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class MoudleTest : public Module {
    REGISTER_HANDLER(MoudleTest)

   public:
    void register_handle_func() {
        // protobuf
        REGISTER_FUNC(KP_TEST_PROTO, MoudleTest::func_test_proto);

        // http
        REGISTER_HTTP_FUNC("/kim/test_cmd/", MoudleTest::func_test_cmd);
        REGISTER_HTTP_FUNC("/kim/helloworld/", MoudleTest::func_hello_world);
        REGISTER_HTTP_FUNC("/kim/test_redis/", MoudleTest::func_test_redis);
        REGISTER_HTTP_FUNC("/kim/test_timeout/", MoudleTest::func_test_timeout);
    }

   private:
    // protobuf.
    Cmd::STATUS func_test_proto(std::shared_ptr<Request> req);

    // http.
    Cmd::STATUS func_test_cmd(std::shared_ptr<Request> req);
    Cmd::STATUS func_hello_world(std::shared_ptr<Request> req);
    Cmd::STATUS func_test_redis(std::shared_ptr<Request> req);
    Cmd::STATUS func_test_timeout(std::shared_ptr<Request> req);
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