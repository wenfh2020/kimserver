#ifndef __EVENTS_CALLBAK_H__
#define __EVENTS_CALLBAK_H__

#include <ev.h>

#include "context.h"

namespace kim {

class ICallback {
   public:
    ICallback() {}
    virtual ~ICallback() {}

   public:
    uint64_t get_seq() { return ++m_seq; }

    // callback.
    /////////////////////////////////
   public:
    // signal.
    virtual void on_terminated(ev_signal* s) {}
    virtual void on_child_terminated(ev_signal* s) {}

    // socket.
    virtual void on_io_read(int fd) {}
    virtual void on_io_write(int fd) {}
    virtual void on_io_error(int fd) {}

    // timer.
    virtual void on_io_timer(void* privdata) {}
    virtual void on_cmd_timer(void* privdata) {}
    virtual void on_repeat_timer(void* privdata) {}

    // events.
    /////////////////////////////////
   public:
    // socket.
    virtual bool send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) { return true; }

    // events
    virtual ev_timer* add_cmd_timer(double secs, ev_timer* w, void* privdata) { return nullptr; }
    virtual bool del_cmd_timer(ev_timer*) { return false; }

   private:
    uint64_t m_seq = 0;
};

}  // namespace kim

#endif  //__EVENTS_CALLBAK_H__