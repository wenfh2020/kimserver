#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <ev.h>

#include <map>
#include <memory>

#include "context.h"
#include "events_callback.h"
#include "server.h"
#include "util/log.h"

namespace kim {

class Events {
   public:
    Events(Log* logger);
    virtual ~Events();

    bool create();
    void destory();
    void run();
    void end_ev_loop();

    // io
    ev_io* add_read_event(int fd, void* privdata);
    bool restart_read_event(ev_io* w, int fd, void* privadata);
    bool del_event(ev_io* w);
    bool stop_event(ev_io* w);

    // signal
    bool setup_signal_events(void* privdata);
    void create_signal_event(int signum, void* privdata);

    // timer
    ev_timer* add_timer_event(int secs, void* privdata);
    bool restart_timer(int secs, ev_timer* w, void* privdat);
    bool del_event(ev_timer* w);

   private:
    // libev callback.
    static void on_io_callback(struct ev_loop* loop, ev_io* w, int events);
    static void on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents);
    static void on_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);

   private:
    Log* m_logger = nullptr;
    struct ev_loop* m_ev_loop = nullptr;
};

}  // namespace kim

#endif
