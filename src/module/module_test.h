#ifndef __MODULE_HELLO_H__
#define __MODULE_HELLO_H__

#include "../module.h"
#include "cmd_hello.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class MoudleTest : public Module {
    REGISTER_HANDLER(MoudleTest)

   public:
    void register_handle_func() {
        REGISTER_HANDLE_FUNC("/kim/test/", MoudleTest::func_test_cmd);
        REGISTER_HANDLE_FUNC("/kim/helloworld/", MoudleTest::func_hello_world);
    }

   private:
    Cmd::STATUS func_test_cmd(std::shared_ptr<Request> req);
    Cmd::STATUS func_hello_world(std::shared_ptr<Request> req);
};

}  // namespace kim

#endif  //__MODULE_HELLO_H__