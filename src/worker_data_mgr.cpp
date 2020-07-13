#include "worker_data_mgr.h"

#include "server.h"

namespace kim {

WorkerDataMgr::WorkerDataMgr() {
    m_itr_worker_info = m_worker_info.begin();
}

WorkerDataMgr::~WorkerDataMgr() {
    for (auto& it : m_worker_info) {
        SAFE_DELETE(it.second);
    }

    m_worker_info.clear();
    m_itr_worker_info = m_worker_info.end();
    m_fd_pid.clear();
}

void WorkerDataMgr::add_worker_info(int index, int pid, int ctrl_fd, int data_fd) {
    WorkerInfo* info = new WorkerInfo;
    info->pid = pid;
    info->index = index;
    info->ctrl_fd = ctrl_fd;
    info->data_fd = data_fd;

    m_fd_pid[ctrl_fd] = pid;
    m_fd_pid[data_fd] = pid;
    m_worker_info[pid] = info;

    m_itr_worker_info = m_worker_info.begin();
}

bool WorkerDataMgr::get_worker_index(int pid, int& index) {
    auto it = m_worker_info.find(pid);
    if (it == m_worker_info.end()) {
        return false;
    }

    index = it->second->index;
    return true;
}

bool WorkerDataMgr::remove_worker_info(int pid) {
    auto it = m_worker_info.find(pid);
    if (it == m_worker_info.end()) {
        return false;
    }

    WorkerInfo* info = it->second;
    if (info == nullptr) {
        m_worker_info.erase(it);
        return false;
    }

    auto it_chanel = m_fd_pid.find(info->ctrl_fd);
    if (it_chanel != m_fd_pid.end()) {
        m_fd_pid.erase(it_chanel);
    }

    it_chanel = m_fd_pid.find(info->data_fd);
    if (it_chanel != m_fd_pid.end()) {
        m_fd_pid.erase(it_chanel);
    }

    SAFE_DELETE(info);
    m_worker_info.erase(it);
    m_itr_worker_info = m_worker_info.begin();
    return true;
}

bool WorkerDataMgr::get_worker_chanel(int pid, int* chs) {
    if (chs == nullptr) {
        return false;
    }

    auto it = m_worker_info.find(pid);
    if (chs == nullptr || it == m_worker_info.end() || it->second == nullptr) {
        return false;
    }

    WorkerInfo* info = it->second;
    chs[0] = info->ctrl_fd;
    chs[1] = info->data_fd;
    return true;
}

int WorkerDataMgr::get_next_worker_data_fd() {
    if (m_worker_info.empty()) {
        return -1;
    }

    m_itr_worker_info++;
    if (m_itr_worker_info == m_worker_info.end()) {
        m_itr_worker_info = m_worker_info.begin();
    }

    return m_itr_worker_info->second->data_fd;
}

}  // namespace kim