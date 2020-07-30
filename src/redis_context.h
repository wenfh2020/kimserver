#ifndef __REDIS_CONTEXT_H__
#define __REDIS_CONTEXT_H__

#include <hiredis/async.h>
#include <hiredis/hiredis.h>

#include "server.h"

namespace kim {

class RdsConnection {
   public:
    enum class STATE {
        NONE = 0,
        CONNECTING,
        OK,
        CLOSED,
        ERROR
    };
    RdsConnection(_cstr& host, int port, redisAsyncContext* c);
    virtual ~RdsConnection();

    bool is_active() { return m_state == STATE::OK; }
    bool is_connecting() { return m_state == STATE::CONNECTING; }
    void set_state(RdsConnection::STATE s) { m_state = s; }
    RdsConnection::STATE get_state() { return m_state; }

    redisAsyncContext* get_ctx() { return m_conn; }
    int get_port() { return m_port; }
    _cstr& get_host() { return m_host; }

   private:
    std::string m_host;
    int m_port = 0;
    std::string m_identity;

    redisAsyncContext* m_conn = nullptr;
    STATE m_state = STATE::CLOSED;
};

}  // namespace kim

#endif  //__REDIS_CONTEXT_H__