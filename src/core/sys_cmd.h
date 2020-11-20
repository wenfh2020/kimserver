#ifndef __KIM_SYS_CMD_H__
#define __KIM_SYS_CMD_H__

#include "cmd.h"
#include "nodes.h"
#include "util/log.h"
#include "worker_data_mgr.h"

namespace kim {

class SysCmd {
   public:
    SysCmd(Log* logger, INet* net, WorkerDataMgr* d);
    virtual ~SysCmd() {}

    bool init();

   public:
    bool send_req_connect_to_worker(Connection* c, int worker_index);

   public:
    /* communication between nodes.  */
    Cmd::STATUS process(Request& req);
    Cmd::STATUS on_req_connect_to_worker(Request& req);
    Cmd::STATUS on_rsp_connect_to_worker(Request& req);
    Cmd::STATUS on_req_tell_worker(Request& req);
    Cmd::STATUS on_rsp_tell_worker(Request& req);

    /* parent and child. */

    /* zookeeper notice. */
    Cmd::STATUS on_req_add_zk_node(Connection* c, const MsgHead& head, const MsgBody& body);

   private:
    bool send_req(Request& req, const std::string& data);
    bool send_req(Connection* c, int cmd, const std::string& data);
    bool send_ack(Request& req, int err, const std::string& errstr, const std::string& data = "");

   protected:
    INet* m_net = nullptr;
    Log* m_logger = nullptr;
    WorkerDataMgr* m_woker_data_mgr = nullptr;
};

}  // namespace kim

#endif  //__KIM_SYS_CMD_H__