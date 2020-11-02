/* 
 * create a new thread to handle the zk sync commands in the background, 
 * and callback for async.
 */
#ifndef __KIM_BIO_H__
#define __KIM_BIO_H__

#include <pthread.h>

#include <list>

#include "server.h"
#include "task.h"
#include "util/log.h"

namespace kim {

class Bio {
   public:
    Bio(Log* logger);
    virtual ~Bio();

    bool bio_init();
    void bio_stop() { m_stop_thread = true; }

    bool add_cmd_task(const std::string& path, zk_task_t::CMD cmd, const std::string& value = "");
    void add_ack_task(zk_task_t* task);

    /* bio thread. */
    static void* bio_process_tasks(void* arg);
    /* call by bio. */
    virtual void process_cmd(zk_task_t* task) {}
    /* call by timer. */
    virtual void process_ack(zk_task_t* task) {}

    /* timer. */
    void on_repeat_timer();
    void handle_acks();

   protected:
    Log* m_logger = nullptr;
    pthread_t m_thread;
    volatile bool m_stop_thread = false;

    pthread_cond_t m_cond;
    pthread_mutex_t m_mutex;
    std::list<zk_task_t*> m_req_tasks;
    std::list<zk_task_t*> m_ack_tasks;
};

}  // namespace kim

#endif  // __KIM_BIO_H__