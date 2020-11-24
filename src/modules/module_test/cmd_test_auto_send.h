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
    Cmd::STATUS execute_steps(int err, void* data) {
        switch (get_exec_step()) {
            case STEP_PARSE_REQUEST: {
                MsgHead* head = req()->msg_head();
                LOG_DEBUG("cmd: %d, seq: %u, len: %d",
                          head->cmd(), head->seq(), head->len());
                return execute_next_step(err, data);
            }
            case STEP_AUTO_SEND: {
                MsgHead head(*req()->msg_head());
                head.set_seq(id());
                head.set_cmd(KP_REQ_TEST_PROTO);
                LOG_DEBUG("auto send head seq: %d, cmd id: %llu", head.seq(), id());
                /* send to other nodes. */
                if (!net()->send_to_node("gate", "hello", head, *m_req->msg_body())) {
                    return Cmd::STATUS::ERROR;
                }
                set_next_step();
                return Cmd::STATUS::RUNNING;
            }
            case STEP_AUTO_SEND_CALLBACK: {
                /* send back to client. */
                if (!response_tcp(ERR_OK, "OK", "good job.")) {
                    LOG_ERROR("send ack failed! fd: %d", m_req->fd());
                    return Cmd::STATUS::ERROR;
                }
                return Cmd::STATUS::COMPLETED;
            }
            default: {
                LOG_ERROR("invalid step");
                response_tcp(ERR_FAILED, "invalid step!");
                return Cmd::STATUS::ERROR;
            }
        }
    }
};

}  // namespace kim

#endif  //__CMD_TEST_AUTO_SEND_H__