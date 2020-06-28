#ifndef __WORKER_H__
#define __WORKER_H__

#include "log.h"
#include "network.h"
#include "node_info.h"
#include "util/CJsonObject.hpp"

namespace kim {

class Worker : public ISignalCallBack {
   public:
    Worker(Log* logger);
    virtual ~Worker() {}

    bool init(const worker_info_t* info, const std::string& server_name);
    void run();

    virtual void on_terminated(struct ev_signal* s);

   private:
    bool create_network();

   private:
    Log* m_logger;
    worker_info_t m_worker_info;
    Network* m_net;
};

}  // namespace kim
#endif
