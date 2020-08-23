#ifndef __KIM_DB_CONNECTION_H__
#define __KIM_DB_CONNECTION_H__

#include <ev.h>
#include <mysql.h>

#include <iostream>
#include <list>

#include "mysql_result.h"
#include "mysql_task.h"
#include "server.h"
#include "util/json/CJsonObject.hpp"
#include "util/log.h"

namespace kim {

class MysqlAsyncConn {
   public:
    enum class STATE {
        NO_CONNECTED = 0,
        CONNECT_WAITING,
        WAIT_OPERATE,
        QUERY_WAITING,
        EXECSQL_WAITING,
        STORE_WAITING
    };

    MysqlAsyncConn(Log* logger);
    virtual ~MysqlAsyncConn();
    bool init(const db_info_t* db_info, struct ev_loop* loop);
    void add_task(sql_task_t* task);

   private:
    bool set_db_info(const db_info_t* db_info);

    // events.
    void set_state(STATE state) { m_state = state; }
    void active_ev_io(int mysql_status);
    int mysql_status_to_libev_event(int status);
    int libev_event_to_mysql_status(int event);
    bool wait_for_mysql(struct ev_loop* loop, ev_io* w, int event);
    static void libev_io_cb(struct ev_loop* loop, ev_io* w, int event);
    void callback(bool is_query);

    // mysql async api.
    bool connect_start();
    bool connect_wait(struct ev_loop* loop, ev_io* w, int event);
    bool check_error_reconnect(const sql_task_t* task);

    bool query_start();
    bool query_wait(struct ev_loop* loop, ev_io* w, int event);

    bool exec_sql_start();
    bool exec_sql_wait(struct ev_loop* loop, ev_io* w, int event);

    int store_result_start();
    bool store_result_wait(struct ev_loop* loop, ev_io* w, int event);

    // sql task.
    bool wait_next_task(int mysql_status = MYSQL_WAIT_WRITE);
    int task_size() const { return m_tasks.size() + (m_cur_task ? 1 : 0); }
    sql_task_t* fetch_next_task();
    void stop_task();

   private:
    Log* m_logger = nullptr;
    ev_io m_watcher;
    struct ev_loop* m_loop = nullptr;

    db_info_t* m_db_info = nullptr;
    MYSQL m_mysql;
    MYSQL_RES* m_query_res = nullptr;
    STATE m_state = STATE::NO_CONNECTED;
    std::list<sql_task_t*> m_tasks;
    sql_task_t* m_cur_task = nullptr;
    MysqlResult m_mysql_result;
};

}  // namespace kim

#endif  //__KIM_DB_CONNECTION_H__