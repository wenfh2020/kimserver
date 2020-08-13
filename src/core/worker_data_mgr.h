#ifndef __WORKER_DATA_MGR_H__
#define __WORKER_DATA_MGR_H__

#include <unordered_map>

#include "node_info.h"

namespace kim {

class
    WorkerDataMgr {
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
    std::unordered_map<int, WorkerInfo*> m_worker_info;                // key: pid, value: worker info.
    std::unordered_map<int, WorkerInfo*>::iterator m_itr_worker_info;  // iterator
    std::unordered_map<int, int> m_fd_pid;                             // key: chanel, value: worker's pid.
};

}  // namespace kim

#endif  //__WORKER_DATA_MGR_H__