#ifndef __SERVER__
#define __SERVER__

#include <sys/socket.h>
#include <sys/wait.h>

#include <fstream>
#include <iosfwd>
#include <iostream>
#include <sstream>

namespace kim {

#define MAX_PATH 256

#define EXIT_SUCCESS 0
#define EXIT_FAIL -1
#define EXIT_CHILD -2
#define EXIT_CHILD_INIT_FAIL -3
#define EXIT_FD_TRANSFER -4

#define SAFE_FREE(x)               \
    {                              \
        if (x != nullptr) free(x); \
        x = nullptr;               \
    }

#define SAFE_DELETE(x)              \
    {                               \
        if (x != nullptr) delete x; \
        x = nullptr;                \
    }

#define SAFE_ARRAY_DELETE(x)          \
    {                                 \
        if (x != nullptr) delete[] x; \
        x = nullptr;                  \
    }

// new m_logger obj, before using.
#define LOG_FORMAT(level, args...)                                           \
    if (m_logger != nullptr) {                                               \
        m_logger->log_data(__FILE__, __LINE__, __FUNCTION__, level, ##args); \
    }

#define LOG_EMERG(args...) LOG_FORMAT((Log::LL_EMERG), ##args)
#define LOG_ALERT(args...) LOG_FORMAT((Log::LL_ALERT), ##args)
#define LOG_CRIT(args...) LOG_FORMAT((Log::LL_CRIT), ##args)
#define LOG_ERROR(args...) LOG_FORMAT((Log::LL_ERR), ##args)
#define LOG_WARN(args...) LOG_FORMAT((Log::LL_WARNING), ##args)
#define LOG_NOTICE(args...) LOG_FORMAT((Log::LL_NOTICE), ##args)
#define LOG_INFO(args...) LOG_FORMAT((Log::LL_INFO), ##args)
#define LOG_DEBUG(args...) LOG_FORMAT((Log::LL_DEBUG), ##args)

enum class E_RDS_STATUS {
    OK = 0,
    WAITING = 2,
    ERROR = 3,
};

#define IO_TIMER_VAL 15

}  // namespace kim

#endif  //__SERVER__