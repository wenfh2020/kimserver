#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <memory>

#include "util/log.h"

#ifdef __cplusplus
extern "C" {
#endif

namespace kim {

typedef struct channel_s {
    int fd;
    int family;
    int codec;
} channel_t;

int write_channel(int fd, channel_t* ch, size_t size, Log* logger);
int read_channel(int fd, channel_t* ch, size_t size, Log* logger);

}  // namespace kim

#ifdef __cplusplus
}
#endif

#endif  //__CHANNEL_H__
