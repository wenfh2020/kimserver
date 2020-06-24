#include "events.h"

#include <ev.h>
#include <signal.h>
#include <unistd.h>

#include "context.h"

namespace kim {

Events::Events(Log* logger)
    : m_logger(logger), m_ev_loop(NULL), m_net(NULL), m_seq(0), m_sig_cb(NULL) {
}

Events::~Events() {
    destory();
}

bool Events::create(const addr_info_t* addr_info, ISignalCallback* s) {
    m_ev_loop = ev_loop_new(EVFLAG_FORKCHECK | EVFLAG_SIGNALFD);
    if (NULL == m_ev_loop) {
        LOG_ERROR("new libev loop failed!");
        return false;
    }

    setup_signal_events(s);

    if (!init_network(addr_info)) {
        LOG_ERROR("init network failed!");
        return false;
    }

    LOG_INFO("init events success!");
    return true;
}

void Events::destory() {
    close_listen_sockets();
    SAFE_DELETE(m_net);

    if (m_ev_loop != NULL) {
        ev_loop_destroy(m_ev_loop);
        m_ev_loop = NULL;
    }

    close_conns();
}

void Events::run() {
    if (m_ev_loop) ev_run(m_ev_loop, 0);
}

void Events::create_ev_signal(int signum) {
    if (m_ev_loop != NULL) {
        ev_signal* s = new ev_signal();
        ev_signal_init(s, signal_callback, signum);
        s->data = (void*)m_sig_cb;
        ev_signal_start(m_ev_loop, s);
    }
}

bool Events::setup_signal_events(ISignalCallback* s) {
    m_sig_cb = s;

    int signals[] = {SIGCHLD, SIGILL, SIGBUS, SIGFPE, SIGKILL};
    for (int i = 0; i < sizeof(signals) / sizeof(int); i++) {
        create_ev_signal(signals[i]);
    }
    return true;
}

void Events::signal_callback(struct ev_loop* loop, struct ev_signal* s, int revents) {
    if (NULL == s->data) return;

    ISignalCallback* sig_cb = static_cast<ISignalCallback*>(s->data);
    if (SIGCHLD == s->signum) {
        sig_cb->on_child_terminated(s);
    } else {
        sig_cb->on_terminated(s);
    }
}  // namespace kim

bool Events::add_read_event(Connection* c) {
    if (c == NULL) {
        LOG_ERROR("invalid connection!");
        return false;
    }

    std::map<int, Connection*>::iterator it = m_conns.find(c->get_fd());
    if (it == m_conns.end()) {
        LOG_ERROR("can not find connetion from fd: %s", c->get_fd());
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
        c->set_private_data(this);

        LOG_DEBUG("start ev io, fd: %d", c->get_fd());
    } else {
        if (ev_is_active(e)) {
            ev_io_stop(m_ev_loop, e);
            ev_io_set(e, e->fd, e->events | EV_READ);
            ev_io_start(m_ev_loop, e);
        } else {
            ev_io_init(e, event_callback, c->get_fd(), EV_READ);
            ev_io_start(m_ev_loop, e);
        }

        LOG_DEBUG("restart ev io, fd: %d", c->get_fd());
    }

    return true;
}

bool Events::add_chanel_event(int fd) {
    Connection* c = create_conn(fd);
    if (c != NULL) {
        c->set_state(kim::Connection::CONN_STATE_CONNECTED);
        add_read_event(c);
        return true;
    }

    return false;
}

void Events::close_listen_sockets() {
    std::list<int>::iterator it = m_listen_fds.begin();
    for (; it != m_listen_fds.end(); it++) {
        if (*it != -1) close(*it);
    }
}

bool Events::init_network(const addr_info_t* addr_info) {
    m_net = new Network(m_logger);
    if (m_net == NULL) {
        LOG_ERROR("new network failed!");
        return false;
    }

    if (!m_net->create(addr_info, m_listen_fds)) {
        LOG_ERROR("create network failed!");
        return false;
    }

    std::list<int>::iterator it = m_listen_fds.begin();
    for (; it != m_listen_fds.end(); it++) {
        add_chanel_event(*it);
    }

    LOG_INFO("init network done!");
    return true;
}

void Events::close_chanel(int* fds) {
    if (close(fds[0]) == -1) {
        LOG_WARNING("close channel failed, fd: %d.", fds[0]);
    }

    if (close(fds[1]) == -1) {
        LOG_WARNING("close channel failed, fd: %d.", fds[1]);
    }
}

void Events::event_callback(struct ev_loop* loop, struct ev_io* e, int events) {
    if (e == NULL) return;

    Connection* c = static_cast<Connection*>(e->data);
    if (c == NULL) return;

    Events* event = static_cast<Events*>(c->get_private_data());
    if (event == NULL) return;

    if (events & EV_READ) {
        event->io_read(c, e);
    }

    if (events & EV_WRITE &&
        c->get_state() != Connection::CONN_STATE_CLOSED) {
        event->io_write(c, e);
    }

    if (events & EV_ERROR) {
        event->io_error(c, e);
    }
}

Connection* Events::create_conn(int fd) {
    std::map<int, Connection*>::iterator it = m_conns.find(fd);
    if (it != m_conns.end()) {
        return it->second;
    }

    uint64_t seq = get_new_seq();
    Connection* c = new Connection(fd, seq);
    if (c == NULL) {
        LOG_ERROR("new connection failed, fd: %d", fd);
        return NULL;
    }

    LOG_DEBUG("create connection fd: %d, seq: %llu", fd, seq);
    m_conns[fd] = c;
    return c;
}

void Events::close_conns() {
    std::map<int, Connection*>::iterator it = m_conns.begin();
    for (; it != m_conns.begin(); it++) {
        SAFE_FREE(it->second->get_ev_io());
        SAFE_DELETE(it->second);
    }
}

bool Events::io_read(Connection* c, struct ev_io* e) {
    if (c == NULL || e == NULL) return false;

    if (e->fd == m_net->get_bind_fd()) {
    } else if (e->fd == m_net->get_gate_bind_fd()) {
    } else {
        return true;
    }

    LOG_DEBUG("io read fd: %d, seq: %d", c->get_fd(), c->get_id());
    return true;
}

bool Events::io_write(Connection* c, struct ev_io* e) {
    LOG_DEBUG("io write fd: %d, seq: %d", c->get_fd(), c->get_id());
    return true;
}

bool Events::io_error(Connection* c, struct ev_io* e) {
    LOG_DEBUG("io error fd: %d, seq: %d", c->get_fd(), c->get_id());
    return true;
}

}  // namespace kim