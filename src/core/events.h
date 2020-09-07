#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <ev.h>
#include <hiredis/async.h>

#include "net.h"
#include "server.h"
#include "util/log.h"

namespace kim {

typedef void (*timer_cb)(struct ev_loop*, ev_timer*, int);

class Events {
   public:
    Events(Log* logger);
    virtual ~Events();

    bool create();
    void destory();
    void run();
    void end_ev_loop();
    struct ev_loop* ev_loop() {
        return m_ev_loop;
    }
    double now();

    // io
    ev_io* add_read_event(int fd, ev_io* w, void* privdata);
    ev_io* add_write_event(int fd, ev_io* w, void* privdata);
    bool del_write_event(ev_io* w);
    bool del_io_event(ev_io* w);
    bool stop_event(ev_io* w);

    // signal
    bool setup_signal_events(void* privdata);
    void create_signal_event(int signum, void* privdata);

    // timer
    ev_timer* add_io_timer(double secs, ev_timer* w, void* privdata);
    ev_timer* add_repeat_timer(double secs, ev_timer* w, void* privdata);
    ev_timer* add_cmd_timer(double secs, ev_timer* w, void* privdata);
    ev_timer* add_session_timer(double secs, ev_timer* w, void* privdata);
    ev_timer* add_timer_event(double secs, ev_timer* w, timer_cb tcb, void* privdata, int repeat_secs = 0);
    bool restart_timer(double secs, ev_timer* w, void* privdata);
    bool del_timer_event(ev_timer* w);

   private:
    // libev callback.
    static void on_io_callback(struct ev_loop* loop, ev_io* w, int events);
    static void on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents);
    static void on_io_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    static void on_cmd_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    static void on_session_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    static void on_repeat_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);

   private:
    Log* m_logger = nullptr;
    struct ev_loop* m_ev_loop = nullptr;
};

}  // namespace kim

#endif
