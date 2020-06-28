#ifndef __NODE_INFO_H__
#define __NODE_INFO_H__

#include <iostream>

namespace kim {

typedef struct addr_info_s {
    addr_info_s() {
        port = 0;
        gate_port = 0;
    }
    std::string bind;       // bind host for inner server.
    int port;               // port for inner server.
    std::string gate_bind;  // bind host for user client.
    int gate_port;          // port for user client.
} addr_info_t;

typedef struct node_info_s {
    node_info_s() {
        worker_processes = 0;
    }

    addr_info_t addr_info;  // network addr info.
    std::string node_type;  // node type in cluster.
    std::string conf_path;  // config path.
    std::string work_path;  // process work path.
    int worker_processes;   // number of worker's processes.
} node_info_t;

typedef struct worker_info_s {
    worker_info_s() {
        pid = 0;
        index = 0;
        ctrl_fd = -1;
        data_fd = -1;
    }

    int pid;                // worker's process id.
    int index;              // worker's index which assiged by master process.
    int ctrl_fd;            // socketpair for parent and child contact.
    int data_fd;            // socketpair for parent and child contact.
    std::string work_path;  // process work path.
} worker_info_t;

}  // namespace kim

#endif  //__NODE_INFO_H__