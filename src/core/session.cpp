#include "session.h"

namespace kim {

// Session
////////////////////////////////////////////////

Session::Session(uint64_t id, Log* logger, INet* net, const std::string& name)
    : Base(id, logger, net, name) {
    set_active_time(get_net()->now());
}

Session::~Session() {
}

void Session::init(const std::string& sessid, double timeout) {
    m_sessid = sessid;
    set_keep_alive(timeout);
}

// SessionMgr
////////////////////////////////////////////////

SessionMgr::~SessionMgr() {
    for (auto& it : m_sessions) {
        Session* s = it.second;
        if (s != nullptr) {
            get_events()->del_timer_event(s->get_timer());
            SAFE_DELETE(s);
        }
    }
    m_sessions.clear();
}

bool SessionMgr::add_session(Session* s) {
    if (s == nullptr) {
        return false;
    }
    const std::string& sessid = s->sessid();
    auto it = m_sessions.find(sessid);
    if (it != m_sessions.end()) {
        s->set_active_time(get_net()->now());
        return true;
    }
    m_sessions[sessid] = s;
    ev_timer* w = get_events()->add_session_timer(60.0, s->get_timer(), s);
    if (w == nullptr) {
        m_sessions.erase(sessid);
        LOG_ERROR("add session(%s) failed!", s->get_name());
        return false;
    }
    s->set_timer(w);
    return true;
}

Session* SessionMgr::get_session(const std::string& sessid, bool re_active) {
    auto it = m_sessions.find(sessid);
    if (it == m_sessions.end()) {
        return nullptr;
    }
    if (re_active) {
        it->second->set_active_time(get_events()->time_now());
    }
    return it->second;
}

bool SessionMgr::del_session(const std::string& sessid) {
    auto it = m_sessions.find(sessid);
    if (it == m_sessions.end()) {
        return false;
    }
    Session* s = it->second;
    if (s != nullptr) {
        get_events()->del_timer_event(s->get_timer());
        SAFE_DELETE(s);
    }
    m_sessions.erase(it);
    return true;
}

void SessionMgr::on_session_timer(void* privdata) {
    Session* s = static_cast<Session*>(privdata);
    double secs = s->get_keep_alive() - (get_net()->now() - s->get_active_time());
    if (secs > 0) {
        LOG_DEBUG("session timer restart, sessid: %llu, restart timer secs: %f",
                  s->sessid().c_str(), secs);
        get_events()->restart_timer(secs, s->get_timer(), privdata);
        return;
    }

    int old = s->get_cur_timeout_cnt();
    Session::STATUS status = s->on_timeout();
    if (status != Session::STATUS::RUNNING) {
        LOG_DEBUG("timeout del session, sessid: %s", s->sessid().c_str());
        del_session(s->sessid());
        return;
    }

    if (old == s->get_cur_timeout_cnt()) {
        s->refresh_cur_timeout_cnt();
    }
    if (s->get_cur_timeout_cnt() >= s->get_max_timeout_cnt()) {
        LOG_DEBUG("timeout del session, sessid: %s", s->sessid().c_str());
        del_session(s->sessid());
        return;
    }
    LOG_DEBUG("session timer reset, session id: %llu, restart timer secs: %f",
              s->get_id(), secs);
    get_events()->restart_timer(s->get_keep_alive(), s->get_timer(), privdata);
}

}  // namespace kim