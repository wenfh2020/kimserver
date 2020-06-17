#ifndef __LOG__
#define __LOG__

#include <stdarg.h>
#include <stdio.h>

#include <iostream>

namespace kim {

class Log {
   public:
    Log(const char* path) {
        if (path != NULL) m_path = path;
    }
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
