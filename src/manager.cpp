#include "manager.h"

#include <fstream>
#include <iosfwd>
#include <sstream>

#include "util/set_proc_title.h"

namespace kim {

Manager::Manager() : m_logger(NULL), m_is_config_loaded(false) {
    m_logger = new kim::Log;
}

Manager::~Manager() {
    SAFE_DELETE(m_logger);
}

void Manager::run() {
    LOG_INFO("%s", "run server!");
    sleep(1000);
}

bool Manager::init(const char* conf_path) {
    if (access(conf_path, R_OK) == -1) {
        LOG_ERROR("no config file!");
        return false;
    }

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

    m_is_config_loaded = true;

    if (!init_logger()) {
        LOG_ERROR("init log fail!");
        return false;
    }

    set_proc_title("%s", m_cur_json_conf("server_name").c_str());

    LOG_INFO("init log success!");
    return true;
}

bool Manager::init_logger() {
    if (!m_is_config_loaded) {
        return false;
    }

    char log_path[MAX_PATH] = {0};
    snprintf(log_path, sizeof(log_path), "%s/%s",
             m_work_path.c_str(), m_cur_json_conf("log_path").c_str());

    FILE* f;
    f = fopen(log_path, "a");
    if (f == NULL) {
        LOG_ERROR("cant not open log file: %s", log_path);
        return false;
    }
    fclose(f);

    m_logger->set_log_path(log_path);
    m_logger->set_level(m_cur_json_conf("log_level").c_str());
    return true;
}

bool Manager::load_config(const char* path) {
    std::ifstream fin(path);
    if (!fin.good()) {
        return false;
    }

    util::CJsonObject conf;
    std::stringstream content;
    content << fin.rdbuf();
    if (!conf.Parse(content.str())) {
        fin.close();
        return false;
    }
    fin.close();

    m_conf_path = path;
    m_cur_json_conf = conf;
    m_old_json_conf = m_cur_json_conf;
    return true;
}

}  // namespace kim