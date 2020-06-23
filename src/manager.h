#ifndef __MANAGER_H__
#define __MANAGER_H__

#include "events.h"
#include "node_info.h"
#include "server.h"
#include "util/CJsonObject.hpp"

namespace kim {

class Manager {
   public:
    Manager(Log* logger);
    virtual ~Manager();

    void run();
    bool init(const char* conf_path);
    void destory();

    static void on_terminated(struct ev_signal* watcher);
    static void on_child_terminated(struct ev_signal* watcher);

   private:
    bool init_logger();
    bool init_events();
    bool load_config(const char* path);
    void create_workers();
    bool add_chanel_event(int fd);

   private:
    Log* m_logger;
    CJsonObject m_json_conf;
    CJsonObject m_old_json_conf;
    node_info_t m_node_info;
    Events* m_events;
    std::map<int, worker_info_t*> m_pid_worker_info;
    std::map<int, int> m_chanel_fd_pid;
};

}  // namespace kim

#endif  //__MANAGER_H__