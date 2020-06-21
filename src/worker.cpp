#include "worker.h"

#include "util/set_proc_title.h"

namespace kim {

Worker::Worker(const worker_info_s* worker_info) {
    m_worker_info.work_path = worker_info->work_path;
    m_worker_info.ctrl_fd = worker_info->ctrl_fd;
    m_worker_info.data_fd = worker_info->data_fd;
    m_worker_info.worker_idx = worker_info->worker_idx;
    m_worker_info.worker_pid = getpid();
}

bool Worker::init(Log* log, const std::string& server_name) {
    m_logger = log;

    char name[64] = {0};
    snprintf(name, sizeof(name), "%s_w_%d",
             server_name.c_str(), m_worker_info.worker_idx);
    set_proc_title("%s", name);
    return true;
}

bool Worker::run() { return true; }

}  // namespace kim