#ifndef __NODE_INFO_H__
#define __NODE_INFO_H__

#include <iostream>

namespace kim {

typedef struct node_info_s {
    node_info_s() {
        port = 0;
        gate_port = 0;
        worker_num = 0;
    }

    std::string node_type;
    std::string host;
    int port;
    std::string gate_host;
    int gate_port;
    std::string conf_path;
    std::string work_path;
    int worker_num;
} node_info_t;

typedef struct worker_info_s {
    worker_info_s() {
        worker_pid = 0;
        worker_idx = 0;
        ctrl_fd = -1;
        data_fd = -1;
    }

    std::string work_path;
    int worker_pid;
    int worker_idx;
    int ctrl_fd;  //socketpair for parent and child contact.
    int data_fd;  //socketpair for parent and child contact.
} worker_info_t;

}  // namespace kim

#endif