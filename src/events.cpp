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

    ISignalCallBack* cb = static_cast<ISignalCallBack*>(s->data);
    (s->signum == SIGCHLD) ? cb->on_child_terminated(s) : cb->on_terminated(s);
}

ev_io* Events::add_read_event(int fd, void* privdata) {
    ev_io* w = (ev_io*)malloc(sizeof(ev_io));
    if (w == nullptr) {
        LOG_ERROR("alloc ev_io failed!");
        return nullptr;
    }

    ev_io_init(w, on_io_callback, fd, EV_READ);
    ev_io_start(m_ev_loop, w);
    w->data = privdata;

    LOG_DEBUG("restart ev io, fd: %d", fd);
    return w;
}

bool Events::restart_read_event(ev_io* w, int fd, void* privdata) {
    if (w == nullptr) {
        return false;
    }
    if (ev_is_active(w)) {
        ev_io_stop(m_ev_loop, w);
        ev_io_set(w, w->fd, w->events | EV_READ);
        ev_io_start(m_ev_loop, w);
    } else {
        ev_io_init(w, on_io_callback, fd, EV_READ);
        ev_io_start(m_ev_loop, w);
    }
    w->data = privdata;
    return false;
}

bool Events::add_timer_event(ev_tstamp secs, ev_timer** w, void* privdata) {
    if (*w == nullptr) {
        *w = (ev_timer*)malloc(sizeof(ev_timer));
        if (w == nullptr) {
            LOG_ERROR("alloc timer failed!");
            return false;
        }
    }

    if (ev_is_active(*w)) {
        ev_timer_stop(m_ev_loop, *w);
        ev_timer_set(*w, secs + ev_time() - ev_now(m_ev_loop), 0);
        ev_timer_start(m_ev_loop, *w);
    } else {
        ev_timer_init(*w, on_timer_callback, secs + ev_time() - ev_now(m_ev_loop), 0.);
        ev_timer_start(m_ev_loop, *w);
    }
    (*w)->data = privdata;

    LOG_DEBUG("start timer, seconds: %d", secs);
    return true;
}

bool Events::restart_timer(int secs, ev_timer* w) {
    if (w == nullptr) {
        return false;
    }
    ev_timer_stop(m_ev_loop, w);
    ev_timer_set(w, secs + ev_time() - ev_now(m_ev_loop), 0);
    ev_timer_start(m_ev_loop, w);
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
    IEventsCallback* cb = static_cast<IEventsCallback*>(w->data);

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

void Events::on_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    IEventsCallback* cb;
    std::shared_ptr<Connection> c;

    if (w->data != nullptr) {
        c = static_cast<ConnectionData*>(w->data)->m_conn;
        if (c != nullptr) {
            cb = static_cast<IEventsCallback*>(c->get_private_data());
            if (cb != nullptr) {
                cb->on_timer(w->data);
            }
        }
    }
}

}  // namespace kim