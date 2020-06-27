#include "events.h"

#include <ev.h>
#include <signal.h>
#include <unistd.h>

#include "context.h"

namespace kim {

Events::Events(Log* logger) : m_logger(logger),
                              m_ev_loop(NULL),
                              m_io_cb(NULL),
                              m_sig_cb(NULL) {
}

Events::~Events() {
    destory();
}

bool Events::create(ISignalCallBack* sig, IEventsCallback* io) {
    LOG_DEBUG("create()");

    m_ev_loop = ev_loop_new(EVFLAG_FORKCHECK | EVFLAG_SIGNALFD);
    if (NULL == m_ev_loop) {
        LOG_ERROR("new libev loop failed!");
        return false;
    }

    m_io_cb = io;
    setup_signal_events(sig);

    LOG_INFO("init events success!");
    return true;
}

void Events::destory() {
    LOG_DEBUG("destory()");

    if (m_ev_loop != NULL) {
        ev_loop_destroy(m_ev_loop);
        m_ev_loop = NULL;
    }
}

void Events::run() {
    LOG_DEBUG("run()");

    if (m_ev_loop) ev_run(m_ev_loop, 0);
}

void Events::create_signal_events(int signum) {
    LOG_DEBUG("create_signal_events()");

    if (m_ev_loop != NULL) {
        ev_signal* s = new ev_signal();
        ev_signal_init(s, signal_callback, signum);
        s->data = (void*)m_sig_cb;
        ev_signal_start(m_ev_loop, s);
    }
}

bool Events::setup_signal_events(ISignalCallBack* s) {
    LOG_DEBUG("setup_signal_events()");

    m_sig_cb = s;

    int signals[] = {SIGCHLD, SIGILL, SIGBUS, SIGFPE, SIGKILL};
    for (int i = 0; i < sizeof(signals) / sizeof(int); i++) {
        create_signal_events(signals[i]);
    }
    return true;
}

void Events::signal_callback(struct ev_loop* loop, struct ev_signal* s, int revents) {
    if (s->data == NULL) return;

    ISignalCallBack* sig_cb = static_cast<ISignalCallBack*>(s->data);
    if (SIGCHLD == s->signum) {
        sig_cb->on_child_terminated(s);
    } else {
        sig_cb->on_terminated(s);
    }
}

bool Events::add_read_event(Connection* c) {
    LOG_DEBUG("add_read_event()");

    if (c == NULL) {
        LOG_ERROR("invalid connection!");
        return false;
    }

    ev_io* e = c->get_ev_io();
    if (e == NULL) {
        e = (ev_io*)malloc(sizeof(ev_io));
        if (e == NULL) {
            LOG_ERROR("new ev_io failed!");
            return false;
        }

        e->data = c;
        ev_io_init(e, event_callback, c->get_fd(), EV_READ);
        ev_io_start(m_ev_loop, e);

        c->set_ev_io(e);
        c->set_private_data(m_io_cb);

        LOG_INFO("start ev io, fd: %d", c->get_fd());
    } else {
        if (ev_is_active(e)) {
            ev_io_stop(m_ev_loop, e);
            ev_io_set(e, e->fd, e->events | EV_READ);
            ev_io_start(m_ev_loop, e);
        } else {
            ev_io_init(e, event_callback, c->get_fd(), EV_READ);
            ev_io_start(m_ev_loop, e);
        }

        LOG_INFO("restart ev io, fd: %d", c->get_fd());
    }

    return true;
}

bool Events::del_event(Connection* c) {
    LOG_DEBUG("del_event()");

    if (c == NULL) return false;

    ev_io* e = c->get_ev_io();
    if (e == NULL) return false;
    ev_io_stop(m_ev_loop, e);
    e->data = NULL;
    SAFE_FREE(e);

    return true;
}

void Events::event_callback(struct ev_loop* loop, struct ev_io* e, int events) {
    if (e == NULL) return;

    Connection* c = static_cast<Connection*>(e->data);
    if (c == NULL) return;

    IEventsCallback* cb = static_cast<IEventsCallback*>(c->get_private_data());
    if (cb == NULL) return;

    if (events & EV_READ) {
        cb->io_read(c, e);
    }

    if ((events & EV_WRITE) &&
        (c->get_state() != Connection::CONN_STATE_CLOSED)) {
        cb->io_write(c, e);
    }

    if (events & EV_ERROR) {
        cb->io_error(c, e);
    }
}

}  // namespace kim