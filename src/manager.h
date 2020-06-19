#ifndef __MANAGER_H__
#define __MANAGER_H__

#include <iostream>

#include "events.h"
#include "log.h"
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

    static void on_terminated(struct ev_signal* watcher);
    static void on_child_terminated(struct ev_signal* watcher);

   private:
    bool init_logger();
    bool init_events();
    void set_logger(kim::Log* l) { m_logger = l; }
    bool load_config(const char* path);
    void create_workers();

   private:
    kim::Log* m_logger;
    util::CJsonObject m_json_conf;
    util::CJsonObject m_old_json_conf;
    kim::node_info_t m_node_info;
    kim::Events* m_events;
};

}  // namespace kim

#endif  //__MANAGER_H__