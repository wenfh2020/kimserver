#ifndef __KIM_EVENTS_CALLBACK_H__
#define __KIM_EVENTS_CALLBACK_H__

#include "net.h"
#include "util/log.h"

namespace kim {

class EventsCallback {
   public:
    EventsCallback();
    EventsCallback(Log* logger, INet* net);
    virtual ~EventsCallback();

    bool init(Log* logger, INet* net);
    bool setup_timer();
    bool setup_signal_events();
    bool setup_signal_event(int sig);
    bool setup_io_callback();
    bool setup_io_timer_callback();
    bool setup_cmd_timer_callback();
    bool setup_session_timer_callback();

   public:
    static void on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents);
    static void on_repeat_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    static void on_io_callback(struct ev_loop* loop, ev_io* w, int events);
    static void on_io_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    static void on_cmd_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    static void on_session_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);

    virtual void on_terminated(ev_signal* s) {}
    virtual void on_child_terminated(ev_signal* s) {}
    virtual void on_io_read(int fd) { printf("88888888\n"); }
    virtual void on_io_write(int fd) { printf("777777777\n"); }
    virtual void on_repeat_timer(void* privdata) {}
    virtual void on_io_timer(void* privdata) {}
    virtual void on_cmd_timer(void* privdata) {}
    virtual void on_session_timer(void* privdata) {}

   protected:
    Log* m_logger = nullptr;     /* logger. */
    INet* m_network = nullptr;   /* Network obj ptr. */
    ev_timer* m_timer = nullptr; /* repeat timer. */
};

}  // namespace kim

#endif  //__KIM_EVENTS_CALLBACK_H__