#include "manager.h"

#include <fstream>
#include <iosfwd>
#include <sstream>

#include "util/set_proc_title.h"

namespace kim {

Manager::Manager() : m_logger(NULL), m_is_config_loaded(false) {
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
        return false;
    }

    if (m_work_path.empty()) {
        char file_path[MAX_PATH] = {0};
        if (!getcwd(file_path, sizeof(file_path))) {
            return false;
        }
        m_work_path = file_path;
    }

    if (!load_config(conf_path)) {
        std::cerr << "init conf fail!" << std::endl;
        return false;
    }

    m_is_config_loaded = true;

    if (!init_logger()) {
        std::cerr << "init logger fail!" << std::endl;
        return false;
    }

    set_proc_title("%s", m_cur_json_conf("server_name").c_str());

    LOG_INFO("init log success!");
    return true;
}

bool Manager::init_logger() {
    if (!m_is_config_loaded) return false;

    char log_path[MAX_PATH] = {0};
    snprintf(log_path, sizeof(log_path), "%s/%s",
             m_work_path.c_str(), m_cur_json_conf("log_path").c_str());

    FILE* f;
    f = fopen(log_path, "a");
    if (f == NULL) {
        std::cerr << "cant not open log file!" << std::endl;
        return false;
    }
    fclose(f);

    int ll = 0;
    std::string level = m_cur_json_conf("log_level");

    if (level.compare("DEBUG") == 0) {
        ll = kim::Log::LL_DEBUG;
    } else if (level.compare("INFO") == 0) {
        ll = kim::Log::LL_INFO;
    } else if (level.compare("NOTICE") == 0) {
        ll = kim::Log::LL_NOTICE;
    } else if (level.compare("WARNING") == 0) {
        ll = kim::Log::LL_WARNING;
    } else if (level.compare("ERR") == 0) {
        ll = kim::Log::LL_ERR;
    } else if (level.compare("CRIT") == 0) {
        ll = kim::Log::LL_CRIT;
    } else if (level.compare("ALERT") == 0) {
        ll = kim::Log::LL_ALERT;
    } else if (level.compare("EMERG") == 0) {
        ll = kim::Log::LL_EMERG;
    } else {
        std::cerr << "invalid log level!" << std::endl;
        return false;
    }

    m_logger = new kim::Log(log_path);
    m_logger->set_level(ll);
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