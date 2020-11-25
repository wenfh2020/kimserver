#ifndef __KIM_WORKER_H__
#define __KIM_WORKER_H__

#include "events_callback.h"
#include "network.h"
#include "nodes.h"
#include "util/json/CJsonObject.hpp"
#include "worker_data_mgr.h"

namespace kim {

class Worker : public EventsCallback {
   public:
    Worker(const std::string& name);
    virtual ~Worker();

    bool init(const worker_info_t* info, const CJsonObject& conf);
    void run();

    /* libev callback. */
    void on_terminated(ev_signal* s);
    void on_repeat_timer(void* privdata);

   private:
    bool load_logger();
    bool load_network();

   private:
    Network* m_net = nullptr;     /* network. */
    CJsonObject m_conf;           // current config.
    worker_info_t m_worker_info;  // current worker info.
};

}  // namespace kim

#endif  //__KIM_WORKER_H__
