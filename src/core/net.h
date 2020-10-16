#ifndef __KIM_NET_H__
#define __KIM_NET_H__

#include <ev.h>
#include <hiredis/async.h>
#include <hiredis/hiredis.h>

#include "connection.h"
#include "db/mysql_async_conn.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class Cmd;
class INet;
class Session;
class Events;
class ZookeeperMgr;
struct zk_task_t;

// privdata for cmd callback.
typedef struct wait_cmd_info_s {
    wait_cmd_info_s(INet* n, uint64_t mid, uint64_t cid, int step = 0)
        : net(n), module_id(mid), cmd_id(cid), exec_step(step) {
    }
    INet* net = nullptr;
    uint64_t module_id = 0;
    uint64_t cmd_id = 0;
    int exec_step = 0;
} wait_cmd_info_t;

class INet {
   public:
    INet() {}
    virtual ~INet() {}

   public:
    virtual double now() { return time_now(); }
    virtual uint64_t new_seq() { return 0; }
    virtual CJsonObject& config() { return m_conf; }
    virtual Events* events() { return nullptr; }
    virtual ZookeeperMgr* zkmgr() { return nullptr; }

    /* cmd */
    virtual bool add_cmd(Cmd* cmd) { return false; }
    virtual Cmd* get_cmd(uint64_t id) { return nullptr; }
    virtual bool del_cmd(Cmd* cmd) { return false; }

    /* session */
    virtual bool add_session(Session* s) { return false; }
    virtual Session* get_session(const std::string& sessid, bool re_active = false) { return nullptr; }
    virtual bool del_session(const std::string& sessid) { return false; }

   public:
    /* signal. */
    virtual void on_terminated(ev_signal* s) {}
    virtual void on_child_terminated(ev_signal* s) {}

    /* socket. */
    virtual void on_io_read(int fd) {}
    virtual void on_io_write(int fd) {}
    virtual void on_io_error(int fd) {}

    /* timer. */
    virtual void on_io_timer(void* privdata) {}
    virtual void on_cmd_timer(void* privdata) {}
    virtual void on_repeat_timer(void* privdata) {}
    virtual void on_session_timer(void* privdata) {}

    /* redis callback */
    virtual void on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) {}

    /* zookeeper callback */
    void on_zk_cmd(const kim::zk_task_t* task) {}
    void on_zk_data_change(const std::string& path, const std::string& new_value, void* privdata) {}
    void on_zk_child_change(const std::string& path, const std::vector<std::string>& children, void* privdata) {}

    /* database callback */
    virtual void on_mysql_exec_callback(const MysqlAsyncConn* c, sql_task_t* task) {}
    virtual void on_mysql_query_callback(const MysqlAsyncConn* c, sql_task_t* task, MysqlResult* res) {}

   public:
    /* socket. */
    virtual bool send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) { return false; }
    virtual bool send_to(std::shared_ptr<Connection> c, const MsgHead& head, const MsgBody& body) { return false; }

    // redis.
    virtual bool redis_send_to(const char* node, Cmd*, const std::vector<std::string>& argv) {
        return false;
    }

    // database.
    virtual bool db_exec(const char* node, const char* sql, Cmd* cmd) { return false; }
    virtual bool db_query(const char* node, const char* sql, Cmd* cmd) { return false; }

    // events
    virtual ev_timer* add_cmd_timer(double secs, ev_timer* w, void* privdata) { return nullptr; }
    virtual bool del_cmd_timer(ev_timer*) { return false; }

   protected:
    CJsonObject m_conf;
};

}  // namespace kim

#endif  //__KIM_NET_H__