#include "manager.h"

#include <sys/socket.h>

#include <fstream>
#include <iosfwd>
#include <sstream>

#include "context.h"
#include "net/anet.h"
#include "server.h"
#include "util/set_proc_title.h"
#include "worker.h"

namespace kim {

Manager::Manager(Log* logger) : m_logger(logger), m_net(nullptr) {
}

Manager::~Manager() {
    destory();
}

void Manager::destory() {
    if (m_net != nullptr) {
        SAFE_DELETE(m_net);
    }

    auto itr = m_pid_worker_info.begin();
    for (; itr != m_pid_worker_info.end(); itr++) {
        SAFE_DELETE(itr->second);
    }
}

void Manager::run() {
    if (m_net != nullptr) m_net->run();
}

bool Manager::init(const char* conf_path) {
    if (!load_config(conf_path)) {
        LOG_ERROR("load config failed! %s", conf_path);
        return false;
    }

    set_proc_title("%s", m_json_conf("server_name").c_str());

    if (!init_logger()) {
        LOG_ERROR("init log failed!");
        return false;
    }

    if (!create_network()) {
        LOG_ERROR("create network failed!");
        return false;
    }

    create_workers();

    LOG_INFO("init manager success!");
    return true;
}

bool Manager::init_logger() {
    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s/%s",
             m_node_info.work_path.c_str(), m_json_conf("log_path").c_str());

    FILE* f;
    f = fopen(path, "a");
    if (f == nullptr) {
        LOG_ERROR("cant not open log file: %s", path);
        return false;
    }
    fclose(f);

    m_logger->set_log_path(path);
    if (!m_logger->set_level(m_json_conf("log_level").c_str())) {
        LOG_ERROR("invalid log level!");
        return false;
    }
    return true;
}

bool Manager::load_config(const char* path) {
    if (access(path, R_OK) == -1) {
        LOG_ERROR("cant not access config file!");
        return false;
    }

    if (m_node_info.work_path.empty()) {
        char work_path[MAX_PATH] = {0};
        if (!getcwd(work_path, sizeof(work_path))) {
            LOG_ERROR("get work path failed!");
            return false;
        }
        m_node_info.work_path = work_path;
    }

    std::ifstream fin(path);
    if (!fin.good()) {
        LOG_ERROR("load config file failed! %s", path);
        return false;
    }

    CJsonObject json_conf;
    std::stringstream content;
    content << fin.rdbuf();
    if (!json_conf.Parse(content.str())) {
        fin.close();
        LOG_ERROR("parse json config failed! %s", path);
        return false;
    }
    fin.close();

    m_node_info.conf_path = path;
    m_old_json_conf = m_json_conf;
    m_json_conf = json_conf;

    if (m_old_json_conf.ToString() != m_json_conf.ToString()) {
        if (m_old_json_conf.ToString().empty()) {
            m_json_conf.Get("worker_processes", m_node_info.worker_processes);
            m_json_conf.Get("node_type", m_node_info.node_type);
            m_json_conf.Get("bind", m_node_info.addr_info.bind);
            m_json_conf.Get("port", m_node_info.addr_info.port);
            m_json_conf.Get("gate_bind", m_node_info.addr_info.gate_bind);
            m_json_conf.Get("gate_port", m_node_info.addr_info.gate_port);
            // LOG_DEBUG("worker processes: %d", m_node_info.worker_processes);
        }
    }

    return true;
}

bool Manager::create_network() {
    m_net = new Network(m_logger, IEventsCallback::TYPE::MANAGER);
    if (m_net == nullptr) {
        LOG_ERROR("new network failed!");
        return false;
    }

    if (!m_net->create(&m_node_info.addr_info, this, &m_worker_data_mgr)) {
        LOG_ERROR("init network fail!");
        return false;
    }

    LOG_INFO("init network done!");
    return true;
}

void Manager::on_terminated(ev_signal* s) {
    if (s == nullptr) return;

    LOG_WARNING("%s terminated by signal %d!",
                m_json_conf("server_name").c_str(), s->signum);
    SAFE_DELETE(s);
    destory();
    exit(s->signum);
}

void Manager::on_child_terminated(ev_signal* s) {
    pid_t pid;
    int status, res;

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
    return true;
}

void Manager::create_workers() {
    for (int i = 0; i < m_node_info.worker_processes; i++) {
        int pid, data_fds[2], ctrl_fds[2];

        if (socketpair(PF_UNIX, SOCK_STREAM, 0, ctrl_fds) < 0) {
            LOG_ERROR("create socket pair failed! %d: %s", errno, strerror(errno));
            continue;
        }

        if (socketpair(PF_UNIX, SOCK_STREAM, 0, data_fds) < 0) {
            LOG_ERROR("create socket pair failed! %d: %s", errno, strerror(errno));
            m_net->close_chanel(ctrl_fds);
            continue;
        }

        if ((pid = fork()) == 0) {  // child
            m_net->end_ev_loop();
            m_net->close_fds();
            close(ctrl_fds[0]);
            close(data_fds[0]);
            anet_no_block(NULL, ctrl_fds[1]);
            anet_no_block(NULL, data_fds[1]);

            worker_info_t info;
            info.work_path = m_node_info.work_path;
            info.ctrl_fd = ctrl_fds[1];
            info.data_fd = data_fds[1];
            info.index = i;

            char name[64] = {0};
            snprintf(name, sizeof(name), "%s_w_%d",
                     m_json_conf("server_name").c_str(), i);

            Worker worker(m_logger, name);
            if (!worker.init(&info)) {
                exit(EXIT_CHILD_INIT_FAIL);
            }
            worker.run();
            LOG_INFO("child exit! index: %d", i);
            exit(EXIT_CHILD);
        } else if (pid > 0) {  // parent
            close(ctrl_fds[1]);
            close(data_fds[1]);
            anet_no_block(NULL, ctrl_fds[0]);
            anet_no_block(NULL, data_fds[0]);
            m_net->add_chanel_event(ctrl_fds[0]);
            m_net->add_chanel_event(data_fds[0]);

            m_worker_data_mgr.add_worker_info(i, pid, ctrl_fds[0], data_fds[0]);
            LOG_INFO("manager ctrl_fd: %d, data_fd: %d", ctrl_fds[0], data_fds[0]);
        } else {
            m_net->close_chanel(data_fds);
            m_net->close_chanel(ctrl_fds);
            LOG_ERROR("error: %d, %s", errno, strerror(errno));
        }
    }
}

}  // namespace kim