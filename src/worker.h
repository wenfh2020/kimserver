#ifndef __WORKER_H__
#define __WORKER_H__

#include "network.h"
#include "node_info.h"
#include "util/json/CJsonObject.hpp"
#include "worker_data_mgr.h"

namespace kim {

class Worker : public ICallback {
   public:
    Worker(_cstr& name);
    virtual ~Worker();

    bool init(const WorkerInfo* info, const CJsonObject& conf);
    void run();

    void on_terminated(ev_signal* s) override;

   private:
    bool load_logger();
    bool load_network();

   private:
    Log* m_logger = nullptr;   // logger.
    Network* m_net = nullptr;  // net work.
    CJsonObject m_conf;        // current config.
    WorkerInfo m_worker_info;  // current worker info.
};

}  // namespace kim

#endif  //__WORKER_H__
