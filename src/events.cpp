#include "events.h"

#include <ev.h>
#include <signal.h>
#include <unistd.h>

#include "context.h"

namespace kim {

Events::Events(std::shared_ptr<Log> logger) : m_logger(logger) {
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

void Events::create_signal_event(int signum, ISignalCallBack* s) {
    LOG_DEBUG("create_signal_event, sig: %d", signum);
    if (m_ev_loop == nullptr) {
        return;
    }

    ev_signal* sig = new ev_signal();
    ev_signal_init(sig, on_signal_callback, signum);
    sig->data = s;
    ev_signal_start(m_ev_loop, sig);
}

bool Events::setup_signal_events(ISignalCallBack* s) {
    LOG_DEBUG("setup_signal_events()");
    if (s == nullptr) {
        return false;
    }

    int signals[] = {SIGCHLD, SIGILL, SIGBUS, SIGFPE, SIGKILL};
    for (unsigned int i = 0; i < sizeof(signals) / sizeof(int); i++) {
        create_signal_event(signals[i], s);
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

bool Events::add_read_event(int fd, ev_io** w, IEventsCallback* cb) {
    if (*w == nullptr) {
        *w = (ev_io*)malloc(sizeof(ev_io));
        if (w == nullptr) {
            LOG_ERROR("new ev_io failed!");
            return false;
        }

        ev_io_init(*w, on_io_callback, fd, EV_READ);
        ev_io_start(m_ev_loop, *w);

        LOG_DEBUG("start ev io, fd: %d", fd);
    } else {
        if (ev_is_active(*w)) {
            ev_io_stop(m_ev_loop, *w);
            ev_io_set(*w, (*w)->fd, (*w)->events | EV_READ);
            ev_io_start(m_ev_loop, *w);
        } else {
            ev_io_init(*w, on_io_callback, fd, EV_READ);
            ev_io_start(m_ev_loop, *w);
        }

        LOG_DEBUG("restart ev io, fd: %d", fd);
    }

    (*w)->data = cb;
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

}  // namespace kim