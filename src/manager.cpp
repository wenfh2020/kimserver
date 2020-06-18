#include "manager.h"

#include <fstream>
#include <iosfwd>
#include <sstream>

#include "util/set_proc_title.h"

namespace kim {

Manager::Manager() : m_logger(NULL), m_is_config_loaded(false) {
}

Manager::~Manager() {
}

void Manager::run() {
    LOG_DEBUG("%s", "server running!");
}

bool Manager::init(const char* conf_path, kim::Log* logger) {
    if (access(conf_path, R_OK) == -1) {
        LOG_ERROR("no config file!");
        return false;
    }

    if (logger == NULL) {
        return false;
    }

    m_logger = logger;

    if (m_work_path.empty()) {
        char file_path[MAX_PATH] = {0};
        if (!getcwd(file_path, sizeof(file_path))) {
            LOG_ERROR("cant not get work path!");
            return false;
        }
        m_work_path = file_path;
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

    if (!m_events.init()) {
        LOG_ERROR("init events fail!");
        return false;
    }
    m_events.set_logger(m_logger);

    LOG_INFO("init success!");
    return true;
}

bool Manager::init_logger() {
    if (!m_is_config_loaded) {
        return false;
    }

    char log_path[MAX_PATH] = {0};
    snprintf(log_path, sizeof(log_path), "%s/%s",
             m_work_path.c_str(), m_json_conf("log_path").c_str());

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
    std::ifstream fin(path);
    if (!fin.good()) {
        return false;
    }

    util::CJsonObject json_conf;
    std::stringstream content;
    content << fin.rdbuf();
    if (!json_conf.Parse(content.str())) {
        fin.close();
        return false;
    }
    fin.close();

    m_conf_path = path;
    m_old_json_conf = m_json_conf;
    m_json_conf = json_conf;
    m_is_config_loaded = true;

    if (m_old_json_conf.ToString() != m_json_conf.ToString()) {
        if (m_old_json_conf.ToString().empty()) {
            m_json_conf.Get("node_type", m_node_info.node_type);
            m_json_conf.Get("host", m_node_info.host);
            m_json_conf.Get("port", m_node_info.port);
            m_json_conf.Get("gate_host", m_node_info.gate_host);
            m_json_conf.Get("gate_port", m_node_info.gate_port);
        }
    }
    return true;
}

}  // namespace kim