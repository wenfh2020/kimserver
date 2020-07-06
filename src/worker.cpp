#include "worker.h"

#include "util/set_proc_title.h"

namespace kim {

Worker::Worker(const std::string& name) {
    set_proc_title("%s", name.c_str());
}

Worker::~Worker() {
    SAFE_DELETE(m_net);
}

bool Worker::init(const worker_info_t* info, const CJsonObject& conf) {
    m_worker_info.work_path = info->work_path;
    m_worker_info.ctrl_fd = info->ctrl_fd;
    m_worker_info.data_fd = info->data_fd;
    m_worker_info.index = info->index;
    m_worker_info.pid = getpid();

    m_json_conf = conf;

    LOG_INFO("init worker, index: %d, ctrl_fd: %d, data_fd: %d",
             info->index, info->ctrl_fd, info->data_fd);

    if (!load_logger()) {
        LOG_ERROR("init log failed!");
        return false;
    }

    if (!load_network()) {
        LOG_ERROR("create network failed!");
        return false;
    }

    return true;
}

bool Worker::load_logger() {
    if (m_logger == nullptr) {
        char path[MAX_PATH] = {0};
        snprintf(path, sizeof(path), "%s/%s", m_worker_info.work_path.c_str(),
                 m_json_conf("log_path").c_str());

        m_logger = std::make_shared<Log>();
        if (m_logger == nullptr) {
            LOG_ERROR("new log failed!");
            return false;
        }

        if (!m_logger->set_log_path(path)) {
            LOG_ERROR("set log path failed! path: %s", path);
            return false;
        }
    }

    if (!m_logger->set_level(m_json_conf("log_level").c_str())) {
        LOG_ERROR("invalid log level!");
        return false;
    }

    return true;
}

bool Worker::load_network() {
    m_net = new Network(m_logger, Network::TYPE::WORKER);
    if (m_net == nullptr) {
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
    if (m_net != nullptr) {
        m_net->run();
    }
}

void Worker::on_terminated(ev_signal* s) {
    if (s == nullptr) {
        return;
    }
    int signum = s->signum;
    SAFE_DELETE(s);
    LOG_CRIT("worker terminated by signal: %d", signum);
    exit(signum);
    return;
}

}  // namespace kim