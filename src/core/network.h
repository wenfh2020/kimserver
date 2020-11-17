#ifndef __KIM_NETWORK_H__
#define __KIM_NETWORK_H__

#include <sys/socket.h>

#include "connection.h"
#include "db/db_mgr.h"
#include "events.h"
#include "module_mgr.h"
#include "net/anet.h"
#include "net/chanel.h"
#include "nodes.h"
#include "redis/redis_mgr.h"
#include "session.h"
#include "sys_cmd.h"
#include "worker_data_mgr.h"

namespace kim {

class Network : public INet {
   public:
    enum class TYPE {
        UNKNOWN = 0,
        MANAGER,
        WORKER,
    };

    /* manager transfer fd to worker by sendmsg, 
     * when ret == -1 && errno == EAGIN, resend again. */
    typedef struct chanel_resend_data_s {
        channel_t ch;
        int count = 0;
    } chanel_resend_data_t;

    Network(Log* logger, TYPE type);
    virtual ~Network();

    Network(const Network&) = delete;
    Network& operator=(const Network&) = delete;

    /* for worker. */
    bool create_w(INet* net, const CJsonObject& config, int ctrl_fd, int data_fd, int index);
    /* for manager. */
    bool create_m(const addr_info* ainfo, INet* net, const CJsonObject& config, WorkerDataMgr* m);
    void destory();

    /* init. */
    bool load_config(const CJsonObject& config);
    bool load_timer(INet* net);
    bool load_modules();
    bool load_db();
    bool load_redis_mgr();
    bool load_public();

    /* current time. */
    virtual double now() override;

    /* events. */
    void run();
    void end_ev_loop();
    virtual Events* events() override;
    std::shared_ptr<Connection> add_read_event(int fd, Codec::TYPE codec, bool is_chanel = false);

    virtual std::string node_type() override { return m_node_type; }
    virtual std::string node_inner_host() override { return m_node_inner_host; }
    virtual int node_inner_port() override { return m_node_inner_port; }
    virtual int worker_index() override { return m_worker_index; }

    virtual void add_client_conn(const std::string& node_id, std::shared_ptr<Connection> c) override;

    /* session */
    virtual bool add_session(Session* s) override;
    virtual Session* get_session(const std::string& sessid, bool re_active = false) override;
    virtual bool del_session(const std::string& sessid) override;

    /* manager and worker contack by socketpair. */
    void close_chanel(int* fds);
    /* close the parent's fds in child. */
    void close_fds();
    /* close socket connection. */
    bool close_conn(int fd);

    /* woker or manager type */
    bool is_worker() override { return m_type == TYPE::WORKER; }
    bool is_manager() override { return m_type == TYPE::MANAGER; }

    bool set_gate_codec(const std::string& codec);
    void set_keep_alive(double secs) { m_keep_alive = secs; }
    double keep_alive() { return m_keep_alive; }

   public:
    virtual uint64_t new_seq() override { return ++m_seq; }

    /* cmd. */
    virtual bool add_cmd(Cmd* cmd) override;
    virtual Cmd* get_cmd(uint64_t id) override;
    virtual bool del_cmd(Cmd* cmd) override;

    bool is_request(int cmd) { return (cmd & 0x00000001); }

    /* cmd timer. */
    virtual ev_timer* add_cmd_timer(double secs, ev_timer* w, void* privdata) override;
    virtual bool del_cmd_timer(ev_timer* w) override;

