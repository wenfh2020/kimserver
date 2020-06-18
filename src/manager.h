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
    bool init_logger();
    void set_logger(kim::Log* l) { m_logger = l; }
    bool load_config(const char* path);

   private:
    kim::Log* m_logger;
    std::string m_conf_path;
    std::string m_work_path;
    util::CJsonObject m_json_conf;
    util::CJsonObject m_old_json_conf;
    bool m_is_config_loaded;
    kim::node_info_t m_node_info;
    kim::Events m_events;
};

}  // namespace kim

#endif  //__MANAGER_H__