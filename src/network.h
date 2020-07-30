#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <iostream>
#include <list>
#include <unordered_map>

#include "callback.h"
#include "codec/codec.h"
#include "context.h"
#include "events.h"
#include "module.h"
#include "net/anet.h"
#include "net/chanel.h"
#include "node_info.h"
#include "redis_context.h"
#include "worker_data_mgr.h"

namespace kim {

typedef struct chanel_resend_data_s {
    channel_t ch;
    int count = 0;
} chanel_resend_data_t;

class Network : public ICallback {
   public:
    enum class TYPE {
        UNKNOWN = 0,
        MANAGER,
        WORKER,
    };

    Network(Log* logger, TYPE type);
    virtual ~Network();

    // for manager.
    bool create(const AddrInfo* addr_info,
                Codec::TYPE code_type, ICallback* s, WorkerDataMgr* m);
    // for worker.
    bool create(ICallback* s, int ctrl_fd, int data_fd);

    void destory();

    bool load_modules();
    bool load_timer();

    // events.
    void run();
    void end_ev_loop();
    std::shared_ptr<Connection> add_read_event(
        int fd, Codec::TYPE codec_type, bool is_chanel = false);

    // socket.
    void close_chanel(int* fds);
    void close_fds();  // for child to close the parent's fds when fork.
    bool close_conn(int fd);

    // owner type
    bool is_worker() { return m_type == TYPE::WORKER; }
    bool is_manager() { return m_type == TYPE::MANAGER; }

    // net
    virtual ev_timer* add_cmd_timer(double secs, ev_timer* w, void* privdata) override;
    virtual bool del_cmd_timer(ev_timer* w) override;

    bool set_gate_codec_type(Codec::TYPE type);
    void set_keep_alive(int keep_alive) { m_keep_alive = keep_alive; }
    int get_keep_alive() { return m_keep_alive; }

    virtual cmd_index_data_t* add_cmd_index_data(uint64_t cmd_id, uint64_t module_id) override;
    virtual bool del_cmd_index_data(uint64_t cmd_id) override;

   public:
    // io callback.
    virtual void on_io_read(int fd) override;
    virtual void on_io_write(int fd) override;
    virtual void on_io_error(int fd) override;

    // timer callback.
    virtual void on_io_timer(void* privdata) override;
    virtual void on_cmd_timer(void* privdata) override;
    virtual void on_repeat_timer(void* privdata) override;

    // socket.
    virtual bool send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) override;
    virtual E_RDS_STATUS redis_send_to(
        _cstr& host, int port, _csvector& rds_cmds, cmd_index_data_t* index) override;
    // redis callback.
    virtual void on_redis_connect(const redisAsyncContext* c, int status) override;
    virtual void on_redis_disconnect(const redisAsyncContext* c, int status) override;
    virtual void on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) override;

   private:
    bool create_events(ICallback* s, int fd1, int fd2,
                       Codec::TYPE codec_type, bool is_worker);

    // socket
    int listen_to_port(const char* bind, int port);
    void accept_server_conn(int fd);
    void accept_and_transfer_fd(int fd);
    void read_transfer_fd(int fd);
    bool read_query_from_client(int fd);

    // connection
    std::shared_ptr<Connection> create_conn(int fd);
    bool close_conn(std::shared_ptr<Connection> c);
    void close_conns();

    // redis
    RdsConnection* redis_connect(_cstr& host, int port, void* privdata);

    virtual uint64_t get_new_seq() override { return ++m_seq; }
    void check_wait_send_fds();

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
    int m_keep_alive = IO_TIMER_VAL;

    Codec::TYPE m_gate_codec_type = Codec::TYPE::PROTOBUF;
    std::unordered_map<uint64_t, Module*> m_core_modules;

    ev_timer* m_timer = nullptr;
    std::list<chanel_resend_data_t*> m_wait_send_fds;

    std::unordered_map<std::string, RdsConnection*> m_redis_conns;
    std::unordered_map<uint64_t, cmd_index_data_t*> m_cmd_index_datas;
};

}  // namespace kim

#endif  // __NETWORK_H__