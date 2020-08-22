#ifndef __KIM_DB_CONNECTION_H__
#define __KIM_DB_CONNECTION_H__

#include <ev.h>
#include <mysql.h>

#include <iostream>
#include <list>

#include "server.h"
#include "util/json/CJsonObject.hpp"
#include "util/log.h"

namespace kim {

/*
    redisLibevAttach(m_ev_loop, c);
    redisAsyncSetConnectCallback(c, on_redis_connect);
    redisAsyncSetDisconnectCallback(c, on_redis_disconnect); 

    static void on_redis_connect(const redisAsyncContext* c, int status);
    static void on_redis_disconnect(const redisAsyncContext* c, int status);
    static void on_redis_callback(redisAsyncContext* c, void* reply, void* privdata);
 */

class MysqlAsyncConn;
typedef struct sql_task_s sql_task_t;

/* mysql callback fn. */
typedef void(MysqlExecCallbackFn)(const MysqlAsyncConn*, sql_task_t* task);
typedef void(MysqlQueryCallbackFn)(const MysqlAsyncConn*, sql_task_t* task, MYSQL_RES* res);

typedef struct sql_task_s {
    enum class OPERATE {
        SELECT,
        EXEC,
    };
    std::string sql;
    OPERATE oper = OPERATE::SELECT;
    MysqlExecCallbackFn* fn_exec = nullptr;
    MysqlQueryCallbackFn* fn_query = nullptr;
    void* privdata = nullptr;
    int error = 0;
    std::string errstr;
} sql_task_t;

/////////////////////////////////////////////

typedef struct db_info_s {
    int port = 0;
    int max_conn_cnt = 0;
    std::string host, db_name, password, charset, user;
} db_info_t;

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

    // mysql async api.
    bool connect_start();
    bool connect_wait(struct ev_loop* loop, ev_io* w, int event);
    bool check_error_reconnect(const sql_task_t* task);

    bool query_start();
    bool query_wait(struct ev_loop* loop, ev_io* w, int event);

    int exec_sql_start();
    bool exec_sql_wait(struct ev_loop* loop, ev_io* w, int event);

    int store_result_start();
    bool store_result_wait(struct ev_loop* loop, ev_io* w, int event);

    // sql task.
    int wait_next_task(int mysql_status = MYSQL_WAIT_WRITE);
    int task_size() const { return m_tasks.size() + (m_cur_task ? 1 : 0); }
    sql_task_t* fetch_next_task();
    void stop_task();

   private:
    Log* m_logger = nullptr;
    struct ev_loop* m_loop = nullptr;
    db_info_t* m_db_info = nullptr;

    MYSQL m_mysql;
    MYSQL_RES* m_query_res = nullptr;
    STATE m_state = STATE::NO_CONNECTED;
    ev_io m_watcher;
    std::list<sql_task_t*> m_tasks;
    sql_task_t* m_cur_task = nullptr;
};

}  // namespace kim

#endif  //__KIM_DB_CONNECTION_H__