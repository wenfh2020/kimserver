#include "worker.h"

namespace kim {

Worker::Worker(const std::string& work_path, int ctrl_fd, int data_fd, int worker_idx) {
    m_worker_info.work_path = work_path;
    m_worker_info.ctrl_fd = ctrl_fd;
    m_worker_info.data_fd = data_fd;
    m_worker_info.worker_idx = worker_idx;
    m_worker_info.worker_pid = getpid();
}

bool Worker::init(const util::CJsonObject& json_conf) {
    return true;
}

bool Worker::run() { return true; }

}  // namespace kim