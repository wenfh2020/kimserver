#ifndef __LOG__
#define __LOG__

#include <stdarg.h>
#include <stdio.h>

#include <iostream>

namespace kim {

class Log {
   public:
    enum {
        LL_EMERG = 0, /* system is unusable */
        LL_ALERT,     /* action must be taken immediately */
        LL_CRIT,      /* critical conditions */
        LL_ERR,       /* error conditions */
        LL_WARNING,   /* warning conditions */
        LL_NOTICE,    /* normal but significant condition */
        LL_INFO,      /* informational */
        LL_DEBUG,     /* debug-level messages */
    };

    Log(const char* path = NULL);
    virtual ~Log() {}

   public:
    void set_level(int level) { m_level = level; }
    void log_data(const char* file_name, int file_line, const char* func_name, int level, const char* fmt, ...);

   private:
    void log_raw(const char* file_name, int file_line, const char* func_name, int level, const char* msg);

   private:
    int m_level;
    std::string m_path;
};

}  // namespace kim

#endif  //__LOG__
