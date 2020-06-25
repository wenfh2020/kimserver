#ifndef __ANET_H__
#define __ANET_H__

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANET_OK 0
#define ANET_ERR -1
#define ANET_ERR_LEN 256

int anet_tcp_server(char *err, int port, const char *bindaddr, int backlog);
int anet_tcp6_server(char *err, int port, const char *bindaddr, int backlog);

int anet_block(char *err, int fd);
int anet_no_block(char *err, int fd);

int anet_tcp_accept(char *err, int s, char *ip, size_t ip_len, int *port);
int anet_keep_alive(char *err, int fd, int interval);
int anet_set_tcp_no_delay(char *err, int fd, int val);

#ifdef __cplusplus
}
#endif

#endif
