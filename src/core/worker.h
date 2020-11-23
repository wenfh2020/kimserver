#ifndef __KIM_WORKER_H__
#define __KIM_WORKER_H__

#include "network.h"
#include "nodes.h"
#include "util/json/CJsonObject.hpp"
#include "worker_data_mgr.h"

namespace kim {

class Worker {
   public:
    Worker(const std::string& name);
    virtual ~Worker();

    bool init(const worker_info_t* info, const CJsonObject& conf);
    void run();

    /* libev callback. */
    static void on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents);
    static void on_repeat_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);
    void on_terminated(ev_signal* s);
    void on_repeat_timer(void* privdata);

   private:
    bool load_logger();
    bool load_network();
    bool load_timer();

   private:
    Log* m_logger = nullptr;     /* logger. */
    Network* m_net = nullptr;    /* network. */
    ev_timer* m_timer = nullptr; /* repeat timer for idle handle. */

    CJsonObject m_conf;           // current config.
    worker_info_t m_worker_info;  // current worker info.
};

}  // namespace kim

#endif  //__KIM_WORKER_H__
