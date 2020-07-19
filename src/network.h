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
#include "net.h"
#include "net/anet.h"
#include "node_info.h"
#include "worker_data_mgr.h"

namespace kim {

class Network : public ICallback, public INet {
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

    // for io events call back. ICallback
    virtual void on_io_read(int fd) override;
    virtual void on_io_write(int fd) override;
    virtual void on_io_error(int fd) override;
    virtual void on_timer(void* privdata) override;

    // net
    virtual bool send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) override;

    bool set_gate_codec_type(Codec::TYPE type);
    void set_keep_alive(long long keep_alive) { m_keep_alive = keep_alive; }
    long long get_keep_alive() { return m_keep_alive; }

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

    int get_new_seq() { return ++m_seq; }

   private:
    Log* m_logger = nullptr;     // logger.
    Events* m_events = nullptr;  // libev's events manager.
    uint64_t m_seq = 0;          // sequence.
    char m_err[ANET_ERR_LEN];    // error string.

    int m_bind_fd = -1;          // inner servers contact each other.
    int m_gate_bind_fd = -1;     // gate bind fd for client.
    int m_manager_ctrl_fd = -1;  // chanel fd use for worker.
    int m_manager_data_fd = -1;  // chanel fd use for worker.

    TYPE m_type = TYPE::UNKNOWN;                                    // owner type
    WorkerDataMgr* m_woker_data_mgr = nullptr;                      // manager handle worker data.
    std::unordered_map<int, std::shared_ptr<Connection> > m_conns;  // key: fd, value: connection.
    long long m_keep_alive = 0;

    Codec::TYPE m_gate_codec_type = Codec::TYPE::PROTOBUF;
    std::list<Module*> m_core_modules;
};

}  // namespace kim

#endif  // __NETWORK_H__