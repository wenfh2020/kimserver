#ifndef __EVENTS_CALLBAK_H__
#define __EVENTS_CALLBAK_H__

#include <ev.h>
#include <hiredis/async.h>
#include <hiredis/hiredis.h>

#include "context.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class INet;

// privdata for cmd callback.
typedef struct wait_cmd_info_s {
    wait_cmd_info_s(INet* n, uint64_t mid, uint64_t cid)
        : net(n), module_id(mid), cmd_id(cid) {
    }
    INet* net = nullptr;
    uint64_t module_id = 0;
    uint64_t cmd_id = 0;
} wait_cmd_info_t;

class INet {
   public:
    INet() {}
    virtual ~INet() {}

   public:
    virtual uint64_t get_new_seq() { return 0; }
    virtual bool get_redis_config(_cstr& key, CJsonObject& config) { return false; }

    // libev callback.
    /////////////////////////////////
   public:
    // signal.
    virtual void on_terminated(ev_signal* s) {}
    virtual void on_child_terminated(ev_signal* s) {}

    // socket.
    virtual void on_io_read(int fd) {}
    virtual void on_io_write(int fd) {}
    virtual void on_io_error(int fd) {}

    // timer.
    virtual void on_io_timer(void* privdata) {}
    virtual void on_cmd_timer(void* privdata) {}
    virtual void on_repeat_timer(void* privdata) {}

    // redis callback
    /////////////////////////////////
    virtual void on_redis_connect(const redisAsyncContext* c, int status) {}
    virtual void on_redis_disconnect(const redisAsyncContext* c, int status) {}
    virtual void on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) {}

   public:
    // socket.
    virtual bool send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) { return false; }
    virtual E_RDS_STATUS redis_send_to(_cstr& host, int port, uint64_t module_id, uint64_t cmd_id, _csvector& rds_cmds) {
        return E_RDS_STATUS::ERROR;
    }

    // events
    virtual ev_timer* add_cmd_timer(double secs, ev_timer* w, void* privdata) { return nullptr; }
    virtual bool del_cmd_timer(ev_timer*) { return false; }
};

}  // namespace kim

#endif  //__EVENTS_CALLBAK_H__