#include "manager.h"

#include "context.h"
#include "net/anet.h"
#include "server.h"
#include "util/set_proc_title.h"
#include "worker.h"

namespace kim {

Manager::Manager() {
}

Manager::~Manager() {
    destory();
}

void Manager::destory() {
    SAFE_DELETE(m_net);
    SAFE_DELETE(m_logger);
}

void Manager::run() {
    if (m_net != nullptr) {
        m_net->run();
    }
}

bool Manager::init(const char* conf_path) {
    char work_path[MAX_PATH] = {0};
    if (!getcwd(work_path, sizeof(work_path))) {
        return false;
    }
    m_node_info.work_path = work_path;

    if (!load_config(conf_path)) {
        LOG_ERROR("load config failed! %s", conf_path);
        return false;
    }

    if (!load_logger()) {
        LOG_ERROR("init log failed!");
        return false;
    }

    if (!load_network()) {
        LOG_ERROR("create network failed!");
        return false;
    }

    create_workers();
    set_proc_title("%s", m_conf("server_name").c_str());

    LOG_INFO("init manager success!");
    return true;
}

bool Manager::load_logger() {
    if (m_logger == nullptr) {
        m_logger = new Log;
        if (m_logger == nullptr) {
            LOG_ERROR("new log failed!");
            return false;
        }
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s/%s",
             m_node_info.work_path.c_str(), m_conf("log_path").c_str());

    if (!m_logger->set_log_path(path)) {
        LOG_ERROR("set log path failed! path: %s", path);
        return false;
    }

    if (!m_logger->set_level(m_conf("log_level").c_str())) {
        LOG_ERROR("invalid log level!");
        return false;
    }

    return true;
}

bool Manager::load_config(const char* path) {
    CJsonObject conf;
    if (!conf.Load(path)) {
        LOG_ERROR("load json config failed! %s", path);
        return false;
    }

    m_node_info.conf_path = path;
    m_old_conf = m_conf;
    m_conf = conf;

    if (m_old_conf.ToString() != m_conf.ToString()) {
        if (m_old_conf.ToString().empty()) {
            m_conf.Get("worker_processes", m_node_info.worker_processes);
            m_conf.Get("node_type", m_node_info.node_type);
            m_conf.Get("bind", m_node_info.addr_info.bind);
            m_conf.Get("port", m_node_info.addr_info.port);
            m_conf.Get("gate_bind", m_node_info.addr_info.gate_bind);
            m_conf.Get("gate_port", m_node_info.addr_info.gate_port);
            LOG_DEBUG("worker processes: %d", m_node_info.worker_processes);
        }
    }

    return true;
}

bool Manager::load_network() {
    m_net = new Network(m_logger, Network::TYPE::MANAGER);
    if (m_net == nullptr) {
        LOG_ERROR("new network failed!");
        return false;
    }

    if (!m_net->create(&m_node_info.addr_info, this, &m_worker_data_mgr)) {
        SAFE_DELETE(m_net);
        LOG_ERROR("init network fail!");
        return false;
    }

    int codec = 0;
    if (m_conf.Get("gate_codec", codec)) {
        m_net->set_gate_codec_type(static_cast<Codec::CODEC_TYPE>(codec));
    }
    LOG_DEBUG("gate codec: %d", codec);

    return true;
}

void Manager::on_terminated(ev_signal* s) {
    if (s == nullptr) {
        return;
    }
    LOG_WARNING("%s terminated by signal %d!",
                m_conf("server_name").c_str(), s->signum);
    SAFE_DELETE(s);
    destory();
    exit(s->signum);
}

void Manager::on_child_terminated(ev_signal* s) {
    int pid, status, res;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            res = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            res = WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
            res = WSTOPSIG(status);
        }
        LOG_CRIT("child terminated! pid: %d, signal %d, error %d:  res: %d!",
                 pid, s->signum, status, res);
        restart_worker(pid);
    }
}

bool Manager::restart_worker(pid_t pid) {
    LOG_DEBUG("restart worker, pid: %d", pid);

    // close conntact chanel.
    int chs[2];
    if (m_worker_data_mgr.get_worker_chanel(pid, chs)) {
        m_net->close_chanel(chs);
    }

    int worker_index;
    if (!m_worker_data_mgr.get_worker_index(pid, worker_index)) {
        LOG_ERROR("can not find pid: %d work info.");
        return false;
    }

    // clear worker data.
    m_worker_data_mgr.remove_worker_info(pid);

    // fork new process.
    bool res = create_worker(worker_index);
    if (res) {
        LOG_INFO("restart worker success! index: %d", worker_index);
    } else {
        LOG_ERROR("create worker failed! index: %d", worker_index);
    }
    return res;
}

bool Manager::create_worker(int worker_index) {
    int pid, data_fds[2], ctrl_fds[2];

    if (socketpair(PF_UNIX, SOCK_STREAM, 0, ctrl_fds) < 0) {
        LOG_ERROR("create socket pair failed! %d: %s", errno, strerror(errno));
        return false;
    }

    if (socketpair(PF_UNIX, SOCK_STREAM, 0, data_fds) < 0) {
        LOG_ERROR("create socket pair failed! %d: %s", errno, strerror(errno));
        m_net->close_chanel(ctrl_fds);
        return false;
    }

    if ((pid = fork()) == 0) {  // child
        m_net->end_ev_loop();
        m_net->close_fds();

        close(ctrl_fds[0]);
        close(data_fds[0]);

        WorkerInfo info;
        info.work_path = m_node_info.work_path;
        info.ctrl_fd = ctrl_fds[1];
        info.data_fd = data_fds[1];
        info.index = worker_index;

        Worker worker(get_worker_name(worker_index));
        if (!worker.init(&info, m_conf)) {
            exit(EXIT_CHILD_INIT_FAIL);
        }
        worker.run();

        LOG_INFO("child exit! index: %d", worker_index);
        exit(EXIT_CHILD);
    } else if (pid > 0) {  // parent
        close(ctrl_fds[1]);
        close(data_fds[1]);

        if (!m_net->add_conncted_read_event(ctrl_fds[0], true) ||
            !m_net->add_conncted_read_event(data_fds[0], true)) {
            m_net->close_conn(ctrl_fds[0]);
            m_net->close_conn(data_fds[0]);
            LOG_CRIT("chanel fd add event failed! kill child: %d", pid);
            kill(pid, SIGKILL);
        }

        m_worker_data_mgr.add_worker_info(
            worker_index, pid, ctrl_fds[0], data_fds[0]);
        LOG_INFO("manager ctrl_fd: %d, data_fd: %d", ctrl_fds[0], data_fds[0]);
        return true;
    } else {
        m_net->close_chanel(data_fds);
        m_net->close_chanel(ctrl_fds);
        LOG_ERROR("error: %d, %s", errno, strerror(errno));
    }

    return false;
}

void Manager::create_workers() {
    for (int i = 0; i < m_node_info.worker_processes; i++) {
        if (!create_worker(i)) {
            LOG_ERROR("create worker failed! index: %d", i);
        }
    }
}

std::string Manager::get_worker_name(int index) {
    char name[64] = {0};
    snprintf(name, sizeof(name), "%s_w_%d",
             m_conf("server_name").c_str(), index);
    return std::string(name);
}

}  // namespace kim