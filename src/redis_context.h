#ifndef __REDIS_CONTEXT_H__
#define __REDIS_CONTEXT_H__

#include <hiredis/async.h>
#include <hiredis/hiredis.h>

#include "net.h"
#include "server.h"
#include "util/log.h"

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
    RdsConnection(Log* logger, INet* net, _cstr& host, int port, redisAsyncContext* c);
    virtual ~RdsConnection();

    bool is_active() { return m_state == STATE::OK; }
    bool is_connecting() { return m_state == STATE::CONNECTING; }
    void set_state(RdsConnection::STATE s) { m_state = s; }
    RdsConnection::STATE get_state() { return m_state; }

    redisAsyncContext* get_ctx() { return m_conn; }
    int get_port() { return m_port; }
    _cstr& get_host() { return m_host; }

    wait_cmd_info_t* add_wait_cmd_info(uint64_t module_id, uint64_t cmd_id, int step = 0);
    bool del_wait_cmd_info(uint64_t cmd_id);
    wait_cmd_info_t* find_wait_cmd_info(uint64_t cmd_id);
    void clear_wait_cmd_infos();
    const std::unordered_map<uint64_t, wait_cmd_info_t*>& get_wait_cmd_infos() { return m_wait_cmds; }

   private:
    Log* m_logger = nullptr;
    INet* m_net = nullptr;
    std::string m_host;
    int m_port = 0;
    std::string m_identity;

    redisAsyncContext* m_conn = nullptr;
    STATE m_state = STATE::CLOSED;

    std::unordered_map<uint64_t, wait_cmd_info_t*> m_wait_cmds;
};

}  // namespace kim

#endif  //__REDIS_CONTEXT_H__