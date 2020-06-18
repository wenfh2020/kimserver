#include "events.h"

namespace kim {

bool Events::init() {
    m_ev_loop = ev_loop_new(EVFLAG_FORKCHECK | EVFLAG_SIGNALFD);
    if (NULL == m_ev_loop) {
        return false;
    }

    return true;
}

void Events::run() {
    ev_run(m_ev_loop, 0);
}

}  // namespace kim