// g++ -std='c++11' -g -Wno-noexcept-type test_async_mysql.cpp -o amysql -lev && ./amysql

#include <ev.h>

#include <iostream>

#include "db_mgr.h"
#include "server.h"
#include "util/json/CJsonObject.hpp"
#include "util/util.h"

ev_timer timer;
int g_callback_cnt = 0;
double g_begin_time;
int g_test_cnt = 0;
const char* g_userdata = "123456";

// close task. leak.

/*
CREATE DATABASE IF NOT EXISTS mytest DEFAULT CHARSET utf8mb4 COLLATE utf8mb4_general_ci;

CREATE TABLE `test_async_mysql` (
    `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
    `value` varchar(32) NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8mb4; 
 */

static void
timeout_cb(EV_P_ ev_timer* w, int revents) {
    std::cout << "timer hit. data: "
              << static_cast<char*>(w->data)
              << std::endl;
}

static void mysql_exec_callback(const kim::MysqlAsyncConn* c, kim::sql_task_t* task) {
    if (++g_callback_cnt == g_test_cnt) {
        std::cout << "spend time: " << time_now() - g_begin_time << std::endl
                  << "write avg:  " << g_test_cnt / (time_now() - g_begin_time)
                  << std::endl;
    }
    if (task != nullptr) {
        if (task->error != 0) {
            std::cout << task->error << " " << task->errstr << std::endl;
        }
    }
}

static void mysql_query_callback(const kim::MysqlAsyncConn* c, kim::sql_task_t* task, MYSQL_RES* res) {
    // std::cout << "query callback, sql: [" << task->sql << "]" << std::endl;
    if (++g_callback_cnt == g_test_cnt) {
        std::cout << "spend time: " << time_now() - g_begin_time << std::endl
                  << "read avg:   " << g_test_cnt / (time_now() - g_begin_time)
                  << std::endl;
    }
    if (task != nullptr) {
        if (task->error != 0) {
            std::cout << task->error << " " << task->errstr << std::endl;
        }
    }
}

int main(int args, char** argv) {
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    if (args < 2) {
        std::cerr << "invalid args! './amysql read' or './amysql write'"
                  << std::endl;
        return 1;
    }

    bool is_write = false;
    if (!strcasecmp(argv[1], "write")) {
        is_write = true;
        std::cout << "write" << std::endl;
    }

    g_test_cnt = atoi(argv[2]);
    if (g_test_cnt == 0) {
        std::cerr << "invalid test cnt: " << argv[2] << std::endl;
        return 1;
    }

    kim::Log* m_logger = new kim::Log;
    if (!m_logger->set_log_path("./kimserver.log")) {
        LOG_ERROR("set log path failed!");
        return 1;
    }
    m_logger->set_level("CRIT");

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
    g_begin_time = time_now();
    for (int i = 0; i < g_test_cnt; i++) {
        if (is_write) {
            std::string sql = format_str("insert into mytest.test_async_mysql (value) values ('%s');", "hello+world");
            pool->sql_exec("test", &mysql_exec_callback, sql);
        } else {
            std::string sql("select value from mytest.test_async_mysql where id = 1;");
            pool->sql_query("test", &mysql_query_callback, sql);
        }
    }

    timer.data = (void*)g_userdata;
    ev_timer_init(&timer, timeout_cb, 1.0, 10.0);
    ev_timer_start(loop, &timer);

    ev_run(loop, 0);
    SAFE_DELETE(pool);
    SAFE_DELETE(m_logger);
    return 0;
}