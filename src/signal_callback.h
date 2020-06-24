#ifndef __SIGNAL_CALLBAK_H__
#define __SIGNAL_CALLBAK_H__

#include <ev.h>

class ISignalCallback {
   public:
    ISignalCallback() {}
    ~ISignalCallback() {}

    virtual void on_terminated(struct ev_signal* watcher) {}
    virtual void on_child_terminated(struct ev_signal* watcher) {}
};

#endif  //__SIGNAL_CALLBAK_H__