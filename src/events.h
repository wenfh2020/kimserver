#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <ev.h>

#include <map>

#include "context.h"
#include "events_callback.h"
#include "log.h"
#include "server.h"

namespace kim {

class Events {
   public:
    Events(Log* logger);
    virtual ~Events();

    bool create(ISignalCallBack* s, IEventsCallback* e);
    void destory();
    void run();
    void end_ev_loop();

    bool add_read_event(Connection* c);
    bool del_event(Connection* c);

   private:
    bool setup_signal_events(ISignalCallBack* s);
    void create_signal_events(int signum, ISignalCallBack* s);

    static void event_callback(struct ev_loop* loop, struct ev_io* e, int events);
    static void signal_callback(struct ev_loop* loop, struct ev_signal* watcher, int revents);

   private:
    Log* m_logger;
    struct ev_loop* m_ev_loop;
    IEventsCallback* m_ev_cb;
};

}  // namespace kim

#endif
