#ifndef __KIM_NET_H__
#define __KIM_NET_H__

#include <ev.h>
#include <hiredis/async.h>
#include <hiredis/hiredis.h>

#include "connection.h"
#include "db/mysql_async_conn.h"
#include "util/json/CJsonObject.hpp"
#include "zookeeper/zk_task.h"

namespace kim {

class Cmd;
class INet;
class Session;
class Events;
class zk_node;
class SysCmd;
class WorkerDataMgr;
class Nodes;
class Request;

/* privdata for cmd callback. */
typedef struct wait_cmd_info_s {
    INet* net;
    uint64_t cmd_id;
    int exec_step;
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
    virtual SysCmd* sys_cmd() { return nullptr; }
    virtual Nodes* nodes() { return nullptr; }

    /* node info. */
    virtual bool is_worker() { return false; }
    virtual bool is_manager() { return false; }
    virtual WorkerDataMgr* worker_data_mgr() { return nullptr; }

    virtual std::string node_type() { return ""; }
    virtual std::string node_host() { return ""; }
    virtual int node_port() { return 0; }
    virtual int worker_index() { return -1; }

    /* connection. */
    virtual bool update_conn_state(int fd, Connection::STATE state) { return false; }
    virtual bool add_client_conn(const std::string& node_id, const fd_t& f) { return false; }

    /* cmd */
    virtual bool
    add_cmd(Cmd* cmd) { return false; }
    virtual Cmd* get_cmd(uint64_t id) { return nullptr; }
    virtual bool del_cmd(Cmd* cmd) { return false; }

    /* session */
    virtual bool add_session(Session* s) { return false; }
    virtual Session* get_session(const std::string& sessid, bool re_active = false) { return nullptr; }
    virtual bool del_session(const std::string& sessid) { return false; }

   public:
    /* socket. */
    virtual bool send_to(Connection* c, const HttpMsg& msg) { return false; }
    virtual bool send_to(Connection* c, const MsgHead& head, const MsgBody& body) { return false; }
    virtual bool send_req(Connection* c, uint32_t cmd, uint32_t seq, const std::string& data) { return false; }
    virtual bool send_to(const fd_t& f, const HttpMsg& msg) { return false; }
    virtual bool send_to(const fd_t& f, const MsgHead& head, const MsgBody& body) { return false; }
    virtual bool send_req(const fd_t& f, uint32_t cmd, uint32_t seq, const std::string& data) { return false; }
    virtual bool send_ack(const Request& req, int err, const std::string& errstr, const std::string& data = "") { return false; }

    /* send to other node. */
    virtual bool auto_send(const std::string& ip, int port, int worker_index, const MsgHead& head, const MsgBody& body) { return false; }
    /* only for worker. */
    virtual bool send_to_node(const std::string& node_type, const std::string& obj, const MsgHead& head, const MsgBody& body) { return false; }
    /* only for manager. */
    virtual bool send_to_children(int cmd, uint64_t seq, const std::string& data) { return false; }
    /* only for worker. */
    virtual bool send_to_parent(int cmd, uint64_t seq, const std::string& data) { return false; }

    // redis.
    virtual bool redis_send_to(const char* node, Cmd*, const std::vector<std::string>& argv) { return false; }

    // database.
    virtual bool db_exec(const char* node, const char* sql, Cmd* cmd) { return false; }
    virtual bool db_query(const char* node, const char* sql, Cmd* cmd) { return false; }

    // events
    virtual ev_timer* add_cmd_timer(double secs, ev_timer* w, void* privdata) { return nullptr; }
    virtual bool del_cmd_timer(ev_timer*) { return false; }

   public:
    /* socket. */
    virtual void on_io_read(int fd) {}
    virtual void on_io_write(int fd) {}
    virtual void on_io_error(int fd) {}

    /* timer. */
    virtual void on_io_timer(void* privdata) {}
    virtual void on_cmd_timer(void* privdata) {}
    virtual void on_session_timer(void* privdata) {}

    /* redis callback */
    virtual void on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) {}

    /* database callback */
    virtual void on_mysql_exec_callback(const MysqlAsyncConn* c, sql_task_t* task) {}
    virtual void on_mysql_query_callback(const MysqlAsyncConn* c, sql_task_t* task, MysqlResult* res) {}

   protected:
    CJsonObject m_conf;
};

}  // namespace kim

#endif  //__KIM_NET_H__