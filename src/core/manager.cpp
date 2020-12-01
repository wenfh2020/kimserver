#include "manager.h"

#include "connection.h"
#include "net/anet.h"
#include "server.h"
#include "util/set_proc_title.h"
#include "util/util.h"
#include "worker.h"

// #define MAX_CLIENTS 10000

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
    char work_path[MAX_PATH];
    if (!getcwd(work_path, sizeof(work_path))) {
        return false;
    }
    m_node_info.set_work_path(work_path);

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

    if (!EventsCallback::init(m_logger, m_net)) {
        LOG_ERROR("init events callback failed!");
        return false;
    }

    if (!setup_timer()) {
        LOG_ERROR("load timer failed!");
        return false;
    }

    create_workers();
    set_proc_title("%s", m_conf("server_name").c_str());
    LOG_INFO("init manager done!");
    return true;
}

bool Manager::load_logger() {
    m_logger = new Log;
    if (m_logger == nullptr) {
        LOG_ERROR("new log failed!");
        return false;
    }

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s",
             m_node_info.work_path().c_str(), m_conf("log_path").c_str());

    if (!m_logger->set_log_path(path)) {
        LOG_ERROR("set log path failed! path: %s", path);
        return false;
    }

    if (!m_logger->set_level(m_conf("log_level").c_str())) {
        LOG_ERROR("invalid log level!");
        return false;
    }

    m_logger->set_worker_index(0);
    m_logger->set_process_type(true);
    return true;
}

bool Manager::load_config(const char* path) {
    CJsonObject conf;
    if (!conf.Load(path)) {
        LOG_ERROR("load json config failed! %s", path);
        return false;
    }

    m_old_conf = m_conf;
    m_conf = conf;
    m_node_info.set_conf_path(path);

    if (m_old_conf.ToString() != m_conf.ToString()) {
        if (m_old_conf.ToString().empty()) {
            m_node_info.set_worker_cnt(str_to_int(m_conf("worker_cnt")));
            m_node_info.set_node_type(m_conf("node_type"));
            m_node_info.mutable_addr_info()->set_node_host(m_conf("node_host"));
            m_node_info.mutable_addr_info()->set_node_port(str_to_int(m_conf("node_port")));
            m_node_info.mutable_addr_info()->set_gate_host(m_conf("gate_host"));
            m_node_info.mutable_addr_info()->set_gate_port(str_to_int(m_conf("gate_port")));
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

    if (!m_net->create_m(m_node_info.mutable_addr_info(), m_conf)) {
        LOG_ERROR("init network failed!");
        SAFE_DELETE(m_net);
        return false;
    }
    return true;
}

void Manager::on_repeat_timer(void* privdata) {
    if (m_net != nullptr) {
        m_net->on_repeat_timer(privdata);
    }
    restart_workers();
}

void Manager::on_terminated(ev_signal* s) {
    LOG_WARN("%s terminated by signal %d!", m_conf("server_name").c_str(), s->signum);
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
    int chs[2];
    int worker_index;

    if (m_net->worker_data_mgr()->get_worker_chanel(pid, chs)) {
        m_net->close_conn(chs[0]);
        m_net->close_conn(chs[1]);
    }

    worker_index = m_net->worker_data_mgr()->get_worker_index(pid);
    if (worker_index == -1) {
        LOG_ERROR("can not find pid: %d work info.");
        return false;
    }

    m_net->worker_data_mgr()->del_worker_info(pid);
    m_restart_workers.push_back(worker_index);
    return true;
}

void Manager::restart_workers() {
    auto it = m_restart_workers.begin();
    for (; it != m_restart_workers.end();) {
        int worker_index = *it;
        LOG_DEBUG("restart worker, index: %d", worker_index);
        if (create_worker(worker_index)) {
            LOG_INFO("restart worker ok! index: %d", worker_index);
            m_restart_workers.erase(it++);
        } else {
            LOG_ERROR("create worker failed! index: %d", worker_index);
            it++;
        }
    }
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

    if ((pid = fork()) == 0) {
        /* child. */
        m_net->end_ev_loop();
        m_net->close_fds();
        destory();

        close(ctrl_fds[0]);
        close(data_fds[0]);

        worker_info_t info{0, worker_index, ctrl_fds[1], data_fds[1], m_node_info.work_path()};
        LOG_INFO("worker chanels, fd1: %d, fd2: %d", info.ctrl_fd, info.data_fd);

        Worker worker(worker_name(worker_index));
        if (!worker.init(&info, m_conf)) {
            exit(EXIT_CHILD_INIT_FAIL);
        }
        worker.run();
        LOG_INFO("child exit! index: %d", worker_index);
        exit(EXIT_CHILD);
    } else if (pid > 0) {
        /* parent. */
        close(ctrl_fds[1]);
        close(data_fds[1]);

        if (!m_net->add_read_event(ctrl_fds[0], Codec::TYPE::PROTOBUF, true) ||
            !m_net->add_read_event(data_fds[0], Codec::TYPE::PROTOBUF, true)) {
            m_net->close_conn(ctrl_fds[0]);
            m_net->close_conn(data_fds[0]);
            LOG_CRIT("chanel fd add event failed! kill child: %d", pid);
            kill(pid, SIGKILL);
            return false;
        }

        m_net->worker_data_mgr()->add_worker_info(
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
    for (int i = 1; i <= m_node_info.worker_cnt(); i++) {
        if (!create_worker(i)) {
            LOG_ERROR("create worker failed! index: %d", i);
        }
    }
}

std::string Manager::worker_name(int index) {
    return format_str("%s_w_%d", m_conf("server_name").c_str(), index);
}

}  // namespace kim