#ifndef __KIM_SESSION_H__
#define __KIM_SESSION_H__

#include <unordered_map>

#include "base.h"
#include "timer.h"

namespace kim {

class Session : public Timer, public Base {
   public:
    enum class STATUS {
        UNKOWN = 0,
        OK = 1,
        RUNNING = 2,
        ERROR = 3,
    };
    Session(uint64_t id, Log* logger, INet* net, const std::string& name = "");
    virtual ~Session();
    void init(const std::string& sessid, double timeout = 60.0);
    const std::string& sessid() const { return m_sessid; }

   public:
    Session::STATUS on_timeout() { return STATUS::OK; }

   protected:
    std::string m_sessid;
};

class SessionMgr : public Base {
   public:
    SessionMgr(Log* logger, INet* net) : Base(0, logger, net) {}
    virtual ~SessionMgr();

    bool add_session(Session* s);
    Session* get_session(const std::string& sessid, bool re_active = false);
    bool del_session(const std::string& sessid);

    // callback.
    void on_session_timer(void* privdata);

   private:
    std::unordered_map<std::string, Session*> m_sessions;
};

}  // namespace kim

#endif  //__KIM_SESSION_H__