    /* libev callback. */
    static void on_io_callback(struct ev_loop* loop, ev_io* w, int events);
    static void on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents);
    static void on_io_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    static void on_cmd_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    static void on_session_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    static void on_repeat_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);

    // io callback.
    virtual void on_io_read(int fd) override;
    virtual void on_io_write(int fd) override;

    // timer callback.
    virtual void on_io_timer(void* privdata) override;
    virtual void on_cmd_timer(void* privdata) override;
    virtual void on_session_timer(void* privdata) override;
    virtual void on_repeat_timer(void* privdata) override;

    /* socket. */
    virtual bool send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) override;
    virtual bool send_to(std::shared_ptr<Connection> c, const MsgHead& head, const MsgBody& body) override;
    virtual bool auto_send(const std::string& ip, int port, int worker_index, const MsgHead& head, const MsgBody& body) override;

    /* redis. */
    virtual bool redis_send_to(const char* node, Cmd* cmd, const std::vector<std::string>& argv) override;

    /* redis callback. */
    static void on_redis_lib_callback(redisAsyncContext* c, void* reply, void* privdata);
    virtual void on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) override;

    /* database. */
    virtual bool db_exec(const char* node, const char* sql, Cmd* cmd) override;
    virtual bool db_query(const char* node, const char* sql, Cmd* cmd) override;

    /* database callback. */
    static void on_mysql_lib_exec_callback(const MysqlAsyncConn* c, sql_task_t* task);
    static void on_mysql_lib_query_callback(const MysqlAsyncConn* c, sql_task_t* task, MysqlResult* res);
    virtual void on_mysql_exec_callback(const MysqlAsyncConn* c, sql_task_t* task) override;
    virtual void on_mysql_query_callback(const MysqlAsyncConn* c, sql_task_t* task, MysqlResult* res) override;

   private:
    void close_fd(int fd);
    void check_wait_send_fds();
    bool create_events(INet* s, int fd1, int fd2, Codec::TYPE codec, bool is_worker);

    ev_io* add_write_event(std::shared_ptr<Connection>& c);

    // socket.
    int listen_to_port(const char* ip, int port);
    void accept_server_conn(int fd);
    void accept_and_transfer_fd(int fd);
    void read_transfer_fd(int fd);
    bool read_query_from_client(int fd);
    bool process_message(std::shared_ptr<Connection>& c);
    bool process_tcp_message(std::shared_ptr<Connection>& c);
    bool process_http_message(std::shared_ptr<Connection>& c);
    bool handle_write_events(std::shared_ptr<Connection>& c, Codec::STATUS status);

    // connection.
    std::shared_ptr<Connection> create_conn(int fd);
    bool close_conn(std::shared_ptr<Connection> c);
    void close_conns();

    /* timer */
    bool add_io_timer(std::shared_ptr<Connection> c, double secs);

   private:
    Log* m_logger = nullptr;
    Events* m_events = nullptr;  /* libev's event manager. */
    uint64_t m_seq = 0;          /* cur increasing sequence. */
    char m_errstr[ANET_ERR_LEN]; /* error string. */

    int m_node_host_fd = -1;    /* host for inner servers. */
    int m_gate_host_fd = -1;    /* gate host fd for client. */
    int m_manager_ctrl_fd = -1; /* chanel fd use for worker. */
    int m_manager_data_fd = -1; /* chanel fd use for worker. */

    TYPE m_type = TYPE::UNKNOWN;                                                /* owner type. */
    double m_keep_alive = IO_TIMEOUT_VAL;                                       /* io timeout time. */
    WorkerDataMgr* m_woker_data_mgr = nullptr;                                  /* manager handle worker data. */
    Codec::TYPE m_gate_codec = Codec::TYPE::UNKNOWN;                            /* gate codec type. */
    std::unordered_map<int, std::shared_ptr<Connection> > m_conns;              /* key: fd, value: connection. */
    std::unordered_map<std::string, std::shared_ptr<Connection> > m_node_conns; /* key: node_id */

    ModuleMgr* m_module_mgr = nullptr;
    std::unordered_map<uint64_t, Cmd*> m_cmds; /* key: cmd id. */

    ev_timer* m_timer = nullptr;                      /* repeat timer for idle handle. */
    std::list<chanel_resend_data_t*> m_wait_send_fds; /* sendmsg maybe return -1 and errno == EAGAIN. */

    DBMgr* m_db_pool = nullptr;          /* data base connection pool. */
    RedisMgr* m_redis_pool = nullptr;    /* redis connection pool. */
    SessionMgr* m_session_mgr = nullptr; /* session pool. */
    Nodes* m_nodes = nullptr;            /* server nodes. */
    SysCmd* m_sys_cmd = nullptr;

    std::string m_node_type;
    std::string m_node_inner_host;
    int m_node_inner_port;
    int m_worker_index = 0;
};

}  // namespace kim

#endif  // __KIM_NETWORK_H__