#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <ev.h>

#include "log.h"
#include "server.h"

namespace kim {

typedef void(ev_cb_fn)(struct ev_signal* watcher);

typedef struct signal_callback_info_s {
    ev_cb_fn* fn_terminated;
    ev_cb_fn* fn_child_terminated;
} signal_callback_info_t;

class Events {
   public:
    Events(kim::Log* logger);
    virtual ~Events();

    bool init(ev_cb_fn* fn_terminated, ev_cb_fn* fn_child_terminated);
    bool create_events();
    void run();
    void close_listen_sockets();

   private:
    void create_ev_signal(int signum);
    static void signal_callback(struct ev_loop* loop, struct ev_signal* watcher, int revents);

   private:
    kim::Log* m_logger;
    struct ev_loop* m_ev_loop;
    signal_callback_info_t* m_sig_cb_info;
};

}  // namespace kim

#endif
