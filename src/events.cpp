#include "events.h"

#include <ev.h>
#include <signal.h>
#include <unistd.h>

#include "context.h"

namespace kim {

Events::Events(Log* logger)
    : m_logger(logger), m_ev_loop(NULL), m_sig_cb_info(NULL), m_network(NULL), m_seq(0) {
}

Events::~Events() {
    destory();
}

bool Events::create(const addr_info_t* addr_info) {
    m_sig_cb_info = new signal_callback_info_t;
    m_ev_loop = ev_loop_new(EVFLAG_FORKCHECK | EVFLAG_SIGNALFD);
    if (NULL == m_ev_loop) {
        LOG_ERROR("new libev loop failed!");
        return false;
    }

    setup_signal_events();

    if (!init_network(addr_info)) {
        LOG_ERROR("init network failed!");
        return false;
    }

    LOG_INFO("init events success!");
    return true;
}

void Events::destory() {
    SAFE_DELETE(m_sig_cb_info);
    close_listen_sockets();
    SAFE_DELETE(m_network);

    if (m_ev_loop != NULL) {
        ev_loop_destroy(m_ev_loop);
        m_ev_loop = NULL;
    }

    close_conns();
}

void Events::run() {
    if (m_ev_loop) ev_run(m_ev_loop, 0);
}

void Events::set_cb_terminated(ev_cb_fn* cb) {
    m_sig_cb_info->fn_terminated = cb;
}

void Events::set_cb_child_terminated(ev_cb_fn* cb) {
    m_sig_cb_info->fn_child_terminated = cb;
}

void Events::create_ev_signal(int signum) {
    if (m_ev_loop != NULL) {
        ev_signal* s = new ev_signal();
        ev_signal_init(s, signal_callback, signum);
        s->data = (void*)m_sig_cb_info;
        ev_signal_start(m_ev_loop, s);
    }
}

bool Events::setup_signal_events() {
    int signals[] = {SIGCHLD, SIGILL, SIGBUS, SIGFPE, SIGKILL};
    for (int i = 0; i < sizeof(signals) / sizeof(int); i++) {
        create_ev_signal(signals[i]);
    }
    return true;
}

void Events::signal_callback(struct ev_loop* loop, struct ev_signal* s, int revents) {
    if (NULL == s->data) return;

    signal_callback_info_t* cb_info = (signal_callback_info_t*)s->data;
    if (SIGCHLD == s->signum) {
        if (cb_info->fn_child_terminated) {
            cb_info->fn_child_terminated(s);
        }
    } else {
        if (cb_info->fn_terminated) {
            cb_info->fn_terminated(s);
        }
    }
}

bool Events::add_read_event(Connection* c) {
    if (c == NULL) {
        LOG_ERROR("invalid connection!");
        return false;
    }

    std::map<int, Connection*>::iterator itr = m_conns.find(c->get_fd());
    if (itr == m_conns.end()) {
        LOG_ERROR("can not find connetion from fd: %s", c->get_fd());
        return false;
    }

    ev_io* e = (ev_io*)c->get_private_data();
    if (e == NULL) {
        e = new ev_io();
        if (e == NULL) {
            LOG_ERROR("new ev_io failed!");
            return false;
        }
        c->set_private_data(e);

        e->data = c;
        ev_io_init(e, cb_io_events, c->get_fd(), EV_READ);
        ev_io_start(m_ev_loop, e);

        LOG_DEBUG("start ev io, fd: %d", c->get_fd());
    } else {
        if (ev_is_active(e)) {
            ev_io_stop(m_ev_loop, e);
            ev_io_set(e, e->fd, e->events | EV_READ);
            ev_io_start(m_ev_loop, e);
        } else {
            ev_io_init(e, cb_io_events, c->get_fd(), EV_READ);
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
    std::list<int>::iterator itr = m_listen_fds.begin();
    for (; itr != m_listen_fds.end(); itr++) {
        if (*itr != -1) close(*itr);
    }
}

bool Events::init_network(const addr_info_t* addr_info) {
    m_network = new Network(m_logger);
    if (m_network == NULL) {
        LOG_ERROR("new network failed!");
        return false;
    }

    if (!m_network->create(addr_info, m_listen_fds)) {
        LOG_ERROR("create network failed!");
        return false;
    }

    std::list<int>::iterator itr = m_listen_fds.begin();
    for (; itr != m_listen_fds.end(); itr++) {
        add_chanel_event(*itr);
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

void Events::cb_io_events(struct ev_loop* loop, struct ev_io* ev, int events) {
    if (ev == NULL) return;

    printf("cb_io_events %d\n", events);

    if (events & EV_READ) {
    }

    if (events & EV_WRITE) {
    }

    if (events & EV_ERROR) {
    }
}

Connection* Events::create_conn(int fd) {
    std::map<int, Connection*>::iterator itr = m_conns.find(fd);
    if (itr != m_conns.end()) {
        return itr->second;
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
    std::map<int, Connection*>::iterator itr = m_conns.begin();
    for (; itr != m_conns.begin(); itr++) {
        SAFE_DELETE(itr->second);
    }
}

}  // namespace kim