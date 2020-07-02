#ifndef __EVENTS_CALLBAK_H__
#define __EVENTS_CALLBAK_H__

#include <ev.h>

#include "context.h"

namespace kim {

typedef struct async_data_s {
    async_data_s() : m_fd(-1), m_seq(0) {}
    int m_fd;
    uint64_t m_seq;
} async_data_t;

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
    enum class TYPE {
        UNKNOWN = 0,
        MANAGER,
        WORKER,
    };

    IEventsCallback() : m_type(TYPE::UNKNOWN) {}
    virtual ~IEventsCallback() {}

    TYPE get_type() { return m_type; }
    void set_type(TYPE type) { m_type = type; }

    // socket's io event callback.
    virtual void on_io_read(int fd) {}
    virtual void on_io_write(int fd) {}
    virtual void on_io_error(int fd) {}

   private:
    TYPE m_type;
};

}  // namespace kim

#endif  //__EVENTS_CALLBAK_H__