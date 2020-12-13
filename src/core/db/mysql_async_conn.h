/* 
 * document:
 * Using the Non-blocking Library:
 * https://mariadb.com/kb/en/using-the-non-blocking-library/
 *
 * Non-blocking API Reference:
 * https://mariadb.com/kb/en/non-blocking-api-reference/
 */

#ifndef __KIM_DB_CONNECTION_H__
#define __KIM_DB_CONNECTION_H__

#include <ev.h>
#include <mysql.h>

#include <iostream>
#include <list>

#include "../server.h"
#include "../util/json/CJsonObject.hpp"
#include "../util/log.h"
#include "mysql_result.h"
#include "mysql_task.h"

namespace kim {

class MysqlAsyncConn {
   public:
    enum class STATE {
        NO_CONNECTED = 0,
        CONNECT_WAITING,
        WAIT_OPERATE,
        QUERY_WAITING,
        EXECSQL_WAITING,
        STORE_WAITING,
        PING_WAITING
    };

    MysqlAsyncConn(Log* logger);
    virtual ~MysqlAsyncConn();
    bool init(const db_info_t* db_info, struct ev_loop* loop);
    bool add_task(sql_task_t* task);
    bool is_connected() { return m_is_connected; }

   private:
    bool set_db_info(const db_info_t* db_info);
    void set_connected(bool ok);

    // events.
    void set_state(STATE state) { m_state = state; }
    void active_ev_io(int mysql_status);
    int mysql_status_to_libev_event(int status);
    int libev_event_to_mysql_status(int event);
    void wait_for_mysql(struct ev_loop* loop, ev_io* w, int event);
    static void libev_io_cb(struct ev_loop* loop, ev_io* w, int event);
    static void libev_timer_cb(struct ev_loop* loop, ev_timer* w, int revents);
    void on_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);

    // mysql async api.
    void connect_start();
    void connect_wait(struct ev_loop* loop, ev_io* w, int event);

    void query_start();
    void query_wait(struct ev_loop* loop, ev_io* w, int event);

    void exec_sql_start();
    void exec_sql_wait(struct ev_loop* loop, ev_io* w, int event);

    void store_result_start();
    void store_result_wait(struct ev_loop* loop, ev_io* w, int event);

    void ping_start();
    void ping_wait(struct ev_loop* loop, ev_io* w, int event);

    void operate_wait();

    // sql task.
    bool wait_next_task(int mysql_status = MYSQL_WAIT_WRITE);
    bool is_task_empty() { return (m_tasks.empty() && m_cur_task == nullptr); }
    sql_task_t* fetch_next_task();
    void callback(sql_task_t* task);
    void stop_task();
    void handle_task();
    void clear_tasks();

    // timer
    bool load_timer();

   private:
    Log* m_logger = nullptr;

    ev_io m_watcher;
    ev_timer m_timer;
    bool m_reading = false;
    bool m_writing = false;
    struct ev_loop* m_loop = nullptr;

    bool m_is_connected = true;
    int m_reconnect_cnt = 0;
    db_info_t* m_db_info = nullptr;

    MYSQL m_mysql;
    MYSQL_RES* m_query_res = nullptr;
    STATE m_state = STATE::NO_CONNECTED;
    MysqlResult m_mysql_result;

    std::list<sql_task_t*> m_tasks;
    sql_task_t* m_cur_task = nullptr;
};

}  // namespace kim

#endif  //__KIM_DB_CONNECTION_H__