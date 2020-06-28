#ifndef __SERVER__
#define __SERVER__

#include <iostream>

namespace kim {

#define MAX_PATH 256

#define EXIT_SUCCESS 0
#define EXIT_FAIL -1
#define EXIT_CHILD -2
#define EXIT_CHILD_INIT_FAIL -3

#define SAFE_DELETE(x) \
    if (x != NULL) delete (x);

#define SAFE_FREE(x) \
    if (x != NULL) free(x);

// new m_logger obj, before using.
#define LOG_FORMAT(level, args...)                                           \
    if (m_logger != NULL) {                                                  \
        m_logger->log_data(__FILE__, __LINE__, __FUNCTION__, level, ##args); \
    }

#define LOG_EMERG(args...) LOG_FORMAT((kim::Log::LL_EMERG), ##args)
#define LOG_ALERT(args...) LOG_FORMAT((kim::Log::LL_ALERT), ##args)
#define LOG_CRIT(args...) LOG_FORMAT((kim::Log::LL_CRIT), ##args)
#define LOG_ERROR(args...) LOG_FORMAT((kim::Log::LL_ERR), ##args)
#define LOG_WARNING(args...) LOG_FORMAT((kim::Log::LL_WARNING), ##args)
#define LOG_NOTICE(args...) LOG_FORMAT((kim::Log::LL_NOTICE), ##args)
#define LOG_INFO(args...) LOG_FORMAT((kim::Log::LL_INFO), ##args)
#define LOG_DEBUG(args...) LOG_FORMAT((kim::Log::LL_DEBUG), ##args)

}  // namespace kim

#endif  //__SERVER__