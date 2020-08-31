#ifndef __KIM_SESSION_TEST_H__
#define __KIM_SESSION_TEST_H__

#include "session.h"

namespace kim {

class SessionTest : public Session {
   public:
    SessionTest(uint64_t id, Log* logger, INet* net, const std::string& name = "")
        : Session(id, logger, net, name) {}
    ~SessionTest() {}

    void set_value(const std::string& v) { m_value = v; }
    const std::string& value() { return m_value; }

   private:
    std::string m_value;
};

}  // namespace kim

#endif  //__KIM_SESSION_TEST_H__