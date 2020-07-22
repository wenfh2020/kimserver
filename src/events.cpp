#include "events.h"

#include <ev.h>
#include <signal.h>
#include <unistd.h>

#include "context.h"

namespace kim {

Events::Events(Log* logger) : m_logger(logger) {
}

Events::~Events() {
    destory();
}

bool Events::create() {
    m_ev_loop = ev_loop_new(EVFLAG_FORKCHECK | EVFLAG_SIGNALFD);
    if (m_ev_loop == nullptr) {
        LOG_ERROR("new libev loop failed!");
        return false;
    }

    return true;
}

void Events::destory() {
    if (m_ev_loop != nullptr) {
        ev_loop_destroy(m_ev_loop);
        m_ev_loop = NULL;
    }
}

void Events::run() {
    if (m_ev_loop != nullptr) {
        ev_run(m_ev_loop, 0);
    }
}

void Events::end_ev_loop() {
    if (m_ev_loop != nullptr) {
        ev_break(m_ev_loop, EVBREAK_ALL);
    }
}

void Events::create_signal_event(int signum, void* privdata) {
    LOG_DEBUG("create_signal_event, sig: %d", signum);
    if (m_ev_loop == nullptr) {
        return;
    }

    ev_signal* sig = new ev_signal();
    ev_signal_init(sig, on_signal_callback, signum);
    sig->data = privdata;
    ev_signal_start(m_ev_loop, sig);
}

bool Events::setup_signal_events(void* privdata) {
    LOG_DEBUG("setup_signal_events()");
    if (privdata == nullptr) {
        return false;
    }

    int signals[] = {SIGCHLD, SIGILL, SIGBUS, SIGFPE, SIGKILL};
    for (unsigned int i = 0; i < sizeof(signals) / sizeof(int); i++) {
        create_signal_event(signals[i], privdata);
    }
    return true;
}

void Events::on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents) {
    if (s == nullptr || s->data == nullptr) {
        return;
    }

    ICallback* cb = static_cast<ICallback*>(s->data);
    (s->signum == SIGCHLD) ? cb->on_child_terminated(s) : cb->on_terminated(s);
}

ev_io* Events::add_read_event(int fd, ev_io* w, void* privdata) {
    if (w == nullptr) {
        w = (ev_io*)malloc(sizeof(ev_io));
        if (w == nullptr) {
            LOG_ERROR("alloc ev_io failed!");
            return nullptr;
        }
        memset(w, 0, sizeof(ev_io));
    }

    if (ev_is_active(w)) {
        ev_io_stop(m_ev_loop, w);
        ev_io_set(w, fd, w->events | EV_READ);
        ev_io_start(m_ev_loop, w);
    } else {
        ev_io_init(w, on_io_callback, fd, EV_READ);
        ev_io_start(m_ev_loop, w);
    }
    w->data = privdata;

    LOG_DEBUG("add read ev io, fd: %d", fd);
    return w;
}

ev_io* Events::add_write_event(int fd, ev_io* w, void* privdata) {
    if (w == nullptr) {
        w = (ev_io*)malloc(sizeof(ev_io));
        if (w == nullptr) {
            LOG_ERROR("alloc ev_io failed!");
            return nullptr;
        }
        memset(w, 0, sizeof(ev_io));
    }

    if (ev_is_active(w)) {
        ev_io_stop(m_ev_loop, w);
        ev_io_set(w, w->fd, w->events | EV_WRITE);
        ev_io_start(m_ev_loop, w);
    } else {
        ev_io_init(w, on_io_callback, fd, EV_WRITE);
        ev_io_start(m_ev_loop, w);
    }
    LOG_DEBUG("add write ev io, fd: %d", fd);
    return w;
}

bool Events::del_write_event(ev_io* w) {
    if (w == nullptr) {
        return false;
    }
    if (w->events | EV_WRITE) {
        ev_io_stop(m_ev_loop, w);
        ev_io_set(w, w->fd, w->events & (~EV_WRITE));
        ev_io_start(m_ev_loop, w);
    }
    return true;
}

