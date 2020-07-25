#ifndef __MODULE_HELLO_H__
#define __MODULE_HELLO_H__

#include "../module.h"
#include "cmd_hello.h"
#include "util/json/CJsonObject.hpp"

namespace kim {
class MoudleCore : public Module {
    REGISTER_HANDLER(MoudleCore)

   public:
    void register_handle_func() {
        REGISTER_HANDLE_FUNC("/kim/im/user/", MoudleCore::func_cmd_hello);
    }

   private:
    Cmd::STATUS func_cmd_hello(std::shared_ptr<Request> req) {
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
};

}  // namespace kim

#endif  //__MODULE_HELLO_H__