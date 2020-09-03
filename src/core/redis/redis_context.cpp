#include "redis_context.h"

#include <hiredis/adapters/libev.h>
#include <hiredis/hiredis.h>

#include "error.h"

namespace kim {

RdsConnection::RdsConnection(Log* logger, struct ev_loop* loop)
    : m_logger(logger), m_loop(loop) {
}

RdsConnection::~RdsConnection() {
    destory();
}

void RdsConnection::destory() {
    if (m_ctx != nullptr) {
        char errstr[64] = {"close connection!"};
        for (auto& it : m_wait_tasks) {
            task_t* task = it;
            wait_task_error_callback(m_ctx, task, REDIS_ERR, errstr);
            SAFE_DELETE(task);
        }
        redisAsyncDisconnect(m_ctx);
    }
    m_wait_tasks.clear();
    set_state(STATE::CLOSED);
    m_ctx = nullptr;
}

bool RdsConnection::connect(const std::string& host, int port) {
    if (host.empty() || port == 0) {
        LOG_ERROR("invalid params!");
        return false;
    }

    if (m_ctx != nullptr) {
        destory();
    }

    redisAsyncContext* c = redisAsyncConnect(host.c_str(), port);
    if (c == nullptr || c->err != REDIS_OK) {
        LOG_ERROR("connect redis failed! errno: %d, error: %s, host: %s, port: %d",
                  c->err, c->errstr, host.c_str(), port);
        return false;
    }

    c->data = this;
    if (redisLibevAttach(m_loop, c) != REDIS_OK) {
        redisAsyncFree(c);
        LOG_ERROR("redis libev attach failed!");
        return false;
    }
    redisAsyncSetConnectCallback(c, on_redis_connect_libev_callback);
    redisAsyncSetDisconnectCallback(c, on_redis_disconnect_libev_callback);
    set_state(STATE::CONNECTING);
    m_ctx = c;
    return true;
}

void RdsConnection::on_redis_connect_libev_callback(const redisAsyncContext* ac, int status) {
    RdsConnection* c = static_cast<RdsConnection*>(ac->data);
    c->on_redis_connect_callback(ac, status);
}

void RdsConnection::on_redis_disconnect_libev_callback(const redisAsyncContext* ac, int status) {
    RdsConnection* c = static_cast<RdsConnection*>(ac->data);
    c->on_redis_connect_callback(ac, status);
}

void RdsConnection::on_redis_connect_callback(const redisAsyncContext* ac, int status) {
    if (status == REDIS_OK) {
        set_state(STATE::CONNECTED);
        LOG_INFO("redis connected!, host: %s, port: %d",
                 ac->c.tcp.host, ac->c.tcp.port);
    } else {
        /* no need to call redisAsyncDisconnect. */
        m_ctx = nullptr;
        set_state(STATE::CLOSED);
        LOG_ERROR("redis connect failed!, host: %s, port: %d",
                  ac->c.tcp.host, ac->c.tcp.port);
    }

    char send_errstr[64] = {"send to redis failed!"};
    char connect_errstr[64] = {"connect to redis failed!"};

    /* send wait tasks. */
    for (auto& it : m_wait_tasks) {
        task_t* task = it;
        if (status == REDIS_OK) {
            if (!send_to(task->cmd_argv, task->fn, task->privdata)) {
                wait_task_error_callback(ac, task, REDIS_ERR, send_errstr);
            }
        } else {
            wait_task_error_callback(ac, task, REDIS_ERR, connect_errstr);
        }
        SAFE_DELETE(it);
    }
    m_wait_tasks.clear();
}

void RdsConnection::on_redis_disconnect_callback(const redisAsyncContext* ac, int status) {
    LOG_ERROR("redis disconnected!, host: %s, port: %d", ac->c.tcp.host, ac->c.tcp.port);
    set_state(STATE::CLOSED);
    m_ctx = nullptr;
}

bool RdsConnection::send_to(
    const std::vector<std::string>& cmd_argv, redisCallbackFn* fn, void* privdata) {
    if (m_ctx == nullptr || is_closed()) {
        LOG_DEBUG("connection is not ok! host: %s, port: %d, state: %d",
                  m_host.c_str(), m_port, m_state);
        return false;
    }

    if (is_connecting()) {
        if (!add_wait_task(cmd_argv, fn, privdata)) {
            LOG_ERROR("add wait task failed!");
            return false;
        }
        return true;
    }

    // LOG_DEBUG("send to redis, cmd: %s", format_redis_cmds(cmd_argv).c_str());

    /* ok. send cmd to redis. */
    size_t arglen[cmd_argv.size()];
    const char* argv[cmd_argv.size()];
    for (size_t i = 0; i < cmd_argv.size(); i++) {
        argv[i] = cmd_argv[i].c_str();
        arglen[i] = cmd_argv[i].length();
    }

    int ret = redisAsyncCommandArgv(m_ctx, fn, privdata, cmd_argv.size(), argv, arglen);
    if (ret != REDIS_OK) {
        LOG_ERROR("redis send to failed! ret: %d, errno: %d, error: %s",
                  ret, m_ctx->err, m_ctx->errstr);
    }
    return (ret == REDIS_OK);
}

bool RdsConnection::add_wait_task(
    const std::vector<std::string>& cmd_argv, redisCallbackFn* fn, void* privdata) {
    if (cmd_argv.size() == 0 || fn == nullptr) {
        return false;
    }

    // LOG_DEBUG("add wait task, redis cmd: %s", format_redis_cmds(cmd_argv).c_str());
    task_t* task = new task_t;
    task->fn = fn;
    task->cmd_argv = cmd_argv;
    task->privdata = privdata;
    m_wait_tasks.push_back(task);
    return true;
}

void RdsConnection::wait_task_error_callback(const redisAsyncContext* ac, task_t* task, int err, char* errstr) {
    /* danger: const to no const. */
    redisAsyncContext* c = const_cast<redisAsyncContext*>(ac);
    int old_error = c->err;
    char* old_errstr = c->errstr;

    if (c->err == REDIS_OK) {
        c->err = REDIS_ERR;
    }
    if (c->errstr == nullptr) {
        c->errstr = errstr;
    }
    task->fn(c, nullptr, task->privdata);
    c->err = old_error;
    c->errstr = old_errstr;
}

}  // namespace kim