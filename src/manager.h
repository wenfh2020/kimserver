#ifndef __MANAGER_H__
#define __MANAGER_H__

#include "events.h"
#include "network.h"
#include "node_info.h"
#include "server.h"
#include "util/CJsonObject.hpp"

namespace kim {

class Manager {
   public:
    Manager();
    virtual ~Manager();

    void run();
    bool init(const char* conf_path, kim::Log* l);
    void destory();

    static void on_terminated(struct ev_signal* watcher);
    static void on_child_terminated(struct ev_signal* watcher);

   private:
    bool init_logger();
    bool init_events();
    bool init_network();
    void set_logger(Log* log) { m_logger = log; }
    bool load_config(const char* path);
    void create_workers();
    void close_listen_sockets();

   private:
    Log* m_logger;
    util::CJsonObject m_json_conf;
    util::CJsonObject m_old_json_conf;
    node_info_t m_node_info;
    Events* m_events;
    Network* m_network;
    std::list<int> m_fds;
};

}  // namespace kim

#endif  //__MANAGER_H__