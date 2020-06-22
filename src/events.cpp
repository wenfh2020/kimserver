#include "events.h"

#include <ev.h>
#include <signal.h>
#include <unistd.h>

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
        return false;
    }

    if (!init_network(addr_info)) {
        LOG_ERROR("init network failed!");
        return false;
    }

    create_events();
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
    ev_signal* s = new ev_signal();
    ev_signal_init(s, signal_callback, signum);
    s->data = (void*)m_sig_cb_info;
    ev_signal_start(m_ev_loop, s);
}

bool Events::create_events() {
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

bool Events::add_io_event(int fd) {
    return true;
}

void Events::close_listen_sockets() {
    std::list<int>::iterator itr = m_fds.begin();
    for (; itr != m_fds.end(); itr++) {
        if (*itr != -1) close(*itr);
    }
}

bool Events::init_network(const addr_info_t* addr_info) {
    m_network = new Network(m_logger);
    if (NULL == m_network) return false;

    if (!m_network->create(addr_info, m_fds)) {
        LOG_ERROR("create network failed!");
        return false;
    }

    LOG_INFO("init network done!");
    return true;
}

void Events::close_chanel(int* fds) {
    if (close(fds[0]) == -1) {
        LOG_WARNING("close() channel failed!");
    }

    if (close(fds[1]) == -1) {
        LOG_WARNING("close() channel failed!");
    }
}

}  // namespace kim