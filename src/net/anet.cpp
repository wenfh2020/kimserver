#include "anet.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ANET_OK 0
#define ANET_ERR -1
#define ANET_ERR_LEN 256

static void anet_set_error(char *err, const char *fmt, ...) {
    va_list ap;

    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, ANET_ERR_LEN, fmt, ap);
    va_end(ap);
}

static int anet_set_block(char *err, int fd, int non_block) {
    int flags;

    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        anet_set_error(err, "fcntl(F_GETFL): %s", strerror(errno));
        return ANET_ERR;
    }

    if (non_block) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, flags) == -1) {
        anet_set_error(err, "fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

int anet_no_block(char *err, int fd) {
    return anet_set_block(err, fd, 1);
}

int anet_block(char *err, int fd) {
    return anet_set_block(err, fd, 0);
}