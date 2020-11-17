#ifndef __CMD_TEST_AUTO_SEND_H__
#define __CMD_TEST_AUTO_SEND_H__

#include "cmd.h"

namespace kim {

class CmdAutoSend : public Cmd {
   public:
    enum E_STEP {
        STEP_PARSE_REQUEST = 0,
        STEP_AUTO_SEND,
        STEP_AUTO_SEND_CALLBACK,
    };

    CmdAutoSend(Log* logger, INet* net,
                uint64_t mid, uint64_t id, const std::string& name = "")
        : Cmd(logger, net, mid, id, name) {
    }

   public:
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req) {
        switch (get_exec_step()) {
            case STEP_PARSE_REQUEST: {
                return Cmd::STATUS::OK;
                break;
            }
            default: {
                LOG_ERROR("invalid step");
                return Cmd::STATUS::ERROR;
                // return response_http(ERR_FAILED, "invalid step!");
            }
        }
        /* 
        
        MsgHead* head = req->msg_head();
    LOG_DEBUG("cmd: %d, seq: %d, len: %d",
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
         */
    }
};

}  // namespace kim

#endif  //__CMD_TEST_AUTO_SEND_H__