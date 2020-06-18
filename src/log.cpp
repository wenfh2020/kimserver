#include "log.h"

#include <sys/time.h>
#include <time.h>

namespace kim {

#define LOG_MAX_LEN 1024

Log::Log() : m_cur_level(LL_DEBUG) {
}

void Log::set_log_path(const char* path) {
    if (path != NULL) m_path = path;
}

bool Log::set_level(int level) {
    if (level >= LL_EMERG || level <= LL_DEBUG) {
        m_cur_level = level;
        return true;
    }
    return false;
}

bool Log::set_level(const char* level) {
    if (NULL == level) return false;

    int ll = -1;
    if (strcasecmp(level, "DEBUG") == 0) {
        ll = kim::Log::LL_DEBUG;
    } else if (strcasecmp(level, "INFO") == 0) {
        ll = kim::Log::LL_INFO;
    } else if (strcasecmp(level, "NOTICE") == 0) {
        ll = kim::Log::LL_NOTICE;
    } else if (strcasecmp(level, "WARNING") == 0) {
        ll = kim::Log::LL_WARNING;
    } else if (strcasecmp(level, "ERR") == 0) {
        ll = kim::Log::LL_ERR;
    } else if (strcasecmp(level, "CRIT") == 0) {
        ll = kim::Log::LL_CRIT;
    } else if (strcasecmp(level, "ALERT") == 0) {
        ll = kim::Log::LL_ALERT;
    } else if (strcasecmp(level, "EMERG") == 0) {
        ll = kim::Log::LL_EMERG;
    } else {
        return false;
    }

    m_cur_level = ll;
    return true;
}

bool Log::log_data(const char* file_name, int file_line, const char* func_name, int level, const char* fmt, ...) {
    va_list ap;
    char msg[LOG_MAX_LEN] = {0};

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    return log_raw(file_name, file_line, func_name, level, msg);
}

bool Log::log_raw(const char* file_name, int file_line, const char* func_name, int level, const char* msg) {
    if (level < LL_EMERG || level > LL_DEBUG || level > m_cur_level) {
        return false;
    }

    FILE* fp;
    fp = m_path.empty() ? stdout : fopen(m_path.c_str(), "a");
    if (!fp) return false;

    int off;
    char buf[64] = {0};
    struct timeval tv;
    char levels[][10] = {"EMERG", "ALERT", "CRIT", "ERR", "WARNING", "NOTICE", "INFO", "DEBUG"};

    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    gettimeofday(&tv, NULL);
    off = strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S.", tm);
    snprintf(buf + off, sizeof(buf) - off, "%03d]", (int)tv.tv_usec / 1000);
    fprintf(fp, "[%s]%s[%s:%s:%d] %s\n", levels[level], buf, file_name, func_name, file_line, msg);

    fflush(fp);
    if (!m_path.empty()) fclose(fp);
    return true;
}

}  // namespace kim