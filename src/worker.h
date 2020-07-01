#ifndef __WORKER_H__
#define __WORKER_H__

#include "network.h"
#include "node_info.h"
#include "util/CJsonObject.hpp"
#include "worker_data_mgr.h"

namespace kim {

class Worker : public ISignalCallBack {
   public:
    Worker(Log* logger, const std::string& name);
    virtual ~Worker() {}

    bool init(const worker_info_t* info, const CJsonObject& conf);
    void run();

    void on_terminated(ev_signal* s) override;

   private:
    bool init_logger();
    bool create_network();

   private:
    Log* m_logger;
    CJsonObject m_json_conf;  // current config.
    worker_info_t m_worker_info;
    Network* m_net;
};

}  // namespace kim

#endif  //__WORKER_H__
