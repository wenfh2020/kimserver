/* ./amysql read 1' or './amysql write 1' */

#include <ev.h>

#include <iostream>

#include "db_mgr.h"
#include "server.h"
#include "util/json/CJsonObject.hpp"
#include "util/util.h"

/*
CREATE DATABASE IF NOT EXISTS mytest DEFAULT CHARSET utf8mb4 COLLATE utf8mb4_general_ci;

CREATE TABLE `test_async_mysql` (
    `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
    `value` varchar(32) NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8mb4; 
*/

// make all project.

int g_test_cnt = 0;
int g_cur_callback_cnt = 0;
int g_err_callback_cnt = 0;
double g_begin_time;
bool g_is_write = false;
const char* g_userdata = "123456";
ev_timer m_timer;

void libev_timer_cb(struct ev_loop* loop, ev_timer* w, int events) {
    std::cout << "------" << std::endl
              << "callback cnt:     " << g_cur_callback_cnt << std::endl
              << "err callback cnt: " << g_err_callback_cnt << std::endl;
}

static void mysql_exec_callback(const kim::MysqlAsyncConn* c, kim::sql_task_t* task) {
    if (++g_cur_callback_cnt == g_test_cnt) {
        std::cout << "spend time: " << time_now() - g_begin_time << std::endl
                  << "write avg:  " << g_test_cnt / (time_now() - g_begin_time)
                  << std::endl;

        std::cout << "callback cnt:     " << g_cur_callback_cnt << std::endl
                  << "err callback cnt: " << g_err_callback_cnt << std::endl;
    }

    if (task != nullptr) {
        if (task->error != 0) {
            std::cout << task->error << " " << task->errstr << std::endl;
        }
    }
}

bool check_result(kim::sql_task_t* task, kim::MysqlResult* res) {
    if (task == nullptr || task->error != 0) {
        g_err_callback_cnt++;
        std::cout << task->error << " " << task->errstr << std::endl;
        return false;
    }

    if (res == nullptr || !res->is_ok()) {
        g_err_callback_cnt++;
        std::cout << "invalid result!" << std::endl;
        return false;
    }

    kim::vec_row_t data;
    if (res->get_num_rows() == 0) {
        g_err_callback_cnt++;
        return false;
    }

    return true;
}

static void mysql_query_callback(const kim::MysqlAsyncConn* c, kim::sql_task_t* task, kim::MysqlResult* res) {
    /*  std::cout << "query callback, sql: [" << task->sql << "]" << std::endl; */
    check_result(task, res);
    if (++g_cur_callback_cnt == g_test_cnt) {
        std::cout << "spend time: " << time_now() - g_begin_time << std::endl
                  << "read avg:   " << g_test_cnt / (time_now() - g_begin_time)
                  << std::endl;

        std::cout << "callback cnt:     " << g_cur_callback_cnt << std::endl
                  << "err callback cnt: " << g_err_callback_cnt << std::endl;
    }
    /* 
    kim::vec_row_t data;
    int size = res->get_result_data(data);
    if (size != 0) {
        for (size_t i = 0; i < data.size(); i++) {
            kim::map_row_t& items = data[i];
            for (const auto& it : items) {
                std::cout << "col: " << it.first << ", "
                          << "data: " << it.second << std::endl;
            }
        }
    }
  */
}

bool check_args(int args, char** argv) {
    if (args < 2) {
        std::cerr << "invalid args! exp: './amysql read 1' or './amysql write 1'"
                  << std::endl;
        return false;
    }

    if (!strcasecmp(argv[1], "write")) {
        g_is_write = true;
    }

    if (argv[2] == nullptr || !isdigit(argv[2][0]) || atoi(argv[2]) == 0) {
        std::cerr << "invalid test cnt." << std::endl;
        return false;
    }

    g_test_cnt = atoi(argv[2]);
    return true;
}

int main(int args, char** argv) {
    if (!check_args(args, argv)) {
        return 1;
    }

    kim::Log* m_logger = new kim::Log;
    if (!m_logger->set_log_path("./kimserver.log")) {
        LOG_ERROR("set log path failed!");
        SAFE_DELETE(m_logger);
        return 1;
    }

    /* "debug", "err", "info", "crit" ... pls set "err" if pressure. */
    // m_logger->set_level(kim::Log::LL_DEBUG);
    m_logger->set_level(kim::Log::LL_INFO);

    kim::CJsonObject config;
    if (!config.Load("../../bin/config.json") || config["database"].IsEmpty()) {
        LOG_ERROR("load json config failed!");
        SAFE_DELETE(m_logger);
        return 1;
    }
    /* LOG_DEBUG("%s", config["database"].ToString().c_str()); */

    /* 
        {"database":{"test":{"host":"127.0.0.1","port":3306,"user":"root",
               "password":"1234567","charset":"utf8mb4","connection_count":10}}} 
    */

    struct ev_loop* loop = EV_DEFAULT;
    ev_timer_init(&m_timer, libev_timer_cb, 1.0, 1.0);
    ev_timer_start(loop, &m_timer);

    kim::DBMgr* pool = new kim::DBMgr(m_logger, loop);
    if (!pool->init(config["database"])) {
        LOG_ERROR("init db pool failed!");
        SAFE_DELETE(pool);
        SAFE_DELETE(m_logger);
        return 1;
    }

    char sql[1024];
    g_begin_time = time_now();

    for (int i = 0; i < g_test_cnt; i++) {
        if (g_is_write) {
            snprintf(sql, sizeof(sql),
                     "insert into mytest.test_async_mysql (value) values ('%s');",
                     "hello world");
            if (!pool->async_exec("test", &mysql_exec_callback, sql)) {
                LOG_ERROR("exec sql failed! sql: %s", sql);
                return 1;
            }
        } else {
            snprintf(sql, sizeof(sql),
                     "select value from mytest.test_async_mysql where id = 1;");
            if (!pool->async_query("test", &mysql_query_callback, sql)) {
                LOG_ERROR("quert sql failed! sql: %s", sql);
                return 1;
            }
        }
    }

    std::cout << "waiting result" << std::endl;

    ev_run(loop, 0);
    SAFE_DELETE(pool);
    SAFE_DELETE(m_logger);
    return 0;
}