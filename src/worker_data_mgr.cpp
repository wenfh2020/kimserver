#include "worker_data_mgr.h"

#include "server.h"

namespace kim {

WorkerDataMgr::WorkerDataMgr() {
    m_itr_worker_info = m_worker_info.begin();
}

WorkerDataMgr::~WorkerDataMgr() {
    auto it = m_worker_info.begin();
    for (; it != m_worker_info.end(); it++) {
        SAFE_DELETE(it->second);
    }

    m_worker_info.clear();
    m_itr_worker_info = m_worker_info.end();
    m_fd_pid.clear();
}

void WorkerDataMgr::add_worker_info(int index, int pid, int ctrl_fd, int data_fd) {
    worker_info_t* info = new worker_info_t;
    info->pid = pid;
    info->index = index;
    info->ctrl_fd = ctrl_fd;
    info->data_fd = data_fd;

    m_worker_info[pid] = info;
    m_itr_worker_info = m_worker_info.begin();

    m_fd_pid[ctrl_fd] = pid;
    m_fd_pid[data_fd] = pid;
}

int WorkerDataMgr::get_next_worker_data_fd() {
    if (m_worker_info.empty()) return -1;

    ++m_itr_worker_info;
    if (m_itr_worker_info == m_worker_info.end()) {
        m_itr_worker_info = m_worker_info.begin();
    }

    return m_itr_worker_info->second->data_fd;
}

}  // namespace kim