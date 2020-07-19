#ifndef __EVENTS_CALLBAK_H__
#define __EVENTS_CALLBAK_H__

#include <ev.h>

#include "context.h"

namespace kim {

class ICallback {
   public:
    ICallback() {}
    virtual ~ICallback() {}

    // signal.
    virtual void on_terminated(ev_signal* s) {}
    virtual void on_child_terminated(ev_signal* s) {}

    // socket.
    virtual void on_io_read(int fd) {}
    virtual void on_io_write(int fd) {}
    virtual void on_io_error(int fd) {}

    // timer.
    virtual void on_timer(void* privdata) {}
};

}  // namespace kim

#endif  //__EVENTS_CALLBAK_H__