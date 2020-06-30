#include "events.h"

#include <ev.h>
#include <signal.h>
#include <unistd.h>

#include "context.h"

namespace kim {

Events::Events(Log* logger) : m_logger(logger),
                              m_ev_loop(nullptr),
                              m_ev_cb(nullptr) {
}

Events::~Events() {
    destory();
}

bool Events::create(IEventsCallback* e) {
    m_ev_loop = ev_loop_new(EVFLAG_FORKCHECK | EVFLAG_SIGNALFD);
    if (m_ev_loop == nullptr) {
        LOG_ERROR("new libev loop failed!");
        return false;
    }

    m_ev_cb = e;
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

bool Events::add_read_event(Connection* c) {
    if (c == nullptr) {
        LOG_ERROR("invalid connection!");
        return false;
    }

    ev_io* e = c->get_ev_io();
    if (e == nullptr) {
        e = (ev_io*)malloc(sizeof(ev_io));
        if (e == nullptr) {
            LOG_ERROR("new ev_io failed!");
            return false;
        }

        c->set_ev_io(e);
        c->set_private_data(m_ev_cb);
        e->data = c;
        ev_io_init(e, on_event_callback, c->get_fd(), EV_READ);
        ev_io_start(m_ev_loop, e);

        LOG_INFO("start ev io, fd: %d", c->get_fd());
    } else {
        if (ev_is_active(e)) {
            ev_io_stop(m_ev_loop, e);
            ev_io_set(e, e->fd, e->events | EV_READ);
            ev_io_start(m_ev_loop, e);
        } else {
            ev_io_init(e, on_event_callback, c->get_fd(), EV_READ);
            ev_io_start(m_ev_loop, e);
        }

        LOG_INFO("restart ev io, fd: %d", c->get_fd());
    }

    return true;
}

bool Events::del_event(Connection* c) {
    if (c == nullptr || c->get_ev_io() == nullptr) {
        return false;
    }

    ev_io* e = c->get_ev_io();
    ev_io_stop(m_ev_loop, e);
    e->data = NULL;
    SAFE_FREE(e);

    LOG_DEBUG("delete event, fd: %d", c->get_fd());
    return true;
}

void Events::on_event_callback(struct ev_loop* loop, ev_io* e, int events) {
    if (e == nullptr || e->data == nullptr) {
        return;
    }

    Connection* c = static_cast<Connection*>(e->data);
    IEventsCallback* cb = static_cast<IEventsCallback*>(c->get_private_data());
    if (cb == nullptr) {
        return;
    }

    if (events & EV_READ) {
        cb->on_io_read(c, e);
    }

    if ((events & EV_WRITE) &&
        (c->get_state() != Connection::CONN_STATE::CLOSED)) {
        cb->on_io_write(c, e);
    }

    if (events & EV_ERROR) {
        cb->on_io_error(c, e);
    }
}

}  // namespace kim