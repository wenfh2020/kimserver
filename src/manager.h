#ifndef __MANAGER_H__
#define __MANAGER_H__

#include "callback.h"
#include "network.h"
#include "node_info.h"
#include "server.h"
#include "util/json/CJsonObject.hpp"
#include "worker_data_mgr.h"

namespace kim {

class Manager : public ICallback {
   public:
    Manager();
    virtual ~Manager();

    bool init(const char* conf_path);
    void destory();
    void run();

    // signal callback
    virtual void on_terminated(ev_signal* s) override;
    virtual void on_child_terminated(ev_signal* s) override;
    virtual void on_repeat_timer(void* privdata) override;

   private:
    bool load_logger();
    bool load_network();
    bool load_config(const char* path);

    void create_workers();                 // fork processes.
    bool create_worker(int worker_index);  // for one process.
    bool restart_worker(pid_t pid);        // fork a new process, when old terminated!
    void restart_workers();
    std::string get_worker_name(int index);

   private:
    Log* m_logger = nullptr;           // logger.
    Network* m_net = nullptr;          // net work.
    CJsonObject m_conf, m_old_conf;    // config.
    NodeInfo m_node_info;              // cluster node.
    WorkerDataMgr m_worker_data_mgr;   // worker node data manager.
    std::list<int> m_restart_workers;  // workers waiting to restart. restore worker's index.
};

}  // namespace kim

#endif  //__MANAGER_H__