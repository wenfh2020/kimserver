#ifndef __KIM_WORKER_DATA_MGR_H__
#define __KIM_WORKER_DATA_MGR_H__

#include "nodes.h"

namespace kim {

typedef struct worker_info_s {
    int pid = 0;           /* worker's process id. */
    int index = -1;        /* worker's index which assiged by manager. */
    int ctrl_fd = -1;      /* socketpair for parent and child. */
    int data_fd = -1;      /* socketpair for parent and child. */
    std::string work_path; /* process work path. */
} worker_info_t;

class WorkerDataMgr {
   public:
    WorkerDataMgr();
    virtual ~WorkerDataMgr();

   public:
    void add_worker_info(int index, int pid, int ctrl_fd, int data_fd);
    bool del_worker_info(int pid);
    int get_next_worker_data_fd();
    bool get_worker_chanel(int pid, int* chs);
    bool get_worker_index(int pid, int& index);
    int get_worker_data_fd(int worker_index);
    const std::unordered_map<int, worker_info_t*>& get_infos() const { return m_worker_info; }

   private:
    /* key: pid. */
    std::unordered_map<int, worker_info_t*> m_worker_info;
    std::unordered_map<int, worker_info_t*>::iterator m_itr_worker_info;
    /* key: chanel, value: worker's pid. */
    std::unordered_map<int, int> m_fd_pid;
};

}  // namespace kim

#endif  //__KIM_WORKER_DATA_MGR_H__