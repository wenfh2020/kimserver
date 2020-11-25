#include "events_callback.h"

#include "cmd.h"
#include "connection.h"
#include "events.h"
#include "session.h"

namespace kim {

EventsCallback::EventsCallback() {
}

EventsCallback::EventsCallback(Log* logger, INet* net)
    : m_logger(logger), m_network(net) {
}

EventsCallback::~EventsCallback() {
    if (m_network != nullptr && m_timer != nullptr) {
        m_network->events()->del_timer_event(m_timer);
        m_timer = nullptr;
    }
}

bool EventsCallback::init(Log* logger, INet* net) {
    if (logger != nullptr && net != nullptr) {
        m_network = net;
        m_logger = logger;
        return true;
    }
    return false;
}

bool EventsCallback::setup_signal_events() {
    if (m_network != nullptr) {
        m_network->events()->set_sig_callback_fn(&on_signal_callback);
        return m_network->events()->setup_signal_events(this);
    }
    return false;
}

bool EventsCallback::setup_signal_event(int sig) {
    if (m_network != nullptr) {
        m_network->events()->set_sig_callback_fn(&on_signal_callback);
        return m_network->events()->create_signal_event(sig, this);
    }
    return false;
}

bool EventsCallback::setup_timer() {
    if (m_network != nullptr) {
        m_network->events()->set_repeat_timer_callback_fn(&on_repeat_timer_callback);
        m_timer = m_network->events()->add_repeat_timer(REPEAT_TIMEOUT_VAL, m_timer, this);
        return (m_timer != nullptr);
    }
    return false;
}

bool EventsCallback::setup_io_callback() {
    if (m_network != nullptr) {
        m_network->events()->set_io_callback_fn(&on_io_callback);
        return true;
    }
    return false;
}

bool EventsCallback::setup_io_timer_callback() {
    if (m_network != nullptr) {
        m_network->events()->set_io_timer_callback_fn(&on_io_timer_callback);
        return true;
    }
    return false;
}

bool EventsCallback::setup_cmd_timer_callback() {
    if (m_network != nullptr) {
        m_network->events()->set_cmd_timer_callback_fn(&on_cmd_timer_callback);
        return true;
    }
    return false;
}

bool EventsCallback::setup_session_timer_callback() {
    if (m_network != nullptr) {
        m_network->events()->set_session_timer_callback_fn(&on_session_timer_callback);
        return true;
    }
    return false;
}

void EventsCallback::on_repeat_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    EventsCallback* e = static_cast<EventsCallback*>(w->data);
    e->on_repeat_timer(w->data);
}

void EventsCallback::on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents) {
    EventsCallback* e = static_cast<EventsCallback*>(s->data);
    (s->signum == SIGCHLD) ? e->on_child_terminated(s) : e->on_terminated(s);
}

void EventsCallback::on_io_callback(struct ev_loop* loop, ev_io* w, int events) {
    EventsCallback* e = (EventsCallback*)w->data;
    if (events & EV_READ) e->on_io_read(w->fd);
    if (events & EV_WRITE) e->on_io_write(w->fd);
}

void EventsCallback::on_io_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    Connection* c = static_cast<Connection*>(w->data);
    EventsCallback* e = static_cast<EventsCallback*>(c->privdata());
    e->on_io_timer(w->data);
}

void EventsCallback::on_cmd_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    Cmd* cmd = static_cast<Cmd*>(w->data);
    EventsCallback* e = (EventsCallback*)cmd->net();
    e->on_cmd_timer((void*)cmd);
}

void EventsCallback::on_session_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    Session* s = static_cast<Session*>(w->data);
    EventsCallback* e = (EventsCallback*)s->net();
    e->on_session_timer(s);
}

}  // namespace kim