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
    bool send_req_connect_to_worker(Connection* c);

    /* zk nodes. */
    bool send_children_add_zk_node(const zk_node& node);
    bool send_children_del_zk_node(const std::string& zk_path);
    bool send_children_reg_zk_node(const register_node& rn);
    bool send_parent_sync_zk_nodes(int version);

   public:
    /* communication between nodes.  */
    Cmd::STATUS process(const Request& req);
    void on_repeat_timer();

   private:
    Cmd::STATUS on_req_connect_to_worker(const Request& req);
    Cmd::STATUS on_rsp_connect_to_worker(const Request& req);
    Cmd::STATUS on_req_tell_worker(const Request& req);
    Cmd::STATUS on_rsp_tell_worker(const Request& req);

    /* parent and child. */

    /* zookeeper notice. */
    Cmd::STATUS on_req_add_zk_node(const Request& req);
    Cmd::STATUS on_rsp_add_zk_node(const Request& req);
    Cmd::STATUS on_req_del_zk_node(const Request& req);
    Cmd::STATUS on_rsp_del_zk_node(const Request& req);
    Cmd::STATUS on_req_reg_zk_node(const Request& req);
    Cmd::STATUS on_rsp_reg_zk_node(const Request& req);
    Cmd::STATUS on_req_sync_zk_nodes(const Request& req);
    Cmd::STATUS on_rsp_sync_zk_nodes(const Request& req);

   private:
    Cmd::STATUS process_worker_msg(const Request& req);
    Cmd::STATUS process_manager_msg(const Request& req);
    bool check_rsp(const Request& req);

   protected:
    INet* m_net = nullptr;
    Log* m_logger = nullptr;
    int m_timer_index = 0;
};

}  // namespace kim

#endif  //__KIM_SYS_CMD_H__