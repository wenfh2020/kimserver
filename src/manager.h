#ifndef __MANAGER__
#define __MANAGER__

#include <iostream>

#include "log.h"
#include "server.h"
#include "util/CJsonObject.hpp"

namespace kim {

class Manager {
   public:
    Manager();
    virtual ~Manager();

    void run();
    bool init(const char* conf_path);
    bool init_logger();
    void set_logger(kim::Log* l) { m_logger = l; }
    bool load_config(const char* path);

   private:
    kim::Log* m_logger;
    std::string m_conf_path;
    std::string m_work_path;
    util::CJsonObject m_cur_json_conf;
    util::CJsonObject m_old_json_conf;
    bool m_is_config_loaded;
};

#define LOG_DETAIL(level, args...)                                           \
    if (m_logger != NULL) {                                                  \
        m_logger->log_data(__FILE__, __LINE__, __FUNCTION__, level, ##args); \
    }

#define LOG_INFO(args...) LOG_DETAIL((kim::Log::LL_INFO), ##args)

}  // namespace kim

#endif  //__MANAGER__