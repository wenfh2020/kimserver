#ifndef __KIM_TIMER_H__
#define __KIM_TIMER_H__

#include <ev.h>

#include "util/util.h"

namespace kim {

class Timer {
   public:
    Timer() {}
    virtual ~Timer() {}

    void set_timer(ev_timer* w) { m_timer = w; }
    ev_timer* timer() { return m_timer; }

    void set_active_time(double t) { m_active_time = t; }
    double active_time() const { return m_active_time; }

    virtual bool is_need_alive_check() { return true; }
    virtual void set_keep_alive(double secs) { m_keep_alive = secs; }
    virtual double keep_alive() { return m_keep_alive; }

    int set_max_timeout_cnt(int cnt) { return m_max_timeout_cnt = cnt; }
    int max_timeout_cnt() { return m_max_timeout_cnt; }
    int cur_timeout_cnt() { return m_cur_timeout_cnt; }
    void refresh_cur_timeout_cnt() { ++m_cur_timeout_cnt; }

   protected:
    ev_timer* m_timer = nullptr;
    double m_active_time = 0;  // connection last active (read/write) time.
    double m_keep_alive = 0;
    int m_cur_timeout_cnt = 0;
    int m_max_timeout_cnt = 0;
};

}  // namespace kim

#endif  //__KIM_TIMER_H__