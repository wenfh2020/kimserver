#ifndef __WORKER_H__
#define __WORKER_H__

#include "network.h"
#include "node_info.h"
#include "util/CJsonObject.hpp"
#include "worker_data_mgr.h"

namespace kim {

class Worker : public ISignalCallBack {
   public:
    Worker(const std::string& name);
    virtual ~Worker();

    bool init(const worker_info_t* info, const CJsonObject& conf);
    void run();

    void on_terminated(ev_signal* s) override;

   private:
    bool load_logger();
    bool load_network();

   private:
    std::shared_ptr<Log> m_logger = nullptr;  //logger.
    Network* m_net = nullptr;                 // net work.
    CJsonObject m_json_conf;                  // current config.
    worker_info_t m_worker_info;              // current worker info.
};

}  // namespace kim

#endif  //__WORKER_H__
