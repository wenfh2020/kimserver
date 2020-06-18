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

#define LOG_EMERG(args...) LOG_DETAIL((kim::Log::LL_EMERG), ##args)
#define LOG_ALERT(args...) LOG_DETAIL((kim::Log::LL_ALERT), ##args)
#define LOG_CRIT(args...) LOG_DETAIL((kim::Log::LL_CRIT), ##args)
#define LOG_ERROR(args...) LOG_DETAIL((kim::Log::LL_ERR), ##args)
#define LOG_WARNING(args...) LOG_DETAIL((kim::Log::LL_WARNING), ##args)
#define LOG_NOTICE(args...) LOG_DETAIL((kim::Log::LL_NOTICE), ##args)
#define LOG_INFO(args...) LOG_DETAIL((kim::Log::LL_INFO), ##args)
#define LOG_DEBUG(args...) LOG_DETAIL((kim::Log::LL_DEBUG), ##args)

}  // namespace kim

#endif  //__MANAGER__