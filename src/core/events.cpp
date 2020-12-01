#include "events.h"

#include <ev.h>
#include <signal.h>
#include <unistd.h>

#include "connection.h"
#include "module.h"
#include "session.h"
#include "util/util.h"

namespace kim {

Events::Events(Log* logger) : Logger(logger) {
}

Events::~Events() {
    end_ev_loop();
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
        m_ev_loop = nullptr;
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

double Events::now() {
    return ev_now(m_ev_loop);
}

bool Events::create_signal_event(int signum, void* privdata) {
    LOG_TRACE("create_signal_event, sig: %d", signum);
    if (m_ev_loop == nullptr) {
        LOG_ERROR("pls create events firstly!");
        return false;
    }

    if (m_sig_callback_fn == nullptr) {
        LOG_ERROR("pls set signal callback fn!");
        return false;
    }

    ev_signal* sig = new ev_signal();
    ev_signal_init(sig, m_sig_callback_fn, signum);
    sig->data = privdata;
    ev_signal_start(m_ev_loop, sig);
    return true;
}

bool Events::setup_signal_events(void* privdata) {
    LOG_TRACE("setup_signal_events()");
    if (privdata == nullptr) {
        return false;
    }
    int signals[] = {SIGCHLD, SIGILL, SIGBUS, SIGFPE, SIGKILL};
    for (unsigned int i = 0; i < sizeof(signals) / sizeof(int); i++) {
        if (!create_signal_event(signals[i], privdata)) {
            return false;
        }
    }
    return true;
}

ev_io* Events::add_read_event(int fd, ev_io* w, void* privdata) {
    if (m_io_callback_fn == nullptr) {
        LOG_ERROR("pls set io callback fn.");
        return nullptr;
    }

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
        ev_io_init(w, m_io_callback_fn, fd, EV_READ);
        ev_io_start(m_ev_loop, w);
    }
    w->data = privdata;

    LOG_TRACE("add read ev io, io: %p, fd: %d", w, fd);
    return w;
}

ev_io* Events::add_write_event(int fd, ev_io* w, void* privdata) {
    if (m_io_callback_fn == nullptr) {
        LOG_ERROR("pls set io callback fn.");
        return nullptr;
    }

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
        ev_io_init(w, m_io_callback_fn, fd, EV_WRITE);
        ev_io_start(m_ev_loop, w);
    }
    LOG_TRACE("add write ev io, fd: %d", fd);
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
        LOG_TRACE("del write event, fd: %d", w->fd);
    }

    return true;
}

ev_timer* Events::add_io_timer(double secs, ev_timer* w, void* privdata) {
    if (m_io_timer_callback_fn == nullptr) {
        LOG_ERROR("pls set io timer callback fn!");
        return nullptr;
    }
    return add_timer_event(secs, w, m_io_timer_callback_fn, privdata);
}

ev_timer* Events::add_cmd_timer(double secs, ev_timer* w, void* privdata) {
    if (m_cmd_timer_callback_fn == nullptr) {
        LOG_ERROR("pls set cmd timer callback fn!");
        return nullptr;
    }
    return add_timer_event(secs, w, m_cmd_timer_callback_fn, privdata, secs);
}

ev_timer* Events::add_repeat_timer(double secs, ev_timer* w, void* privdata) {
    if (m_repeat_timer_callback_fn == nullptr) {
        LOG_ERROR("pls set repeat timer callback fn!");
        return nullptr;
    }
    return add_timer_event(secs, w, m_repeat_timer_callback_fn, privdata, secs);
}

ev_timer* Events::add_session_timer(double secs, ev_timer* w, void* privdata) {
    if (m_session_timer_callback_fn == nullptr) {
        LOG_ERROR("pls set session timer callback fn!");
        return nullptr;
    }
    return add_timer_event(secs, w, m_session_timer_callback_fn, privdata, secs);
}

ev_timer* Events::add_timer_event(double secs, ev_timer* w, cb_timer tcb, void* privdata, int repeat_secs) {
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

    LOG_TRACE("start timer, seconds: %f", secs);
    return w;
}

bool Events::restart_timer(double secs, ev_timer* w, void* privdata) {
    if (w == nullptr) {
        return false;
    }
    ev_timer_stop(m_ev_loop, w);
    ev_timer_set(w, secs + ev_time() - ev_now(m_ev_loop), 0);
    ev_timer_start(m_ev_loop, w);
    w->data = privdata;
    LOG_TRACE("restart timer, seconds: %f", secs);
    return true;
}

bool Events::del_io_event(ev_io* w) {
    if (w == nullptr) {
        return false;
    }

    LOG_TRACE("delete io event, fd: %d", w->fd);

    ev_io_stop(m_ev_loop, w);
    w->data = NULL;
    SAFE_FREE(w);
    return true;
}

bool Events::del_timer_event(ev_timer* w) {
    if (w == nullptr) {
        return false;
    }

    LOG_TRACE("delete timer event: %p", w);

    ev_timer_stop(m_ev_loop, w);
    w->data = nullptr;
    SAFE_FREE(w);
    return true;
}

bool Events::stop_io_event(ev_io* w) {
    if (w == nullptr) {
        return false;
    }
    ev_io_stop(m_ev_loop, w);
    return true;
}

}  // namespace kim