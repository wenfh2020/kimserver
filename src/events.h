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

    bool create(ISignalCallBack* sig, IEventsCallback* io);
    void destory();
    void run();

    bool accept_server_conn(int fd);

    bool add_read_event(Connection* c);

    static void event_callback(struct ev_loop* loop, struct ev_io* e, int events);

   private:
    bool setup_signal_events(ISignalCallBack* s);
    void create_ev_signal(int signum);
    static void signal_callback(struct ev_loop* loop, struct ev_signal* watcher, int revents);

   private:
    Log* m_logger;
    struct ev_loop* m_ev_loop;

    ISignalCallBack* m_sig_cb;
    IEventsCallback* m_io_cb;
};

}  // namespace kim

#endif
