#ifndef __NODE_INFO_H__
#define __NODE_INFO_H__

#include <iostream>

namespace kim {

class AddrInfo {
   public:
    AddrInfo() {}
    virtual ~AddrInfo() {}

    std::string bind;       // bind host for inner server.
    int port = 0;           // port for inner server.
    std::string gate_bind;  // bind host for user client.
    int gate_port = 0;      // port for user client.
};

////////////////////////////////////

class NodeInfo {
   public:
    NodeInfo() {}
    virtual ~NodeInfo() {}

    AddrInfo addr_info;        // network addr info.
    std::string node_type;     // node type in cluster.
    std::string conf_path;     // config path.
    std::string work_path;     // process work path.
    int worker_processes = 1;  // number of worker's processes.
};

////////////////////////////////////

class WorkInfo {
   public:
    WorkInfo() {}
    virtual ~WorkInfo() {}

    int pid = 0;            // worker's process id.
    int index = -1;         // worker's index which assiged by master process.
    int ctrl_fd = -1;       // socketpair for parent and child contact.
    int data_fd = -1;       // socketpair for parent and child contact.
    std::string work_path;  // process work path.
};

}  // namespace kim

#endif  //__NODE_INFO_H__