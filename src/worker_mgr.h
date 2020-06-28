#ifndef __WORKER_MGR_H__
#define __WORKER_MGR_H__

#include <map>

#include "node_info.h"

namespace kim {

class WorkerMgr {
   public:
    WorkerMgr();
    virtual ~WorkerMgr();

   public:
    void add_worker_info(int worker_index, int pid, int ctrl_fd, int data_fd);
    int get_next_worker_data_fd();

   private:
    std::map<int, worker_info_t*> m_worker_info;                // key: pid, value: worker info.
    std::map<int, worker_info_t*>::iterator m_itr_worker_info;  // iterator
    std::map<int, int> m_fd_pid;                                // key: chanel, value: worker's pid.
};

}  // namespace kim

#endif  //__WORKER_MGR_H__