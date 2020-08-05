#ifndef __MODULE_TEST_H__
#define __MODULE_TEST_H__

#include "../module.h"
#include "cmd_hello.h"
#include "cmd_test_redis.h"
#include "cmd_test_timeout.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class MoudleTest : public Module {
    REGISTER_HANDLER(MoudleTest)

   public:
    void register_handle_func() {
        REGISTER_FUNC("/kim/test_cmd/", MoudleTest::func_test_cmd);
        REGISTER_FUNC("/kim/helloworld/", MoudleTest::func_hello_world);
        REGISTER_FUNC("/kim/test_redis/", MoudleTest::func_test_redis);
        REGISTER_FUNC("/kim/test_timeout/", MoudleTest::func_test_timeout);
    }

   private:
    Cmd::STATUS func_test_cmd(std::shared_ptr<Request> req);
    Cmd::STATUS func_hello_world(std::shared_ptr<Request> req);
    Cmd::STATUS func_test_redis(std::shared_ptr<Request> req);
    Cmd::STATUS func_test_timeout(std::shared_ptr<Request> req);
};

}  // namespace kim

#endif  //__MODULE_TEST_H__