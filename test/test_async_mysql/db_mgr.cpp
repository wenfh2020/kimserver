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
        info->port = atoi(obj("port").c_str());
        info->max_conn_cnt = atoi(obj("max_conn_cnt").c_str());

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
    LOG_DEBUG("dsfdsfds");
    auto itr = m_dbs.find(node);
    if (itr == m_dbs.end()) {
        LOG_ERROR("invalid db node : %s!", node.c_str());
        return nullptr;
    }

    db_info_t* db_info = itr->second;
    std::string identity = format_identity(
        db_info->host, db_info->port, db_info->db_name);
    MysqlAsyncConn* c = nullptr;
    LOG_DEBUG("connection identity: %s", identity.c_str());

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
        LOG_DEBUG("list_itr %p, %p", conns.begin(), pair.first);
    } else {
        auto& list = it->second.second;
        auto& list_itr = it->second.first;
        if (list.size() < db_info->max_conn_cnt) {
            c = async_connect(db_info);
            if (c == nullptr) {
                LOG_ERROR("async db connect failed! %s:%d",
                          db_info->host.c_str(), db_info->port);
                return nullptr;
            }
            list.push_back(c);
            list_itr++;
            LOG_DEBUG("list_itr1 %p", list_itr);
        } else {
            if (++list_itr == list.end()) {
                list_itr = list.begin();
            }
            LOG_DEBUG("list_itr2 %p", list_itr);
            c = *list_itr;
        }
    }

    return c;
}

bool DBMgr::sql_exec(const std::string& node,
                     MysqlExecCallbackFn* fn, const std::string& sql, void* privdata) {
    MysqlAsyncConn* c = get_conn(node);
    if (c == nullptr) {
        LOG_ERROR("get conn failed! node: %s", node.c_str());
        return false;
    }
    sql_task_t* task = new sql_task_t;
    task->oper = sql_task_t::OPERATE::EXEC;
    task->sql = sql;
    task->fn_exec = fn;
    task->privdata = privdata;
    c->add_task(task);
    return true;
}

bool DBMgr::check_query_sql(const std::string& sql) {
    // split the first world.
    std::stringstream ss(sql);
    std::string oper;
    ss >> oper;
    LOG_DEBUG("query sql, oper: %s", oper.c_str());

    // find the select's word.
    const char* select[] = {"select", "show", "explain", "desc"};
    for (int i = 0; i < 4; i++) {
        if (!strcasecmp(oper.c_str(), select[i])) {
            return true;
        }
    }
    return false;
}

bool DBMgr::sql_query(const std::string& node,
                      MysqlQueryCallbackFn* fn, const std::string& sql, void* privdata) {
    if (!check_query_sql(sql)) {
        LOG_ERROR("invalid query sql: %s", sql.c_str());
        return false;
    }

    MysqlAsyncConn* c = get_conn(node);
    if (c == nullptr) {
        LOG_ERROR("get conn failed! node: %s", node.c_str());
        return false;
    }
    sql_task_t* task = new sql_task_t;
    task->oper = sql_task_t::OPERATE::SELECT;
    task->sql = sql;
    task->fn_query = fn;
    task->privdata = privdata;
    c->add_task(task);
    return true;
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
