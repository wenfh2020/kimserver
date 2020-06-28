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

Manager::Manager(Log* logger) : m_logger(logger), m_net(NULL) {
}

Manager::~Manager() {
    destory();
}

void Manager::destory() {
    if (m_net != NULL) {
        m_net->end_ev_loop();
        SAFE_DELETE(m_net);
    }

    std::map<int, worker_info_t*>::iterator itr = m_pid_worker_info.begin();
    for (; itr != m_pid_worker_info.end(); itr++) {
        worker_info_t* info = itr->second;
        if (info != NULL) {
            if (info->ctrl_fd != -1) close(info->ctrl_fd);
            if (info->data_fd != -1) close(info->data_fd);
            SAFE_DELETE(info);
        }
    }
}

void Manager::run() {
    if (m_net == NULL) {
        LOG_CRIT("create network failed!");
        return;
    }

    m_net->run();
    LOG_INFO("server is running!");
}

bool Manager::init(const char* conf_path) {
    if (!load_config(conf_path)) {
        LOG_ERROR("init conf fail! %s", conf_path);
        return false;
    }

    set_proc_title("%s", m_json_conf("server_name").c_str());

    if (!init_logger()) {
        LOG_ERROR("init log fail!");
        return false;
    }

    if (!create_network()) {
        LOG_ERROR("init events fail!");
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
    if (f == NULL) {
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
        char work_path[MAX_PATH];
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
    m_net = new Network(m_logger);
    if (m_net == NULL) {
        LOG_ERROR("new network failed!");
        return false;
    }

    if (!m_net->create(&m_node_info.addr_info, this)) {
        LOG_ERROR("init network fail!");
        return false;
    }

    LOG_INFO("init network done!");
    return true;
}

void Manager::on_terminated(struct ev_signal* s) {
    if (s == NULL) return;

    LOG_WARNING("%s terminated by signal %d!",
                m_json_conf("server_name").c_str(), s->signum);
    SAFE_DELETE(s);
    destory();
    exit(s->signum);
}

void Manager::on_child_terminated(struct ev_signal* watcher) {
}

void Manager::create_workers() {
    int pid = 0, sum = 0;

    for (int i = 0; i < m_node_info.worker_processes; i++) {
        int data_fds[2], ctrl_fds[2];

        if (socketpair(PF_UNIX, SOCK_STREAM, 0, ctrl_fds) < 0) {
            LOG_ERROR("create socket pair failed! %d: %s", errno, strerror(errno));
            continue;
        }

        if (socketpair(PF_UNIX, SOCK_STREAM, 0, data_fds) < 0) {
            LOG_ERROR("create socket pair failed! %d: %s", errno, strerror(errno));
            m_net->close_chanel(ctrl_fds);
            continue;
        }

        if ((pid = fork()) == 0) {
            // child
            m_net->close_listen_sockets();
            close(ctrl_fds[0]);
            close(data_fds[0]);
            anet_no_block(NULL, ctrl_fds[1]);
            anet_no_block(NULL, data_fds[1]);

            worker_info_t info;
            info.work_path = m_node_info.work_path;
            info.ctrl_fd = ctrl_fds[1];
            info.data_fd = data_fds[1];
            info.worker_idx = i;

            Worker worker(&info);
            if (!worker.init(m_logger, m_json_conf("server_name"))) {
                exit(EXIT_CHILD_INIT_FAIL);
            }
            worker.run();

            LOG_INFO("child exit! pid: %d, index: %d", getpid(), i);
            exit(EXIT_CHILD);
        } else if (pid > 0) {
            sum++;
            // parent
            close(ctrl_fds[1]);
            close(data_fds[1]);
            anet_no_block(NULL, ctrl_fds[0]);
            anet_no_block(NULL, data_fds[0]);

            worker_info_t* info = new worker_info_t;
            info->worker_idx = i;
            info->ctrl_fd = ctrl_fds[0];
            info->data_fd = data_fds[0];
            info->work_path = m_node_info.work_path;
            m_pid_worker_info[pid] = info;

            m_chanel_fd_pid[ctrl_fds[0]] = pid;
            m_chanel_fd_pid[data_fds[0]] = pid;

            // m_events->add_chanel_event(ctrl_fds[0]);
            // m_events->add_chanel_event(data_fds[0]);
        } else {
            LOG_ERROR("error: %d, %s", errno, strerror(errno));
        }
    }

    LOG_INFO("fork process count: %d", sum);
}

}  // namespace kim