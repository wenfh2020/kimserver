#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <ev.h>

#include <map>

#include "context.h"
#include "log.h"
#include "network.h"
#include "server.h"
#include "signal_callback.h"

namespace kim {

class Events {
   public:
    Events(Log* logger);
    virtual ~Events();

    bool create(const addr_info_t* addr_info, ISignalCallback* s);
    void destory();
    void run();

    Connection* create_conn(int fd);
    void close_conns();

    bool add_read_event(Connection* c);
    bool add_chanel_event(int fd);
    void close_listen_sockets();
    void close_chanel(int* fds);

    int get_new_seq() { return ++m_seq; }

    static void event_callback(struct ev_loop* loop, struct ev_io* e, int events);

    // socket io event callback.
    bool io_read(Connection* c, struct ev_io* e);
    bool io_write(Connection* c, struct ev_io* e);
    bool io_error(Connection* c, struct ev_io* e);

   private:
    bool init_network(const addr_info_t* addr_info);
    bool setup_signal_events(ISignalCallback* s);
    void create_ev_signal(int signum);
    static void signal_callback(struct ev_loop* loop, struct ev_signal* watcher, int revents);

   private:
    Log* m_logger;
    struct ev_loop* m_ev_loop;
    Network* m_net;
    std::list<int> m_listen_fds;
    uint64_t m_seq;
    std::map<int, Connection*> m_conns;
    ISignalCallback* m_sig_cb;
};

}  // namespace kim

#endif
