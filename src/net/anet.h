#ifndef __ANET_H__
#define __ANET_H__

#ifdef __cplusplus
extern "C" {
#endif

int anet_tcp_server(char *err, int port, const char *bindaddr, int backlog);
int anet_tcp6_server(char *err, int port, const char *bindaddr, int backlog);

int anet_block(char *err, int fd);
int anet_no_block(char *err, int fd);

#ifdef __cplusplus
}
#endif

#endif
