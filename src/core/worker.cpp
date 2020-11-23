#include "worker.h"

#include <random>

#include "util/set_proc_title.h"
#include "util/util.h"

namespace kim {

Worker::Worker(const std::string& name) {
    srand((unsigned)time(NULL));
    set_proc_title("%s", name.c_str());
}

Worker::~Worker() {
    if (m_net != nullptr) {
        m_net->events()->del_timer_event(m_timer);
        m_timer = nullptr;
    }
    SAFE_DELETE(m_net);
    SAFE_DELETE(m_logger);
}

bool Worker::init(const worker_info_t* info, const CJsonObject& conf) {
    m_worker_info.work_path = info->work_path;
    m_worker_info.ctrl_fd = info->ctrl_fd;
    m_worker_info.data_fd = info->data_fd;
    m_worker_info.index = info->index;
    m_worker_info.pid = getpid();

    m_conf = conf;

    if (!load_logger()) {
        LOG_ERROR("init log failed!");
        return false;
    }

    LOG_INFO("init worker, index: %d, ctrl_fd: %d, data_fd: %d",
             info->index, info->ctrl_fd, info->data_fd);

    if (!load_network()) {
        LOG_ERROR("create network failed!");
        return false;
    }

    if (!load_timer()) {
        LOG_ERROR("load timer failed!");
        return false;
    }

    return true;
}

bool Worker::load_logger() {
    LOG_TRACE("load logger.");

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s/%s", m_worker_info.work_path.c_str(),
             m_conf("log_path").c_str());

    m_logger = new Log;
    if (m_logger == nullptr) {
        LOG_ERROR("new log failed!");
        return false;
    }

    if (!m_logger->set_log_path(path)) {
        LOG_ERROR("set log path failed! path: %s", path);
        return false;
    }

    if (!m_logger->set_level(m_conf("log_level").c_str())) {
        LOG_ERROR("invalid log level!");
        return false;
    }

    m_logger->set_process_type(false);
    m_logger->set_worker_index(m_worker_info.index);
    return true;
}

bool Worker::load_timer() {
    m_net->events()->set_repeat_timer_callback_fn(&on_repeat_timer_callback);
    m_timer = m_net->events()->add_repeat_timer(REPEAT_TIMEOUT_VAL, m_timer, this);
    return (m_timer != nullptr);
}

bool Worker::load_network() {
    LOG_TRACE("load network!");

    m_net = new Network(m_logger, Network::TYPE::WORKER);
    if (m_net == nullptr) {
        LOG_ERROR("new network failed!");
        goto error;
    }

    if (!m_net->create_w(m_conf, m_worker_info.ctrl_fd,
                         m_worker_info.data_fd, m_worker_info.index)) {
        LOG_ERROR("init network failed!");
        goto error;
    }

    /* set events, catch 'ctrl + c'. */
    m_net->events()->set_sig_callback_fn(&on_signal_callback);
    if (!m_net->events()->create_signal_event(SIGINT, this)) {
        LOG_ERROR("create signal failed!");
        goto error;
    }

    LOG_INFO("load net work done!");
    return true;

error:
    SAFE_DELETE(m_net);
    return false;
}

void Worker::run() {
    if (m_net != nullptr) {
        m_net->run();
    }
}

void Worker::on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents) {
    Worker* worker = static_cast<Worker*>(s->data);
    worker->on_terminated(s);
}

void Worker::on_repeat_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    Worker* worker = static_cast<Worker*>(w->data);
    worker->on_repeat_timer(w->data);
}

void Worker::on_terminated(ev_signal* s) {
    /* catch 'ctrl + c' */
    int signum = s->signum;
    LOG_CRIT("worker terminated by signal: %d", signum);
    SAFE_DELETE(s);
    exit(signum);
}

void Worker::on_repeat_timer(void* privdata) {
    if (m_net != nullptr) {
        m_net->on_repeat_timer(privdata);
    }
}

}  // namespace kim