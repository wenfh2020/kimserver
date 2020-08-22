// g++ -std='c++11' -g -Wno-noexcept-type test_async_mysql.cpp -o amysql -lev && ./amysql

#include <ctype.h>
#include <ev.h>

#include <iostream>

#include "db_mgr.h"
#include "server.h"
#include "util/json/CJsonObject.hpp"
#include "util/util.h"

ev_timer timer;
const char* g_userdata = "123456";
#define MAX_CALLBACK_CNT 100
int callback_cnt = 0;
double begin_time, end_time;

class MysqlAsyncConn;
class DBConnMgr;

// test api.
// close task. leak.

/*

CREATE DATABASE IF NOT EXISTS mytest DEFAULT CHARSET utf8mb4 COLLATE utf8mb4_general_ci

CREATE TABLE `test_async_mysql` (
    `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
    `value` varchar(32) NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8mb4; 
 */

static void timeout_cb(EV_P_ ev_timer* w, int revents) {
    std::cout << "timer hit. data: "
              << static_cast<char*>(w->data)
              << std::endl;
}

static void mysql_exec_callback(const kim::MysqlAsyncConn*, kim::sql_task_t* task) {
    std::cout << "exec callback, sql: <" << task->sql << ">" << std::endl;
}

static void mysql_query_callback(const kim::MysqlAsyncConn*, kim::sql_task_t* task, MYSQL_RES* res) {
    // std::cout << "query callback, sql: [" << task->sql << "]" << std::endl;
    if (++callback_cnt == MAX_CALLBACK_CNT) {
        std::cout << "spend time: " << time_now() - begin_time << std::endl;
    }
}

int main() {
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    kim::Log* m_logger = new kim::Log;
    if (!m_logger->set_log_path("./kimserver.log")) {
        LOG_ERROR("set log path failed!");
        return 1;
    }
    m_logger->set_level("info");

    kim::CJsonObject config;
    if (!config.Load("../../bin/config.json")) {
        LOG_ERROR("load json config failed!");
        return 1;
    }

    if (config["database"].IsEmpty()) {
        LOG_ERROR("an not find key!");
        SAFE_DELETE(m_logger);
        return 1;
    }
    // LOG_DEBUG("%s", config["database"].ToString().c_str());

    struct ev_loop* loop = EV_DEFAULT;

    kim::DBMgr* pool = new kim::DBMgr(m_logger, loop);
    if (!pool->init(config["database"])) {
        std::cerr << "init db pool failed!" << std::endl;
        SAFE_DELETE(pool);
        SAFE_DELETE(m_logger);
        return 1;
    }
    LOG_INFO("init mysql pool done!");
    /* 
        {"database":{"test":{"host":"127.0.0.1","port":3306,"user":"root",
               "password":"1234567","charset":"utf8mb4","connection_count":10}}} 
    */

    std::string sql;
    sql = format_str("insert into mytest.test_async_mysql (value) values ('%s');", "hello+world");
    // pool->sql_exec("test", &mysql_exec_callback, sql);

    begin_time = time_now();
    for (int i = 0; i < MAX_CALLBACK_CNT; i++) {
        sql = format_str("select value from mytest.test_async_mysql where id = %d;", 1);
        pool->sql_query("test", &mysql_query_callback, sql);
    }

    timer.data = (void*)g_userdata;
    ev_timer_init(&timer, timeout_cb, 1.0, 100.0);
    ev_timer_start(loop, &timer);

    ev_run(loop, 0);
    SAFE_DELETE(pool);
    SAFE_DELETE(m_logger);
    return 0;
}