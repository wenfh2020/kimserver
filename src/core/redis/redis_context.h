#ifndef __REDIS_CONTEXT_H__
#define __REDIS_CONTEXT_H__

#include <hiredis/async.h>
#include <hiredis/hiredis.h>

#include <vector>

#include "net.h"
#include "server.h"
#include "util/log.h"

namespace kim {

class RdsConnection {
   public:
    typedef struct task_s {
        std::vector<std::string> argv;
        redisCallbackFn* fn = nullptr;
        void* privdata = nullptr;
    } task_t;

    enum class STATE {
        CONNECTING = 0,
        CONNECTED,
        CLOSED
    };

    RdsConnection(Log* logger, struct ev_loop* loop);
    virtual ~RdsConnection();
    bool connect(const std::string& host, int port);
    bool send_to(const std::vector<std::string>& argv, redisCallbackFn* fn, void* privdata);

    // state.
    bool is_connected() { return m_state == STATE::CONNECTED; }
    bool is_connecting() { return m_state == STATE::CONNECTING; }
    bool is_active() { return (is_connecting() || is_connected()); }
    bool is_closed() { return m_state == STATE::CLOSED; }

    int port() { return m_port; }
    const std::string& host() const { return m_host; }
    const char* host() { return m_host.c_str(); }

   private:
    void destory();
    void set_state(RdsConnection::STATE s) { m_state = s; }

    // task.
    bool add_wait_task(const std::vector<std::string>& argv, redisCallbackFn* fn, void* privdata);
    void wait_task_error_callback(const redisAsyncContext* c, task_t* task, int err, char* errstr);

    // callback.
    void on_redis_connect_callback(const redisAsyncContext* c, int status);
    void on_redis_disconnect_callback(const redisAsyncContext* c, int status);
    static void on_redis_connect_libev_callback(const redisAsyncContext* c, int status);
    static void on_redis_disconnect_libev_callback(const redisAsyncContext* c, int status);

   private:
    Log* m_logger = nullptr;
    struct ev_loop* m_loop = nullptr;

    int m_port = 0;
    std::string m_host;
    std::list<task_t*> m_wait_tasks; /* add task to task list, before connection is ok.*/

    STATE m_state = STATE::CLOSED;
    redisAsyncContext* m_ctx = nullptr; /* hiredis async connection. */
};

}  // namespace kim

#endif  //__REDIS_CONTEXT_H__