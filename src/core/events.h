#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <ev.h>
#include <hiredis/async.h>

#include "net.h"
#include "server.h"
#include "util/log.h"

namespace kim {

class Events {
   public:
    Events(Log* logger);
    virtual ~Events();

    Events(const Events&) = delete;
    Events& operator=(const Events&) = delete;

    /* callback fn type. */
    typedef void (*cb_io)(struct ev_loop*, ev_io*, int);
    typedef void (*cb_sig)(struct ev_loop*, ev_signal*, int);
    typedef void (*cb_timer)(struct ev_loop*, ev_timer*, int);

    bool create();
    void run();
    void end_ev_loop();
    struct ev_loop* ev_loop() {
        return m_ev_loop;
    }
    double now();

    /* io */
    ev_io* add_read_event(int fd, ev_io* w, void* privdata);
    ev_io* add_write_event(int fd, ev_io* w, void* privdata);
    bool del_write_event(ev_io* w);
    bool del_io_event(ev_io* w);
    bool stop_io_event(ev_io* w);

    /* signal */
    bool setup_signal_events(void* privdata);
    bool create_signal_event(int signum, void* privdata);

    /* timer */
    ev_timer* add_io_timer(double secs, ev_timer* w, void* privdata);
    ev_timer* add_repeat_timer(double secs, ev_timer* w, void* privdata);
    ev_timer* add_cmd_timer(double secs, ev_timer* w, void* privdata);
    ev_timer* add_session_timer(double secs, ev_timer* w, void* privdata);
    ev_timer* add_timer_event(double secs, ev_timer* w, cb_timer tcb, void* privdata, int repeat_secs = 0);
    bool restart_timer(double secs, ev_timer* w, void* privdata);
    bool del_timer_event(ev_timer* w);

    /* set callback fn. */
    void set_io_callback_fn(cb_io fn) { m_io_callback_fn = fn; }
    void set_sig_callback_fn(cb_sig fn) { m_sig_callback_fn = fn; }
    void set_io_timer_callback_fn(cb_timer fn) { m_io_timer_callback_fn = fn; }
    void set_cmd_timer_callback_fn(cb_timer fn) { m_cmd_timer_callback_fn = fn; }
    void set_repeat_timer_callback_fn(cb_timer fn) { m_repeat_timer_callback_fn = fn; }
    void set_session_timer_callback_fn(cb_timer fn) { m_session_timer_callback_fn = fn; }

   private:
    void destory();

   private:
    Log* m_logger = nullptr;
    struct ev_loop* m_ev_loop = nullptr;

    /* callback fn. */
    cb_io m_io_callback_fn = nullptr;
    cb_sig m_sig_callback_fn = nullptr;
    cb_timer m_io_timer_callback_fn = nullptr;
    cb_timer m_cmd_timer_callback_fn = nullptr;
    cb_timer m_repeat_timer_callback_fn = nullptr;
    cb_timer m_session_timer_callback_fn = nullptr;
};

}  // namespace kim

#endif
