#include "util.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

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