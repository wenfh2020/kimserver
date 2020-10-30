#include "db_mgr.h"

#include <vector>

#define DEF_CONN_CNT 10
#define MAX_CONN_CNT 30

namespace kim {

DBMgr::DBMgr(Log* logger, struct ev_loop* loop)
    : m_logger(logger), m_loop(loop) {
}

DBMgr::~DBMgr() {
    destory_db_infos();
    for (auto& it : m_conns) {
        auto& list = it.second.second;
        for (auto& itr : list) {
            SAFE_DELETE(itr);
        }
    }
    m_conns.clear();
}

void DBMgr::destory_db_infos() {
    for (auto& it : m_dbs) {
        SAFE_DELETE(it.second);
    }
    m_dbs.clear();
}

bool DBMgr::init(CJsonObject& config) {
    std::vector<std::string> vec;
    config.GetKeys(vec);

    for (const auto& it : vec) {
        const CJsonObject& obj = config[it];
        db_info_t* info = new db_info_t;

        info->host = obj("host");
        info->db_name = obj("name").empty() ? "mysql" : obj("name");
        info->password = obj("password");
        info->charset = obj("charset");
        info->user = obj("user");
        info->port = std::stoi(obj("port"));
        info->max_conn_cnt = std::stoi(obj("max_conn_cnt"));

        if (info->max_conn_cnt == 0) {
            info->max_conn_cnt = DEF_CONN_CNT;
        } else {
            if (info->max_conn_cnt > MAX_CONN_CNT) {
                LOG_WARN("db max conn count is too large! cnt: %d", info->max_conn_cnt);
                info->max_conn_cnt = MAX_CONN_CNT;
            }
        }
        LOG_DEBUG("max client cnt: %d", info->max_conn_cnt);

        if (info->host.empty() || info->port == 0 ||
            info->password.empty() || info->charset.empty() || info->user.empty()) {
            LOG_ERROR("invalid db node info: %s", it.c_str());
            SAFE_DELETE(info);
            destory_db_infos();
            return false;
        }

        m_dbs.insert({it, info});
    }

    return true;
}

MysqlAsyncConn* DBMgr::get_conn(const std::string& node) {
    auto itr = m_dbs.find(node);
    if (itr == m_dbs.end()) {
        LOG_ERROR("invalid db node: %s!", node.c_str());
        return nullptr;
    }

    MysqlAsyncConn* c = nullptr;
    db_info_t* db_info = itr->second;
    std::string identity = format_identity(
        db_info->host, db_info->port, db_info->db_name);

    // get connection.
    auto it = m_conns.find(identity);
    if (it == m_conns.end()) {
        c = async_connect(db_info);
        if (c == nullptr) {
            LOG_ERROR("async db connect failed! %s:%d",
                      db_info->host.c_str(), db_info->port);
            return nullptr;
        }
        std::list<MysqlAsyncConn*> conns({c});
        m_conns.insert({identity, {conns.begin(), conns}});
        MysqlConnPair& pair = m_conns.begin()->second;
        pair.first = pair.second.begin();
    } else {
        auto& list = it->second.second;
        auto& list_itr = it->second.first;
        if ((int)list.size() < db_info->max_conn_cnt) {
            c = async_connect(db_info);
            if (c == nullptr) {
                LOG_ERROR("async db connect failed! %s:%d",
                          db_info->host.c_str(), db_info->port);
                return nullptr;
            }
            list.push_back(c);
        } else {
            if (++list_itr == list.end()) {
                list_itr = list.begin();
            }
            c = *list_itr;
            if (!c->is_connected()) {
                return nullptr;
            }
        }
    }

    return c;
}

bool DBMgr::handle_sql(bool is_query, const char* node,
                       void* fn, const char* sql, void* privdata) {
    if (node == nullptr || fn == nullptr || sql == nullptr) {
        LOG_ERROR("invalid params!");
        return false;
    }

    if (is_query && !check_query_sql(sql)) {
        LOG_ERROR("invalid query sql: %s", sql);
        return false;
    }

    MysqlAsyncConn* c = get_conn(node);
    if (c == nullptr) {
        LOG_ERROR("get db conn failed! node: %s", node);
        return false;
    }

    sql_task_t* task = new sql_task_t;
    if (is_query) {
        task->oper = sql_task_t::OPERATE::SELECT;
        task->fn_query = (MysqlQueryCallbackFn*)fn;
    } else {
        task->oper = sql_task_t::OPERATE::EXEC;
        task->fn_exec = (MysqlExecCallbackFn*)fn;
    }
    task->sql = sql;
    task->privdata = privdata;
    c->add_task(task);
    return true;
}

bool DBMgr::async_exec(const char* node, MysqlExecCallbackFn* fn,
                       const char* sql, void* privdata) {
    return handle_sql(false, node, (void*)fn, sql, privdata);
}

bool DBMgr::check_query_sql(const std::string& sql) {
    // split the first world.
    std::stringstream ss(sql);
    std::string oper;
    ss >> oper;

    // find the select's word.
    const char* select[] = {"select", "show", "explain", "desc"};
    for (int i = 0; i < 4; i++) {
        if (!strcasecmp(oper.c_str(), select[i])) {
            return true;
        }
    }
    return false;
}

bool DBMgr::async_query(const char* node, MysqlQueryCallbackFn* fn,
                        const char* sql, void* privdata) {
    return handle_sql(true, node, (void*)fn, sql, privdata);
}

std::string
DBMgr::format_identity(const std::string& host, int port, const std::string& db_name) {
    char identity[64];
    snprintf(identity, sizeof(identity), "%s:%d:%s", host.c_str(), port, db_name.c_str());
    return std::string(identity);
}

MysqlAsyncConn* DBMgr::async_connect(const db_info_t* db_info) {
    MysqlAsyncConn* c = new MysqlAsyncConn(m_logger);
    if (c == nullptr) {
        LOG_ERROR("alloc db connection connect failed! %s:%d",
                  db_info->host.c_str(), db_info->port);
        return nullptr;
    }
    /* {"database":{"test":{"host":"127.0.0.1","port":3306,"user":"root",
               "password":"1234567","charset":"utf8mb4","connection_count":10}}} */
    if (!c->init(db_info, m_loop)) {
        LOG_ERROR("init connection connect failed! %s:%d",
                  db_info->host.c_str(), db_info->port);
        SAFE_DELETE(c);
        return nullptr;
    }
    return c;
}

}  // namespace kim
