#ifndef __MANAGER_H__
#define __MANAGER_H__

#include "events_callback.h"
#include "network.h"
#include "node_info.h"
#include "server.h"
#include "util/CJsonObject.hpp"

namespace kim {

class Manager : public ISignalCallBack {
   public:
    Manager(Log* logger);
    virtual ~Manager();

    bool init(const char* conf_path);
    void destory();
    void run();

    // signal callback
    virtual void on_terminated(struct ev_signal* watcher);
    virtual void on_child_terminated(struct ev_signal* watcher);

   private:
    bool init_logger();
    bool create_network();
    bool load_config(const char* path);
    void create_workers();

   private:
    Log* m_logger;                // logger
    CJsonObject m_json_conf;      // current config.
    CJsonObject m_old_json_conf;  // old config
    node_info_t m_node_info;      // cluster node.
    // Events* m_events;             // events handler.
    Network* m_net;
    std::map<int, worker_info_t*> m_pid_worker_info;
    std::map<int, int> m_chanel_fd_pid;
};

}  // namespace kim

#endif  //__MANAGER_H__