#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <iostream>
#include <list>

#include "context.h"
#include "events.h"
#include "events_callback.h"
#include "log.h"
#include "net/anet.h"
#include "node_info.h"

namespace kim {

class Network : public IEventsCallback {
   public:
    Network(Log* logger = NULL);
    virtual ~Network();

    bool create(const addr_info_t* addr_info, ISignalCallBack* s = NULL);
    void run();
    void end_ev_loop();
    void destory();

    void close_listen_sockets();
    void close_chanel(int* fds);

    // for io events call back. IEventsCallback
    virtual bool io_read(Connection* c, struct ev_io* e);
    virtual bool io_write(Connection* c, struct ev_io* e);
    virtual bool io_error(Connection* c, struct ev_io* e);

   private:
    bool init_events(ISignalCallBack* s);

    // socket
    int listen_to_port(const char* bind, int port);
    bool accept_server_conn(int fd);
    void accept_tcp_handler(int fd);

    // connection
    Connection* create_conn(int fd);
    bool close_conn(Connection* c);
    void close_conns();
    void read_query_from_client(Connection* c);

    // events
    bool add_chanel_event(int fd);
    bool add_conncted_read_event(int fd);

    int get_new_seq() { return ++m_seq; }

   private:
    Log* m_logger;                       // log manager.
    uint64_t m_seq;                      // sequence.
    char m_err[ANET_ERR_LEN];            // error string.
    int m_bind_fd;                       // inner servers contact each other.
    int m_gate_bind_fd;                  // gate bind fd for client.
    Events* m_events;                    // libev's events manager.
    std::map<int, Connection*> m_conns;  // key:fd, value: connection
};

}  // namespace kim

#endif  // __NETWORK_H__