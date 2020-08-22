#ifndef __KIM_DB_MGR_H__
#define __KIM_DB_MGR_H__

#include <iostream>
#include <list>

#include "mysql_async_conn.h"
#include "server.h"
#include "util/json/CJsonObject.hpp"
#include "util/log.h"

namespace kim {

class DBMgr {
   public:
    DBMgr(Log* logger, struct ev_loop* loop);
    virtual ~DBMgr();

    bool init(CJsonObject& config);
    bool sql_exec(const std::string& node, MysqlExecCallbackFn* fn, const std::string& sql, void* privdata = nullptr);
    bool sql_query(const std::string& node, MysqlQueryCallbackFn* fn, const std::string& sql, void* privdata = nullptr);

   private:
    void destory_db_infos();
    bool check_query_sql(const std::string& sql);
    MysqlAsyncConn* get_conn(const std::string& node);
    MysqlAsyncConn* async_connect(const db_info_t* db_info);
    std::string format_identity(const std::string& host, int port, const std::string& db_name);

   private:
    Log* m_logger = nullptr;
    struct ev_loop* m_loop = nullptr;
    typedef std::pair<std::list<MysqlAsyncConn*>::iterator, std::list<MysqlAsyncConn*>> MysqlConnPair;
    std::unordered_map<std::string, MysqlConnPair> m_conns;
    std::unordered_map<std::string, db_info_t*> m_dbs;
};

}  // namespace kim

#endif  //__KIM_DB_MGR_H__