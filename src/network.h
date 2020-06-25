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
    void destory();
    bool init_events(ISignalCallBack* s);

    void accept_tcp_handler(int fd, void* privdata);
    void close_chanel(int* fds);
    void close_listen_sockets();
    Connection* create_conn(int fd);
    void close_conns();

    bool add_chanel_event(int fd);
    bool add_conncted_read_event(int fd);

    int get_bind_fd() { return m_bind_fd; }
    int get_gate_bind_fd() { return m_gate_bind_fd; }

    int get_new_seq() { return ++m_seq; }

    // IEventsCallback
    virtual bool io_read(Connection* c, struct ev_io* e);
    virtual bool io_write(Connection* c, struct ev_io* e);
    virtual bool io_error(Connection* c, struct ev_io* e);

   private:
    int listen_to_port(const char* bind, int port);

   private:
    Log* m_logger;
    uint64_t m_seq;
    char m_err[ANET_ERR_LEN];
    int m_bind_fd;
    int m_gate_bind_fd;
    Events* m_events;
    bool m_is_created;
    std::map<int, Connection*> m_conns;
};

}  // namespace kim

#endif  // __NETWORK_H__