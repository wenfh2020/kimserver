#ifndef __WORKER_DATA_MGR_H__
#define __WORKER_DATA_MGR_H__

#include "nodes.h"

namespace kim {

typedef struct worker_info_s {
    int pid = 0;            // worker's process id.
    int index = -1;         // worker's index which assiged by master process.
    int ctrl_fd = -1;       // socketpair for parent and child.
    int data_fd = -1;       // socketpair for parent and child.
    std::string work_path;  // process work path.
} worker_info_t;

class WorkerDataMgr {
   public:
    WorkerDataMgr();
    virtual ~WorkerDataMgr();

   public:
    void add_worker_info(int index, int pid, int ctrl_fd, int data_fd);
    bool remove_worker_info(int pid);
    int get_next_worker_data_fd();
    bool get_worker_chanel(int pid, int* chs);
    bool get_worker_index(int pid, int& index);

   private:
    std::unordered_map<int, worker_info_t*> m_worker_info;                // key: pid, value: worker info.
    std::unordered_map<int, worker_info_t*>::iterator m_itr_worker_info;  // iterator
    std::unordered_map<int, int> m_fd_pid;                                // key: chanel, value: worker's pid.
};

}  // namespace kim

#endif  //__WORKER_DATA_MGR_H__