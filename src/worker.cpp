#include "worker.h"

#include "util/set_proc_title.h"

namespace kim {

Worker::Worker(Log* logger) : m_logger(logger), m_net(NULL) {
}

bool Worker::init(const worker_info_t* info, const std::string& server_name) {
    m_worker_info.work_path = info->work_path;
    m_worker_info.ctrl_fd = info->ctrl_fd;
    m_worker_info.data_fd = info->data_fd;
    m_worker_info.index = info->index;
    m_worker_info.pid = getpid();

    char name[64] = {0};
    snprintf(name, sizeof(name), "%s_w_%d",
             server_name.c_str(), m_worker_info.index);
    set_proc_title("%s", name);

    LOG_DEBUG("worker name: %s", name);

    if (!create_network()) {
        LOG_ERROR("create network failed!");
        return false;
    }

    LOG_INFO("init network done!");
    return true;
}

bool Worker::create_network() {
    LOG_DEBUG("create_network()");

    m_net = new Network(m_logger, IEventsCallback::WORKER);
    if (m_net == NULL) {
        LOG_ERROR("new network failed!");
        return false;
    }

    if (!m_net->create(this, m_worker_info.ctrl_fd, m_worker_info.data_fd)) {
        LOG_ERROR("init network fail!");
        return false;
    }

    return true;
}

void Worker::run() {
    if (m_net != NULL) m_net->run();
}

void Worker::on_terminated(struct ev_signal* s) {
    LOG_DEBUG("on_terminated()");

    int signum = s->signum;
    SAFE_DELETE(s);
    LOG_CRIT("worker terminated by signal: %d", signum);
    exit(signum);
    return;
}

}  // namespace kim