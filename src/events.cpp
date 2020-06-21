#include "events.h"

#include <ev.h>
#include <signal.h>

namespace kim {

Events::Events(Log* logger)
    : m_logger(logger), m_ev_loop(NULL), m_sig_cb_info(NULL) {
}

Events::~Events() {
}

bool Events::create() {
    m_sig_cb_info = new signal_callback_info_t;
    m_ev_loop = ev_loop_new(EVFLAG_FORKCHECK | EVFLAG_SIGNALFD);
    if (NULL == m_ev_loop) {
        return false;
    }

    create_events();
    LOG_INFO("init events success!");
    return true;
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

}  // namespace kim