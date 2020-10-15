#include "bio.h"

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <list>

#include "../../../src/core/util/util.h"
#include "task.h"

namespace kim {

/* Make sure we have enough stack to perform all the things we do 
 * in the main thread. */
#define REDIS_THREAD_STACK_SIZE (1024 * 1024 * 4)
pthread_mutex_t g_mutex;
std::list<zk_task_t*> g_tasks;

Bio::Bio(Log* logger) : m_logger(logger) {
}

Bio::~Bio() {
    for (auto& it : g_tasks) {
        delete it;
    }
    g_tasks.clear();
}

bool Bio::add_task(const std::string& path, zk_task_t::OPERATE oper, void* privdata,
                   const std::string& value, int flag) {
    zk_task_t* task = new zk_task_t;
    if (task == nullptr) {
        LOG_ERROR("new task failed! path: %s", path.c_str());
        return false;
    }
    task->path = path;
    task->oper = oper;
    task->value = value;
    task->flag = flag;
    task->privdata = privdata;
    task->create_time = time_now();

    pthread_mutex_lock(&g_mutex);
    g_tasks.push_back(task);
    pthread_mutex_unlock(&g_mutex);
    return true;
}

bool Bio::bio_init() {
    pthread_attr_t attr;
    pthread_t thread;
    size_t stacksize;

    pthread_mutex_init(&g_mutex, NULL);
    /* Set the stack size as by default it may be small in some system */
    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr, &stacksize);
    if (!stacksize) stacksize = 1; /* The world is full of Solaris Fixes */
    while (stacksize < REDIS_THREAD_STACK_SIZE) stacksize *= 2;
    pthread_attr_setstacksize(&attr, stacksize);

    /* Ready to spawn our threads. We use the single argument the thread
     * function accepts in order to pass the job ID the thread is
     * responsible of. */
    if (pthread_create(&thread, &attr, bio_process_jobs, this) != 0) {
        LOG_ERROR("Fatal: Can't initialize Background Jobs.");
        return false;
    }
    m_thread = thread;
    return true;
}

void* Bio::bio_process_jobs(void* arg) {
    Bio* mgr = (Bio*)arg;
    printf("mgr ptr: %p\n", mgr);

    sigset_t sigset;
    /* Make the thread killable at any time, so that bioKillThreads()
     * can work reliably. */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    /* Block SIGALRM so we are sure that only the main thread will
     * receive the watchdog signal. */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    if (pthread_sigmask(SIG_BLOCK, &sigset, NULL)) {
        // LOG_WARN("Warning: can't mask SIGALRM in bio.c thread: %s", strerror(errno));
    }

    while (!mgr->m_stop_thread) {
        printf("task size: %lu\n", g_tasks.size());
        zk_task_t* task = nullptr;

        /* no lock for logic, but data. 
         * take one from task's list, and then handle it. */
        pthread_mutex_lock(&g_mutex);
        if (g_tasks.size() != 0) {
            task = *g_tasks.begin();
            g_tasks.erase(g_tasks.begin());
        }
        pthread_mutex_unlock(&g_mutex);

        if (task == nullptr) {
            sleep(1);
            continue;
        }

        mgr->process_task(task);
        SAFE_DELETE(task);
    }

    return nullptr;
}

}  // namespace kim
