#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <ev.h>

#include "log.h"
#include "network.h"
#include "server.h"

namespace kim {

typedef void(ev_cb_fn)(struct ev_signal* watcher);

typedef struct signal_callback_info_s {
    signal_callback_info_s() {
        fn_terminated = NULL;
        fn_child_terminated = NULL;
    }
    ev_cb_fn* fn_terminated;
    ev_cb_fn* fn_child_terminated;
} signal_callback_info_t;

class Events {
   public:
    Events(Log* logger);
    virtual ~Events();

    bool create(const addr_info_t* addr_info);
    void destory();
    void run();

    bool add_io_event(int fd);
    void close_listen_sockets();
    void close_chanel(int* fds);

    void set_cb_terminated(ev_cb_fn* cb);
    void set_cb_child_terminated(ev_cb_fn* cb);
    int get_new_seq() { return ++m_seq; }

   private:
    bool init_network(const addr_info_t* addr_info);
    bool create_events();
    void create_ev_signal(int signum);
    static void signal_callback(struct ev_loop* loop, struct ev_signal* watcher, int revents);

   private:
    Log* m_logger;
    struct ev_loop* m_ev_loop;
    signal_callback_info_t* m_sig_cb_info;
    Network* m_network;
    std::list<int> m_fds;
    int m_seq;
};

}  // namespace kim

#endif
