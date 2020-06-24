#ifndef __MANAGER_H__
#define __MANAGER_H__

#include "events.h"
#include "node_info.h"
#include "server.h"
#include "signal_callback.h"
#include "util/CJsonObject.hpp"

namespace kim {

class Manager : public ISignalCallback {
   public:
    Manager(Log* logger);
    virtual ~Manager();

    void run();
    bool init(const char* conf_path);
    void destory();

    virtual void on_terminated(struct ev_signal* watcher);
    virtual void on_child_terminated(struct ev_signal* watcher);

   private:
    bool init_logger();
    bool init_events();
    bool load_config(const char* path);
    void create_workers();

   private:
    Log* m_logger;                // logger
    CJsonObject m_json_conf;      // current config.
    CJsonObject m_old_json_conf;  // old config
    node_info_t m_node_info;      // cluster node.
    Events* m_events;             // events handler.
    std::map<int, worker_info_t*> m_pid_worker_info;
    std::map<int, int> m_chanel_fd_pid;
};

}  // namespace kim

#endif  //__MANAGER_H__