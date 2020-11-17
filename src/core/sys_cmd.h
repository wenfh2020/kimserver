#ifndef __KIM_SYS_CMD_H__
#define __KIM_SYS_CMD_H__

#include "cmd.h"
#include "util/log.h"
#include "worker_data_mgr.h"

namespace kim {

class SysCmd {
   public:
    SysCmd(Log* logger, INet* net, WorkerDataMgr* d);
    virtual ~SysCmd() {}

   public:
    bool send_req_connect_to_worker(std::shared_ptr<Connection>& c, int worker_index);

   public:
    Cmd::STATUS process(std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body);
    Cmd::STATUS req_connect_to_worker(std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body);
    Cmd::STATUS rsp_connect_to_worker(std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body);

    Cmd::STATUS req_tell_worker(std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body);
    Cmd::STATUS rsp_tell_worker(std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body);

   protected:
    INet* m_net = nullptr;
    Log* m_logger = nullptr;
    WorkerDataMgr* m_woker_data_mgr = nullptr;
};

}  // namespace kim

#endif  //__KIM_SYS_CMD_H__