#ifndef __CMD_HELLO_H__
#define __CMD_HELLO_H__

#include "../cmd.h"

namespace kim {

class CmdHello : public Cmd {
   public:
    CmdHello(Log* logger) : Cmd(logger) {}
    virtual ~CmdHello() {
        std::cout << "delete cmd hello" << std::endl;
    }

    virtual Cmd::STATUS call_back(Request* req) {
        LOG_DEBUG("cmd hello, http path");
        const HttpMsg* msg = req->get_http_msg();
        if (msg == nullptr) {
            return Cmd::STATUS::ERROR;
        }
        LOG_DEBUG("cmd hello, http path: %s", msg->path().c_str());
        return Cmd::STATUS::OK;
    }
};

}  // namespace kim

#endif  //__CMD_HELLO_H__