#ifndef __NODE_INFO_H__
#define __NODE_INFO_H__

#include <iostream>

namespace kim {

typedef struct {
    std::string node_type;
    std::string host;
    int port;
    std::string gate_host;
    int gate_port;
} node_info_t;

}  // namespace kim

#endif