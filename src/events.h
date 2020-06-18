#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <ev.h>

#include "log.h"

namespace kim {

class Events {
   public:
    Events() : m_logger(NULL), m_ev_loop(NULL) {}
    virtual ~Events() {}

    bool init();
    void run();
    void set_logger(kim::Log* l) { m_logger = l; }

   private:
    kim::Log* m_logger;
    struct ev_loop* m_ev_loop;
};

}  // namespace kim

#endif
