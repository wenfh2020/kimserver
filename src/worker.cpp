#include "worker.h"

#include "util/set_proc_title.h"

namespace kim {

Worker::Worker(Log* logger, const std::string& server_name) : m_logger(logger),
                                                              m_net(NULL) {
    set_proc_title("%s", server_name.c_str());
}

bool Worker::init(const worker_info_t* info, WorkerDataMgr* mgr) {
    m_worker_info.work_path = info->work_path;
    m_worker_info.ctrl_fd = info->ctrl_fd;
    m_worker_info.data_fd = info->data_fd;
    m_worker_info.index = info->index;
    m_worker_info.pid = getpid();

    if (!create_network(mgr)) {
        LOG_ERROR("create network failed!");
        return false;
    }

    LOG_INFO("init network done!");
    return true;
}

bool Worker::create_network(WorkerDataMgr* mgr) {
    m_net = new Network(m_logger, IEventsCallback::WORKER);
    if (m_net == NULL) {
        LOG_ERROR("new network failed!");
        return false;
    }

    if (!m_net->create(this, mgr, m_worker_info.ctrl_fd, m_worker_info.data_fd)) {
        LOG_ERROR("init network fail!");
        return false;
    }

    LOG_INFO("create network done!");
    return true;
}

void Worker::run() {
    if (m_net != NULL) m_net->run();
}

void Worker::on_terminated(struct ev_signal* s) {
    LOG_DEBUG("on_terminated()");
    if (s == NULL) return;

    int signum = s->signum;
    LOG_CRIT("worker terminated by signal: %d", signum);
    SAFE_DELETE(s);
    exit(signum);
    return;
}

}  // namespace kim