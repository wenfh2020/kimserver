#include "worker_data_mgr.h"

#include "server.h"

namespace kim {

WorkerDataMgr::WorkerDataMgr() {
    m_itr_worker = m_workers.begin();
}

WorkerDataMgr::~WorkerDataMgr() {
    for (auto& it : m_workers) {
        SAFE_DELETE(it.second);
    }
    m_workers.clear();
    m_itr_worker = m_workers.end();
    m_fd_pid.clear();
}

bool WorkerDataMgr::add_worker_info(int index, int pid, int ctrl_fd, int data_fd) {
    worker_info_t* info = new worker_info_t{pid, index, ctrl_fd, data_fd};
    if (info != nullptr) {
        m_fd_pid[ctrl_fd] = pid;
        m_fd_pid[data_fd] = pid;
        m_workers[pid] = info;
        m_itr_worker = m_workers.begin();
        return true;
    }
    return false;
}

bool WorkerDataMgr::get_worker_index(int pid, int& index) {
    auto it = m_workers.find(pid);
    if (it != m_workers.end()) {
        index = it->second->index;
        return true;
    }
    return false;
}

int WorkerDataMgr::get_worker_data_fd(int worker_index) {
    for (auto& v : m_workers) {
        if (v.second->index == worker_index) {
            return v.second->data_fd;
        }
    }
    return -1;
}

bool WorkerDataMgr::del_worker_info(int pid) {
    auto it = m_workers.find(pid);
    if (it == m_workers.end()) {
        return false;
    }

    worker_info_t* info = it->second;
    if (info == nullptr) {
        m_workers.erase(it);
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
    m_workers.erase(it);
    m_itr_worker = m_workers.begin();
    return true;
}

bool WorkerDataMgr::get_worker_chanel(int pid, int* chs) {
    if (chs == nullptr) {
        return false;
    }

    auto it = m_workers.find(pid);
    if (chs == nullptr || it == m_workers.end() || it->second == nullptr) {
        return false;
    }

    worker_info_t* info = it->second;
    chs[0] = info->ctrl_fd;
    chs[1] = info->data_fd;
    return true;
}

int WorkerDataMgr::get_next_worker_data_fd() {
    if (m_workers.empty()) {
        return -1;
    }

    m_itr_worker++;
    if (m_itr_worker == m_workers.end()) {
        m_itr_worker = m_workers.begin();
    }

    return m_itr_worker->second->data_fd;
}

}  // namespace kim