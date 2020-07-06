#ifndef __MANAGER_H__
#define __MANAGER_H__

#include "events_callback.h"
#include "network.h"
#include "node_info.h"
#include "server.h"
#include "util/CJsonObject.hpp"
#include "worker_data_mgr.h"

namespace kim {

class Manager : public ISignalCallBack {
   public:
    Manager();
    virtual ~Manager();

    bool init(const char* conf_path);
    void destory();
    void run();

    // signal callback
    void on_terminated(ev_signal* s) override;
    void on_child_terminated(ev_signal* s) override;

   private:
    bool load_logger();
    bool load_network();
    bool load_config(const char* path);

    void create_workers();                 // fork processes.
    bool create_worker(int worker_index);  // for one process.
    bool restart_worker(pid_t pid);        // fork a new process, when old terminated!

   private:
    std::shared_ptr<Log> m_logger = nullptr;  // logger.
    Network* m_net = nullptr;                 // net work.
    CJsonObject m_json_conf;                  // current config.
    CJsonObject m_old_json_conf;              // old config
    node_info_t m_node_info;                  // cluster node.
    WorkerDataMgr m_worker_data_mgr;          // worker node data manager.
};

}  // namespace kim

#endif  //__MANAGER_H__