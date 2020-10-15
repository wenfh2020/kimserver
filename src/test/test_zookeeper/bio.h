/* new a thread to handle the zk commands. */
#ifndef __KIM_BIO_H__
#define __KIM_BIO_H__

#include <pthread.h>

#include "../../../src/core/server.h"
#include "../../../src/core/util/log.h"
#include "task.h"

namespace kim {

class Bio {
   public:
    Bio(Log* logger);
    virtual ~Bio();

    bool bio_init();
    void bio_stop() { m_stop_thread = true; }
    static void* bio_process_jobs(void* arg);

    virtual void process_task(zk_task_t* task) {}
    bool add_task(const std::string& path, zk_task_t::OPERATE oper,
                  void* privdata, const std::string& value = "", int flag = 0);

   protected:
    Log* m_logger = nullptr;
    pthread_t m_thread = nullptr;
    bool m_stop_thread = false;
};

}  // namespace kim

#endif  // __KIM_BIO_H__