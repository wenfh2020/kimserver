#ifndef __KIM_BASE_H__
#define __KIM_BASE_H__

#include "events.h"
#include "net.h"
#include "util/log.h"

namespace kim {

class Base {
   public:
    Base() {}
    Base(uint64_t id, Log* logger, INet* net, const std::string& name = "")
        : m_id(id), m_logger(logger), m_net(net), m_name(name) {
    }
    Base(const Base&) = delete;
    Base& operator=(const Base&) = delete;
    virtual ~Base() {}

   public:
    void set_id(uint64_t id) { m_id = id; }
    uint64_t id() { return m_id; }

    void set_logger(Log* logger) { m_logger = logger; }

    INet* net() { return m_net; }
    Events* events() { return m_net->events(); }
    void set_net(INet* net) { m_net = net; }

    void set_name(const std::string& name) { m_name = name; }
    const std::string& name() const { return m_name; }
    const char* name() { return m_name.c_str(); }

    // session
    template <typename T>
    T* get_alloc_session(const std::string& sessid, double secs = 60.0, bool re_active = false) {
        T* s = dynamic_cast<T*>(net()->get_session(sessid, re_active));
        if (s == nullptr) {
            s = new T(m_net->new_seq(), m_logger, m_net);
            if (s == nullptr) {
                LOG_ERROR("alloc session failed! sessionid: %s", sessid.c_str());
                return nullptr;
            }
            s->init(sessid, secs);
            if (!m_net->add_session(s)) {
                SAFE_DELETE(s);
                LOG_ERROR("add session failed! sessid: %s", sessid.c_str());
                return nullptr;
            }
        }
        return s;
    }

   protected:
    uint64_t m_id = 0;
    Log* m_logger = nullptr;
    INet* m_net = nullptr;
    std::string m_name;
};

}  // namespace kim

#endif  //__KIM_BASE_H__
