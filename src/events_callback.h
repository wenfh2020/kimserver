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
    enum class OBJ_TYPE {
        UNKNOWN = 0,
        MANAGER,
        WORKER,
    };

    IEventsCallback() : m_type(OBJ_TYPE::UNKNOWN) {}
    virtual ~IEventsCallback() {}

    OBJ_TYPE get_type() { return m_type; }
    void set_type(OBJ_TYPE type) { m_type = type; }

    // socket's io event callback.
    virtual bool on_io_read(Connection* c, struct ev_io* e) { return true; }
    virtual bool on_io_write(Connection* c, struct ev_io* e) { return true; }
    virtual bool on_io_error(Connection* c, struct ev_io* e) { return true; }

   private:
    OBJ_TYPE m_type;
};

}  // namespace kim

#endif  //__EVENTS_CALLBAK_H__