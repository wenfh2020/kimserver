#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <iostream>
#include <list>
#include <unordered_map>

#include "context.h"
#include "events.h"
#include "events_callback.h"
#include "log.h"
#include "net/anet.h"
#include "node_info.h"
#include "worker_data_mgr.h"

namespace kim {

class Network : public IEventsCallback {
   public:
    Network(Log* logger, IEventsCallback::TYPE type);
    virtual ~Network();

    bool create(const addr_info_t* addr_info, ISignalCallBack* s, WorkerDataMgr* m);
    bool create(ISignalCallBack* s, int ctrl_fd, int data_fd);
    void run();
    void end_ev_loop();
    void destory();

    void close_listen_sockets();
    bool add_chanel_event(int fd);
    void close_chanel(int* fds);

    // for io events call back. IEventsCallback
    bool on_io_read(Connection* c, struct ev_io* e) override;
    bool on_io_write(Connection* c, struct ev_io* e) override;
    bool on_io_error(Connection* c, struct ev_io* e) override;

   private:
    bool create_events(ISignalCallBack* s);

    // socket
    int listen_to_port(const char* bind, int port);
    bool accept_server_conn(int fd);
    void accept_tcp_handler(int fd);
    bool accept_and_transfer_fd(int fd);

    // connection
    Connection* create_conn(int fd);
    bool close_conn(Connection* c);
    void close_conns();
    bool read_query_from_client(Connection* c);

    // events

    bool add_conncted_read_event(int fd);

    int get_new_seq() { return ++m_seq; }

   private:
    Log* m_logger;                                 // log manager.
    uint64_t m_seq;                                // sequence.
    char m_err[ANET_ERR_LEN];                      // error string.
    int m_bind_fd;                                 // inner servers contact each other.
    int m_gate_bind_fd;                            // gate bind fd for client.
    Events* m_events;                              // libev's events manager.
    std::unordered_map<int, Connection*> m_conns;  // key:fd, value: connection
    int m_manager_ctrl_fd;
    int m_manager_data_fd;
    WorkerDataMgr* m_woker_data_mgr;
};

}  // namespace kim

#endif  // __NETWORK_H__