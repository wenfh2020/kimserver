#ifndef __CMD_TEST_TIMEOUT_H__
#define __CMD_TEST_TIMEOUT_H__

#include "cmd.h"

namespace kim {

class CmdTestTimeout : public Cmd {
   public:
    CmdTestTimeout(Log* logger, INet* net, uint64_t mid, uint64_t id, const std::string& name = "")
        : Cmd(logger, net, mid, id, name) {
    }

   public:
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req) {
        const HttpMsg* msg = m_req->get_http_msg();
        if (msg == nullptr) {
            return Cmd::STATUS::ERROR;
        }
        LOG_DEBUG("cmd hello, http path: %s, data: %s",
                  msg->path().c_str(), msg->body().c_str());
        return Cmd::STATUS::RUNNING;
    }
};

}  // namespace kim

#endif  //__CMD_TEST_TIMEOUT_H__