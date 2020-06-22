#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <iostream>
#include <list>

#include "log.h"
#include "net/anet.h"
#include "node_info.h"

namespace kim {

class Network {
   public:
    Network(Log* logger = NULL);
    virtual ~Network() {}

    bool create(const addr_info_t* addr_info, std::list<int>& fds);
    void accept_tcp_handler(int fd, void* privdata);

   private:
    int listen_to_port(const char* bind, int port);

   private:
    Log* m_logger;
    char m_neterr[ANET_ERR_LEN];
};

}  // namespace kim
#endif  // __NETWORK_H__