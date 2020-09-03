#include <iostream>
#include <vector>

#include "redis/redis_mgr.h"
#include "server.h"
#include "util/json/CJsonObject.hpp"
#include "util/log.h"

bool g_is_write = false;
double g_begin_time;
int g_test_cnt = 0;
int g_send_cnt = 0;
int g_send_ok_cnt = 0;
int g_send_err_cnt = 0;
int g_cur_callback_cnt = 0;
int g_err_callback_cnt = 0;
int g_ok_callback_cnt = 0;

ev_timer m_timer;
kim::RedisMgr* g_mgr = nullptr;

typedef struct user_data_s {
    user_data_s(int n) : num(n) {}
    int num = 0;
} user_data_t;

void on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) {
    /*     std::cout << "--sfs---" << std::endl;
    user_data_t* d = static_cast<user_data_t*>(privdata);
    if (d != nullptr) {
        std::cout << "-----" << std::endl
                  << "err: " << c->err << std::endl
                  << "errstr: " << c->errstr << std::endl
                  << "privdata: " << d->num << std::endl;
        SAFE_DELETE(d);
    } */

    if (c->err != REDIS_OK) {
        g_err_callback_cnt++;
    } else {
        g_ok_callback_cnt++;
    }

    if (++g_cur_callback_cnt == g_test_cnt) {
        std::cout << "spend time: " << time_now() - g_begin_time << std::endl
                  << "write avg:  " << g_test_cnt / (time_now() - g_begin_time)
                  << std::endl;

        std::cout << "callback cnt:     " << g_cur_callback_cnt << std::endl
                  << "err callback cnt: " << g_err_callback_cnt << std::endl;
    }

    user_data_t* d = static_cast<user_data_t*>(privdata);
    SAFE_DELETE(d);
}

void libev_timer_cb(struct ev_loop* loop, ev_timer* w, int events) {
    std::cout << "------" << std::endl
              << "send cnt:         " << g_send_cnt << std::endl
              << "send ok cnt:      " << g_send_ok_cnt << std::endl
              << "send err cnt:     " << g_send_err_cnt << std::endl
              << "callback cnt:     " << g_cur_callback_cnt << std::endl
              << "ok callback cnt:  " << g_ok_callback_cnt << std::endl
              << "err callback cnt: " << g_err_callback_cnt << std::endl;

    std::vector<std::string> read_cmds{"get", "key"};
    std::vector<std::string> write_cmds{"set", "key", "hello world!"};
    user_data_t* d = new user_data_t(++g_send_cnt);
    if (!g_mgr->send_to("test", g_is_write ? write_cmds : read_cmds,
                        on_redis_callback, (void*)d)) {
        SAFE_DELETE(d);
        g_send_err_cnt++;
    } else {
        g_send_ok_cnt++;
    }
}

bool check_args(int args, char** argv) {
    if (args < 2) {
        std::cerr << "invalid args! './test_redis read 1' or './test_redis write 1'"
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
        std::cerr << "set log path failed!" << std::endl;
        SAFE_DELETE(m_logger);
        return 1;
    }
    m_logger->set_level(kim::Log::LL_DEBUG);

    kim::CJsonObject config;
    if (!config.Load("../../../bin/config.json")) {
        SAFE_DELETE(m_logger);
        LOG_ERROR("load json config failed!");
        return 1;
    }

    struct ev_loop* loop = EV_DEFAULT;
    ev_timer_init(&m_timer, libev_timer_cb, 1.0, 1.0);
    ev_timer_start(loop, &m_timer);

    g_mgr = new kim::RedisMgr(m_logger, loop);
    if (!g_mgr->init(config["redis"])) {
        SAFE_DELETE(g_mgr);
        SAFE_DELETE(m_logger);
        LOG_ERROR("init redis g_mgr failed!");
        return 1;
    }

    g_begin_time = time_now();
    // std::vector<std::string> read_cmds{"get", "key"};
    // std::vector<std::string> write_cmds{"set", "key", "hello world!"};
    // for (int i = 0; i < g_test_cnt; i++) {
    //     user_data_t* d = new user_data_t(++g_send_cnt);
    //     g_mgr->send_to("test", g_is_write ? write_cmds : read_cmds,
    //                    on_redis_callback, (void*)d);
    // }

    ev_run(loop, 0);
    SAFE_DELETE(g_mgr);
    SAFE_DELETE(m_logger);
    return 0;
}