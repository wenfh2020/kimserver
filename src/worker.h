#ifndef __WORKER_H__
#define __WORKER_H__

#include "node_info.h"
#include "util/CJsonObject.hpp"

namespace kim {

class Worker {
   public:
    Worker(const std::string& work_path, int ctrl_fd, int data_fd, int worker_idx);
    virtual ~Worker() {}

    bool init(const util::CJsonObject& json_conf);
    bool run();

   private:
    worker_info_t m_worker_info;
};

}  // namespace kim
#endif
