#include "log.h"

#include <sys/time.h>
#include <time.h>

namespace kim {

#define LOG_MAX_LEN 1024

Log::Log(const char* path) {
    set_log_path(path);
}

void Log::set_log_path(const char* path) {
    if (path != NULL) m_path = path;
}

void Log::log_data(const char* file_name, int file_line, const char* func_name, int level, const char* fmt, ...) {
    va_list ap;
    char msg[LOG_MAX_LEN] = {0};

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    log_raw(file_name, file_line, func_name, level, msg);
}

void Log::log_raw(const char* file_name, int file_line, const char* func_name, int level, const char* msg) {
    if (level > m_level) return;

    FILE* fp;
    fp = m_path.empty() ? stdout : fopen(m_path.c_str(), "a");
    if (!fp) return;

    if (m_path.empty()) {
        fprintf(fp, "%s", msg);
    } else {
        int off;
        char buf[64] = {0};
        struct timeval tv;

        time_t t = time(NULL);
        struct tm* tm = localtime(&t);
        gettimeofday(&tv, NULL);
        off = strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S.", tm);
        snprintf(buf + off, sizeof(buf) - off, "%03d]", (int)tv.tv_usec / 1000);
        fprintf(fp, "%s[%s:%s:%d] %s\n", buf, file_name, func_name, file_line, msg);
    }

    fflush(fp);
    if (!m_path.empty()) fclose(fp);
}

}  // namespace kim