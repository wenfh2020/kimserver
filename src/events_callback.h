#ifndef __EVENTS_CALLBAK_H__
#define __EVENTS_CALLBAK_H__

#include <ev.h>

#include "context.h"

namespace kim {

class ISignalCallBack {
   public:
    ISignalCallBack() {}
    virtual ~ISignalCallBack() {}

    // signal callback.
    virtual void on_terminated(struct ev_signal* watcher) {}
    virtual void on_child_terminated(struct ev_signal* watcher) {}
};

class IEventsCallback {
   public:
    IEventsCallback() {}
    virtual ~IEventsCallback() {}

    // socket's io event callback.
    virtual bool io_read(Connection* c, struct ev_io* e) { return true; }
    virtual bool io_write(Connection* c, struct ev_io* e) { return true; }
    virtual bool io_error(Connection* c, struct ev_io* e) { return true; }
};

}  // namespace kim

#endif  //__EVENTS_CALLBAK_H__