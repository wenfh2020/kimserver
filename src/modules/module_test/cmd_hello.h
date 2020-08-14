#ifndef __CMD_HELLO_H__
#define __CMD_HELLO_H__

#include "cmd.h"

namespace kim {

class CmdHello : public Cmd {
   public:
    CmdHello(Log* logger, INet* net,
             uint64_t mid, uint64_t id, const std::string& name = "")
        : Cmd(logger, net, mid, id, name) {
    }

   public:
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req) {
        const HttpMsg* msg = req->get_http_msg();
        if (msg == nullptr) {
            return Cmd::STATUS::ERROR;
        }
        LOG_DEBUG("cmd hello, http path: %s, data: %s",
                  msg->path().c_str(), msg->body().c_str());
        CJsonObject data;
        data.Add("id", "123");
        data.Add("name", "kimserver");
        return response_http(0, "success", data);
    }
};

}  // namespace kim

#endif  //__CMD_HELLO_H__