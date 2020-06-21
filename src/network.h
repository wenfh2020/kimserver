#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <iostream>
#include <list>

#include "log.h"
#include "node_info.h"

namespace kim {

class Network {
   public:
    Network(Log* logger = NULL);
    virtual ~Network() {}

    bool create(const addr_info_t* addr_info, std::list<int>& fds);

   private:
    int listen_to_port(const char* bind, int port);

   private:
    Log* m_logger;
};

}  // namespace kim
#endif  // __NETWORK_H__