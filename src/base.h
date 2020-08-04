#ifndef __KIM_BASE_H__
#define __KIM_BASE_H__

#include "net.h"
#include "util/log.h"

namespace kim {

class Base {
   public:
    Base() {}
    Base(uint64_t id, Log* logger, INet* net, _cstr& name = "")
        : m_id(id), m_logger(logger), m_net(net), m_name(name) {}
    virtual ~Base() {}

   public:
    void set_id(uint64_t id) { m_id = id; }
    uint64_t get_id() { return m_id; }

    void set_logger(Log* logger) { m_logger = logger; }

    INet* get_net() { return m_net; }
    void set_net(INet* net) { m_net = net; }

    void set_name(_cstr& name) { m_name = name; }
    _cstr& get_name() const { return m_name; }
    const char* get_name() { return m_name.c_str(); }

   protected:
    uint64_t m_id = 0;
    Log* m_logger = nullptr;
    INet* m_net = nullptr;
    std::string m_name;
};

}  // namespace kim

#endif  //__KIM_BASE_H__