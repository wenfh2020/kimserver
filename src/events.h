#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <ev.h>

#include <map>

#include "context.h"
#include "events_callback.h"
#include "server.h"
#include "util/log.h"

namespace kim {

class Events {
   public:
    Events(Log* logger);
    virtual ~Events();

    bool create(IEventsCallback* e);
    void destory();
    void run();
    void end_ev_loop();

    bool add_read_event(Connection* c);
    bool del_event(Connection* c);
    bool setup_signal_events(ISignalCallBack* s);
    void create_signal_event(int signum, ISignalCallBack* s);

   private:
    static void on_io_callback(struct ev_loop* loop, ev_io* e, int events);
    static void on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents);

   private:
    Log* m_logger;
    struct ev_loop* m_ev_loop;
    IEventsCallback* m_ev_cb;
};

}  // namespace kim

#endif
