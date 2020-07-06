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

    bool add_read_event(int fd, ev_io** w, IEventsCallback* cb);
    bool del_event(ev_io* w);
    bool stop_event(ev_io* w);
    bool setup_signal_events(ISignalCallBack* cb);
    void create_signal_event(int signum, ISignalCallBack* cb);

    ev_tstamp get_now_time() { return ev_now(m_ev_loop); }

   private:
    static void on_io_callback(struct ev_loop* loop, ev_io* w, int events);
    static void on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents);

   private:
    Log* m_logger = nullptr;
    struct ev_loop* m_ev_loop = nullptr;
};

}  // namespace kim

#endif
