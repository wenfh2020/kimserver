#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include <random>
#include <sstream>

#define MAX_PATH 256
#define CONFIG_MIN_RESERVED_FDS 32

std::vector<std::string> split_str(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream ss(s);
    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string format_str(const char* const fmt, ...) {
    char* buffer = NULL;
    va_list ap;

    va_start(ap, fmt);
    (void)vasprintf(&buffer, fmt, ap);
    va_end(ap);

    std::string result = buffer;
    free(buffer);

    return result;
}

std::string format_addr(const std::string& host, int port) {
    char identity[64];
    snprintf(identity, sizeof(identity), "%s:%d", host.c_str(), port);
    return std::string(identity);
}

std::string work_path() {
    char work_path[MAX_PATH] = {0};
    if (!getcwd(work_path, sizeof(work_path))) {
        return "";
    }
    return std::string(work_path);
}

std::string format_redis_cmds(const std::vector<std::string>& argv) {
    std::ostringstream oss;
    for (auto& it : argv) {
        oss << it << " ";
    }
    return oss.str();
}

void daemonize(void) {
    int fd;

    if (fork() != 0) exit(0); /* parent exits */
    setsid();                 /* create a new session */

    /* Every output goes to /dev/null. If server is daemonized but
     * the 'logfile' is set to 'stdout' */
    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
}

bool adjust_files_limit(int& max_clients) {
    rlim_t maxfiles = max_clients + CONFIG_MIN_RESERVED_FDS;
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        return false;
    }

    rlim_t oldlimit = limit.rlim_cur;
    if (oldlimit >= maxfiles) {
        return true;
    }

    rlim_t bestlimit;
    // int setrlimit_error = 0;

    /* Try to set the file limit to match 'maxfiles' or at least
     * to the higher value supported less than maxfiles. */
    bestlimit = maxfiles;
    while (bestlimit > oldlimit) {
        rlim_t decr_step = 16;

        limit.rlim_cur = bestlimit;
        limit.rlim_max = bestlimit;
        if (setrlimit(RLIMIT_NOFILE, &limit) != -1) {
            break;
        }
        // setrlimit_error = errno;

        /* We failed to set file limit to 'bestlimit'. Try with a
         * smaller limit decrementing by a few FDs per iteration. */
        if (bestlimit < decr_step) {
            break;
        }
        bestlimit -= decr_step;
    }

    /* Assume that the limit we get initially is still valid if
     * our last try was even lower. */
    if (bestlimit < oldlimit) {
        bestlimit = oldlimit;
    }

    if (bestlimit < maxfiles) {
        // unsigned int old_maxclients = max_clients;
        max_clients = bestlimit - CONFIG_MIN_RESERVED_FDS;
        /* maxclients is unsigned so may overflow: in order
                 * to check if maxclients is now logically less than 1
                 * we test indirectly via bestlimit. */
        if (bestlimit <= CONFIG_MIN_RESERVED_FDS) {
            return false;
        }
        return false;
    }
    return true;
}

const char* to_lower(char* s, int len) {
    for (int j = 0; j < len; j++) {
        s[j] = tolower(s[j]);
    }
    return s;
}

long long mstime() {
    struct timeval tv;
    long long mst;

    gettimeofday(&tv, NULL);
    mst = ((long long)tv.tv_sec) * 1000;
    mst += tv.tv_usec / 1000;
    return mst;
}

long long ustime() {
    struct timeval tv;
    long long ust;

    gettimeofday(&tv, NULL);
    ust = ((long)tv.tv_sec) * 1000000;
    ust += tv.tv_usec;
    return ust;
}

double time_now() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((tv).tv_sec + (tv).tv_usec * 1e-6);
}

double decimal_rand() {
    return double(rand()) / (double(RAND_MAX) + 1.0);
}