#ifndef __WORKER_H__
#define __WORKER_H__

#include "log.h"
#include "network.h"
#include "node_info.h"
#include "util/CJsonObject.hpp"
#include "worker_data_mgr.h"

namespace kim {

class Worker : public ISignalCallBack {
   public:
    Worker(Log* logger, const std::string& name);
    virtual ~Worker() {}

    bool init(const worker_info_t* info);
    void run();

    void on_terminated(ev_signal* s) override;

   private:
    bool create_network();

   private:
    Log* m_logger;
    worker_info_t m_worker_info;
    Network* m_net;
};

}  // namespace kim
#endif