ev_timer* Events::add_io_timer(int secs, ev_timer* w, void* privdata) {
    return add_timer_event(secs, w, on_io_timer_callback, privdata);
}

ev_timer* Events::add_repeat_timer(int secs, ev_timer* w, void* privdata) {
    return add_timer_event(secs, w, on_repeat_timer_callback, privdata, secs);
}

ev_timer* Events::add_timer_event(int secs, ev_timer* w, timer_cb tcb, void* privdata, int repeat_secs) {
    if (w == nullptr) {
        w = (ev_timer*)malloc(sizeof(ev_timer));
        if (w == nullptr) {
            LOG_ERROR("alloc timer failed!");
            return nullptr;
        }
        memset(w, 0, sizeof(ev_timer));
    }

    if (ev_is_active(w)) {
        ev_timer_stop(m_ev_loop, w);
        ev_timer_set(w, secs + ev_time() - ev_now(m_ev_loop), repeat_secs);
        ev_timer_start(m_ev_loop, w);
    } else {
        ev_timer_init(w, tcb, secs + ev_time() - ev_now(m_ev_loop), repeat_secs);
        ev_timer_start(m_ev_loop, w);
    }
    w->data = privdata;

    LOG_DEBUG("start timer, seconds: %d", secs);
    return w;
}

bool Events::restart_timer(int secs, ev_timer* w, void* privdata) {
    if (w == nullptr) {
        return false;
    }
    ev_timer_stop(m_ev_loop, w);
    ev_timer_set(w, secs + ev_time() - ev_now(m_ev_loop), 0);
    ev_timer_start(m_ev_loop, w);
    w->data = privdata;

    LOG_DEBUG("restart timer, seconds: %d", secs);
    return true;
}

bool Events::del_event(ev_io* w) {
    if (w == nullptr) {
        return false;
    }

    LOG_DEBUG("delete event, fd: %d", w->fd);

    ev_io_stop(m_ev_loop, w);
    w->data = NULL;
    SAFE_FREE(w);
    return true;
}

bool Events::del_event(ev_timer* w) {
    if (w == nullptr) {
        return false;
    }

    LOG_DEBUG("delete timer event");
    ev_timer_stop(m_ev_loop, w);
    w->data = NULL;
    SAFE_FREE(w);
    return true;
}

bool Events::stop_event(ev_io* w) {
    if (w == nullptr) {
        return false;
    }
    ev_io_stop(m_ev_loop, w);
    return true;
}

void Events::on_io_callback(struct ev_loop* loop, ev_io* w, int events) {
    if (w->data == nullptr) {
        return;
    }

    int fd = w->fd;
    ICallback* cb = static_cast<ICallback*>(w->data);

    if (events & EV_READ) {
        cb->on_io_read(fd);
    }

    if (events & EV_WRITE) {
        cb->on_io_write(fd);
    }

    /* when error happen (read / write),
     * handle EV_READ / EV_WRITE events will be ok. */
    if (events & EV_ERROR) {
        cb->on_io_error(fd);
    }
}

void Events::on_io_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    ICallback* cb;
    std::shared_ptr<Connection> c;

    if (w->data != nullptr) {
        c = static_cast<ConnectionData*>(w->data)->m_conn;
        if (c != nullptr) {
            cb = static_cast<ICallback*>(c->get_private_data());
            if (cb != nullptr) {
                cb->on_io_timer(w->data);
                return;
            }
        }
    }

    ev_timer_stop(loop, w);
}

void Events::on_repeat_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    if (w->data == nullptr) {
        return;
    }
    ICallback* cb = static_cast<ICallback*>(w->data);
    if (cb != nullptr) {
        cb->on_repeat_timer(w->data);
    }
}

}  // namespace kim