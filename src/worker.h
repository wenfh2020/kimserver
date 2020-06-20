#ifndef __WORKER_H__
#define __WORKER_H__

#include "log.h"
#include "node_info.h"
#include "util/CJsonObject.hpp"

namespace kim {

class Worker {
   public:
    Worker(const worker_info_s* worker_info);
    virtual ~Worker() {}

    bool init(kim::Log* log, const std::string& server_name);
    bool run();

   private:
    kim::Log* m_logger;
    worker_info_t m_worker_info;
};

}  // namespace kim
#endif
