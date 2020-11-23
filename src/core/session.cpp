#include "session.h"

namespace kim {

// Session
////////////////////////////////////////////////

Session::Session(uint64_t id, Log* logger, INet* n, const std::string& name)
    : Base(id, logger, n, name) {
    set_active_time(net()->now());
    set_max_timeout_cnt(SESSION_MAX_TIMEOUT_CNT);
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
            events()->del_timer_event(s->timer());
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
        s->set_active_time(net()->now());
        return true;
    }
    m_sessions[sessid] = s;
    ev_timer* w = events()->add_session_timer(s->keep_alive(), s->timer(), s);
    if (w == nullptr) {
        m_sessions.erase(sessid);
        LOG_ERROR("add session(%s) failed!", s->name());
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
        it->second->set_active_time(events()->now());
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
        events()->del_timer_event(s->timer());
        SAFE_DELETE(s);
    }

    m_sessions.erase(it);
    return true;
}

void SessionMgr::on_session_timer(void* privdata) {
    Session* s = static_cast<Session*>(privdata);
    double secs = s->keep_alive() - (net()->now() - s->active_time());
    if (secs > 0) {
        LOG_TRACE("session timer restart, sessid: %llu, restart timer secs: %f",
                  s->sessid(), secs);
        events()->restart_timer(secs, s->timer(), privdata);
        return;
    }

    int old = s->cur_timeout_cnt();
    Session::STATUS status = s->on_timeout();
    if (status != Session::STATUS::RUNNING) {
        LOG_TRACE("timeout del session, sessid: %s", s->sessid());
        del_session(s->sessid());
        return;
    }

    if (old == s->cur_timeout_cnt()) {
        s->refresh_cur_timeout_cnt();
    }

    if (s->cur_timeout_cnt() >= s->max_timeout_cnt()) {
        LOG_DEBUG("timeout del session, sessid: %s", s->sessid());
        del_session(s->sessid());
        return;
    }

    LOG_TRACE("session timer reset, session id: %llu, restart timer secs: %f",
              s->id(), secs);
    events()->restart_timer(s->keep_alive(), s->timer(), privdata);
}

}  // namespace kim