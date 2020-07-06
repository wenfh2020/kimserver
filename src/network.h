#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <iostream>
#include <unordered_map>

#include "context.h"
#include "events.h"
#include "events_callback.h"
#include "net/anet.h"
#include "node_info.h"
#include "worker_data_mgr.h"

namespace kim {

class Network : public IEventsCallback {
   public:
    enum class TYPE {
        UNKNOWN = 0,
        MANAGER,
        WORKER,
    };

    Network(std::shared_ptr<Log> logger, TYPE type);
    virtual ~Network();

    // for manager.
    bool create(const addr_info_t* addr_info, ISignalCallBack* s, WorkerDataMgr* m);
    // for worker.
    bool create(ISignalCallBack* s, int ctrl_fd, int data_fd);
    void destory();

    // events.
    void run();
    void end_ev_loop();
    bool add_conncted_read_event(int fd, bool is_chanel = false);

    // socket.
    void close_chanel(int* fds);
    void close_fds();  // for child to close the parent's fds when fork.
    bool close_conn(int fd);

    // owner type
    TYPE get_type() { return m_type; }
    void set_type(TYPE type) { m_type = type; }
    bool is_worker() { return m_type == TYPE::WORKER; }
    bool is_manager() { return m_type == TYPE::MANAGER; }

    // for io events call back. IEventsCallback
    void on_io_read(int fd) override;
    void on_io_write(int fd) override;
    void on_io_error(int fd) override;

   private:
    bool create_events(ISignalCallBack* s, int fd1, int fd2, bool is_worker);

    // socket
    int listen_to_port(const char* bind, int port);
    void accept_server_conn(int fd);
    void accept_and_transfer_fd(int fd);
    void read_transfer_fd(int fd);
    void read_query_from_client(int fd);

    // connection
    Connection* create_conn(int fd);
    bool close_conn(Connection* c);
    void close_conns();

    int get_new_seq() { return ++m_seq; }

   private:
    std::shared_ptr<Log> m_logger = nullptr;       // log manager.
    Events* m_events = nullptr;                    // libev's events manager.
    uint64_t m_seq = 0;                            // sequence.
    char m_err[ANET_ERR_LEN];                      // error string.
    int m_bind_fd = -1;                            // inner servers contact each other.
    int m_gate_bind_fd = -1;                       // gate bind fd for client.
    std::unordered_map<int, Connection*> m_conns;  // key:fd, value: connection
    int m_manager_ctrl_fd = -1;                    // chanel fd use for worker.
    int m_manager_data_fd = -1;                    // chanel fd use for worker.
    WorkerDataMgr* m_woker_data_mgr = nullptr;
    TYPE m_type = TYPE::UNKNOWN;
};

}  // namespace kim

#endif  // __NETWORK_H__