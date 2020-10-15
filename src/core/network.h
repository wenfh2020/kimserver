#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <sys/socket.h>

#include <iostream>
#include <list>
#include <unordered_map>

#include "connection.h"
#include "db/db_mgr.h"
#include "events.h"
#include "module_mgr.h"
#include "net/anet.h"
#include "net/chanel.h"
#include "redis/redis_mgr.h"
#include "session.h"
#include "worker_data_mgr.h"

namespace kim {

class Network : public INet {
   public:
    enum class TYPE {
        UNKNOWN = 0,
        MANAGER,
        WORKER,
    };

    /* manager aync send fd to worker by sendmsg, 
     * return -1, errno == EAGIN, and then resend it again. */
    typedef struct chanel_resend_data_s {
        channel_t ch;
        int count = 0;
    } chanel_resend_data_t;

    Network(Log* logger, TYPE type);
    virtual ~Network();
    bool create(INet* net, const CJsonObject& config, int ctrl_fd, int data_fd);                     // for worker.
    bool create(const AddrInfo* addr_info, INet* net, const CJsonObject& config, WorkerDataMgr* m);  // for manager.
    void destory();

    bool load_config(const CJsonObject& config);
    bool load_timer(INet* net);
    bool load_modules();
    bool load_db();
    bool load_redis_mgr();

    virtual double now() override;
    virtual Events* events() override;

    // events.
    void run();
    void end_ev_loop();

    std::shared_ptr<Connection> add_read_event(int fd, Codec::TYPE codec, bool is_chanel = false);
    virtual ev_timer* add_cmd_timer(double secs, ev_timer* w, void* privdata) override;
    virtual bool del_cmd_timer(ev_timer* w) override;

    // session
    virtual bool add_session(Session* s) override;
    virtual Session* get_session(const std::string& sessid, bool re_active = false) override;
    virtual bool del_session(const std::string& sessid) override;

    // socket.
    void close_chanel(int* fds);
    void close_fds();  // for child to close the parent's fds when fork.
    bool close_conn(int fd);

    // owner type
    bool is_worker() { return m_type == TYPE::WORKER; }
    bool is_manager() { return m_type == TYPE::MANAGER; }

    bool set_gate_codec(const std::string& codec);
    void set_keep_alive(double secs) { m_keep_alive = secs; }
    double keep_alive() { return m_keep_alive; }

   public:
    virtual uint64_t new_seq() override { return ++m_seq; }
    virtual bool add_cmd(Cmd* cmd) override;
    virtual Cmd* get_cmd(uint64_t id) override;
    virtual bool del_cmd(Cmd* cmd) override;

    // io callback.
    virtual void on_io_read(int fd) override;
    virtual void on_io_write(int fd) override;
    virtual void on_io_error(int fd) override;

    // timer callback.
    virtual void on_io_timer(void* privdata) override;
    virtual void on_cmd_timer(void* privdata) override;
    virtual void on_session_timer(void* privdata) override;
    virtual void on_repeat_timer(void* privdata) override;

    // redis callback.
    static void on_redis_lib_callback(redisAsyncContext* c, void* reply, void* privdata);
    virtual void on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) override;

    // database callback.
    static void on_mysql_lib_exec_callback(const MysqlAsyncConn* c, sql_task_t* task);
    static void on_mysql_lib_query_callback(const MysqlAsyncConn* c, sql_task_t* task, MysqlResult* res);

    virtual void on_mysql_exec_callback(const MysqlAsyncConn* c, sql_task_t* task) override;
    virtual void on_mysql_query_callback(const MysqlAsyncConn* c, sql_task_t* task, MysqlResult* res) override;

    // socket.
    virtual bool send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) override;
    virtual bool send_to(std::shared_ptr<Connection> c, const MsgHead& head, const MsgBody& body) override;

    // redis.
    virtual bool redis_send_to(const char* node, Cmd* cmd, const std::vector<std::string>& argv) override;

    // database
    virtual bool db_exec(const char* node, const char* sql, Cmd* cmd) override;
    virtual bool db_query(const char* node, const char* sql, Cmd* cmd) override;

   private:
    void close_fd(int fd);
    void check_wait_send_fds();
    bool create_events(INet* s, int fd1, int fd2, Codec::TYPE codec, bool is_worker);

    // socket.
    int listen_to_port(const char* bind, int port);
    void accept_server_conn(int fd);
    void accept_and_transfer_fd(int fd);
    void read_transfer_fd(int fd);
    bool read_query_from_client(int fd);
    bool process_message(std::shared_ptr<Connection>& c);

    // connection.
    std::shared_ptr<Connection> create_conn(int fd);
    bool close_conn(std::shared_ptr<Connection> c);
    void close_conns();

   private:
    Log* m_logger = nullptr;      // logger.
    Events* m_events = nullptr;   // libev's events manager.
    uint64_t m_seq = 0;           // sequence.
    char m_errstr[ANET_ERR_LEN];  // error string.

    int m_bind_fd = -1;          // inner servers contact each other.
    int m_gate_bind_fd = -1;     // gate bind fd for client.
    int m_manager_ctrl_fd = -1;  // chanel fd use for worker.
    int m_manager_data_fd = -1;  // chanel fd use for worker.

    TYPE m_type = TYPE::UNKNOWN;                                    // owner type
    WorkerDataMgr* m_woker_data_mgr = nullptr;                      // manager handle worker data.
    std::unordered_map<int, std::shared_ptr<Connection> > m_conns;  // key: fd, value: connection.
    double m_keep_alive = IO_TIMEOUT_VAL;                           // io timeout time.

    Codec::TYPE m_gate_codec = Codec::TYPE::PROTOBUF;  // gate codec type.
    ModuleMgr* m_module_mgr = nullptr;
    std::unordered_map<uint64_t, Cmd*> m_cmds;

    ev_timer* m_timer = nullptr;                       // repeat timer for idle handle.
    std::list<chanel_resend_data_t*> m_wait_send_fds;  // sendmsg maybe return -1 and errno == EAGAIN.

    DBMgr* m_db_pool = nullptr;
    RedisMgr* m_redis_pool = nullptr;
    SessionMgr* m_session_mgr = nullptr;
};

}  // namespace kim

#endif  // __NETWORK_H__