#ifndef __EVENTS_CALLBAK_H__
#define __EVENTS_CALLBAK_H__

#include <ev.h>

#include "context.h"

namespace kim {

///////////////////////////////////////////////////////

class ISignalCallBack {
   public:
    ISignalCallBack() {}
    virtual ~ISignalCallBack() {}

    // signal callback.
    virtual void on_terminated(ev_signal* s) {}
    virtual void on_child_terminated(ev_signal* s) {}
};

///////////////////////////////////////////////////////

class IEventsCallback {
   public:
    IEventsCallback() {}
    virtual ~IEventsCallback() {}

    // socket's io event callback.
    virtual void on_io_read(int fd) {}
    virtual void on_io_write(int fd) {}
    virtual void on_io_error(int fd) {}

    virtual void on_timer(void* privdata) {}
};

}  // namespace kim

#endif  //__EVENTS_CALLBAK_H__