#ifndef __KIM_SYS_CMD_H__
#define __KIM_SYS_CMD_H__

#include "cmd.h"
#include "nodes.h"
#include "protobuf/sys/nodes.pb.h"
#include "util/log.h"
#include "worker_data_mgr.h"

namespace kim {

class SysCmd {
   public:
    SysCmd(Log* logger, INet* net);
    virtual ~SysCmd() {}

    bool init();

   public:
    bool send_req_connect_to_worker(Connection* c, int worker_index);
    bool send_req_add_zk_node(const zk_node& node);
    bool send_req_del_zk_node(const std::string& zk_path);

   public:
    /* communication between nodes.  */
    Cmd::STATUS process(Request& req);

   private:
    Cmd::STATUS on_req_connect_to_worker(Request& req);
    Cmd::STATUS on_rsp_connect_to_worker(Request& req);
    Cmd::STATUS on_req_tell_worker(Request& req);
    Cmd::STATUS on_rsp_tell_worker(Request& req);

    /* parent and child. */

    /* zookeeper notice. */
    Cmd::STATUS on_req_add_zk_node(Request& req);
    Cmd::STATUS on_rsp_add_zk_node(Request& req);
    Cmd::STATUS on_req_del_zk_node(Request& req);
    Cmd::STATUS on_rsp_del_zk_node(Request& req);

   private:
    Cmd::STATUS process_worker_msg(Request& req);
    Cmd::STATUS process_manager_msg(Request& req);
    bool check_rsp(Request& req);
    bool send_req(Request& req, const std::string& data);
    bool send_req(Connection* c, int cmd, const std::string& data);
    bool send_ack(Request& req, int err, const std::string& errstr, const std::string& data = "");

   protected:
    INet* m_net = nullptr;
    Log* m_logger = nullptr;
};

}  // namespace kim

#endif  //__KIM_SYS_CMD_H__