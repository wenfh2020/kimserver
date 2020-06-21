#include "manager.h"

#include <sys/socket.h>

#include <fstream>
#include <iosfwd>
#include <sstream>

#include "net/anet.h"
#include "util/set_proc_title.h"
#include "worker.h"

namespace kim {

Manager::Manager(Log* logger)
    : m_logger(logger), m_events(NULL), m_network(NULL) {
    m_fds.clear();
}

Manager::~Manager() {
    destory();
}

void Manager::destory() {
    close_listen_sockets();
    SAFE_DELETE(m_events);
    SAFE_DELETE(m_network);
}

void Manager::run() {
    if (m_events != NULL) {
        LOG_INFO("%s", "server is running!");
        m_events->run();
    }
}

bool Manager::init(const char* conf_path) {
    if (m_node_info.work_path.empty()) {
        char file_path[MAX_PATH] = {0};
        if (!getcwd(file_path, sizeof(file_path))) {
            LOG_ERROR("cant not get work path!");
            return false;
        }
        m_node_info.work_path = file_path;
    }

    if (!load_config(conf_path)) {
        LOG_ERROR("init conf fail! %s", conf_path);
        return false;
    }

    set_proc_title("%s", m_json_conf("server_name").c_str());

    if (!init_logger()) {
        LOG_ERROR("init log fail!");
        return false;
    }

    if (!init_events()) {
        LOG_ERROR("init events fail!");
        return false;
    }

    if (!init_network()) {
        LOG_ERROR("init network fail!");
        return false;
    }

    create_workers();
    LOG_INFO("init success!");
    return true;
}

bool Manager::init_logger() {
    char log_path[MAX_PATH] = {0};
    snprintf(log_path, sizeof(log_path), "%s/%s",
             m_node_info.work_path.c_str(), m_json_conf("log_path").c_str());

    FILE* f;
    f = fopen(log_path, "a");
    if (f == NULL) {
        LOG_ERROR("cant not open log file: %s", log_path);
        return false;
    }
    fclose(f);

    m_logger->set_log_path(log_path);
    m_logger->set_level(m_json_conf("log_level").c_str());
    return true;
}

bool Manager::load_config(const char* path) {
    if (access(path, R_OK) == -1) {
        LOG_ERROR("no config file!");
        return false;
    }

    std::ifstream fin(path);
    if (!fin.good()) {
        return false;
    }

    CJsonObject json_conf;
    std::stringstream content;
    content << fin.rdbuf();
    if (!json_conf.Parse(content.str())) {
        fin.close();
        return false;
    }
    fin.close();

    m_node_info.conf_path = path;
    m_old_json_conf = m_json_conf;
    m_json_conf = json_conf;

    if (m_old_json_conf.ToString() != m_json_conf.ToString()) {
        if (m_old_json_conf.ToString().empty()) {
            m_node_info.worker_num =
                strtoul(m_json_conf("process_num").c_str(), NULL, 10);
            m_json_conf.Get("node_type", m_node_info.node_type);
            m_json_conf.Get("bind", m_node_info.addr_info.bind);
            m_json_conf.Get("port", m_node_info.addr_info.port);
            m_json_conf.Get("gate_bind", m_node_info.addr_info.gate_port);
            m_json_conf.Get("gate_port", m_node_info.addr_info.gate_port);
        }
    }
    return true;
}

bool Manager::init_events() {
    m_events = new Events(m_logger);
    if (!m_events->create()) {
        LOG_ERROR("init events fail!");
        return false;
    }
    m_events->set_cb_terminated(&on_terminated);
    m_events->set_cb_child_terminated(&on_child_terminated);

    LOG_INFO("init events done!");
    return true;
}

bool Manager::init_network() {
    m_network = new Network(m_logger);
    if (NULL == m_network) return false;

    if (!m_network->create(&m_node_info.addr_info, m_fds)) {
        LOG_ERROR("create network failed!");
        return false;
    }

    LOG_INFO("init network done!");
    return true;
}

void Manager::on_terminated(struct ev_signal* s) {
    if (NULL == s) return;

    // LOG_WARNING("%s terminated by signal %d!",
    //             m_json_conf("server_name").c_str(), s->signum);
    exit(s->signum);
}

void Manager::on_child_terminated(struct ev_signal* watcher) {
}

void Manager::create_workers() {
    int pid = 0;

    for (int i = 0; i < m_node_info.worker_num; i++) {
        int data_fds[2];
        int ctrl_fds[2];

        if (socketpair(PF_UNIX, SOCK_STREAM, 0, ctrl_fds) < 0) {
            LOG_ERROR("create socket pair failed! %d: %s", errno, strerror(errno));
        }

        if (socketpair(PF_UNIX, SOCK_STREAM, 0, data_fds) < 0) {
            LOG_ERROR("create socket pair failed! %d: %s", errno, strerror(errno));
        }

        if ((pid = fork()) == 0) {
            close_listen_sockets();
            close(ctrl_fds[0]);
            close(data_fds[0]);
            anet_no_block(NULL, ctrl_fds[1]);
            anet_no_block(NULL, data_fds[1]);

            worker_info_s work_info;
            work_info.work_path = m_node_info.work_path;
            work_info.ctrl_fd = ctrl_fds[1];
            work_info.data_fd = data_fds[1];
            work_info.worker_idx = i;

            Worker worker(&work_info);
            if (!worker.init(m_logger, m_json_conf("server_name"))) {
                exit(3);
            }
            worker.run();
            exit(-2);
        } else if (pid > 0) {
            close(ctrl_fds[1]);
            close(data_fds[1]);
            anet_no_block(NULL, ctrl_fds[0]);
            anet_no_block(NULL, data_fds[0]);
        } else {
            LOG_ERROR("error %d: %s", errno, strerror(errno));
        }
    }
}

void Manager::close_listen_sockets() {
    std::list<int>::iterator itr = m_fds.begin();
    for (; itr != m_fds.end(); itr++) {
        if (*itr != -1) close(*itr);
    }
}

}  // namespace kim