#ifndef __MANAGER_H__
#define __MANAGER_H__

#include "events_callback.h"
#include "net.h"
#include "network.h"
#include "nodes.h"
#include "server.h"

namespace kim {

class Manager : public EventsCallback {
   public:
    Manager();
    virtual ~Manager();

    bool init(const char* conf_path);
    void destory();
    void run();

    /* libev callback. */
    virtual void on_terminated(ev_signal* s) override;
    virtual void on_child_terminated(ev_signal* s) override;
    virtual void on_repeat_timer(void* privdata) override;

   private:
    bool load_logger();
    bool load_network();
    bool load_config(const char* path);

    void create_workers();                /* fork children. */
    bool create_worker(int worker_index); /* creates the specified index process. */
    bool restart_worker(pid_t pid);       /* restart the specified pid process. */
    void restart_workers();               /* delay restart of a process that has been shut down. */
    std::string worker_name(int index);

   private:
    Log* m_logger = nullptr;  /* logger. */
    Network* m_net = nullptr; /* net work. */

    CJsonObject m_conf, m_old_conf;   /* config. */
    node_info m_node_info;            /* cluster node. */
    std::list<int> m_restart_workers; /* workers waiting to restart. restore worker's index. */
};

}  // namespace kim

#endif  //__MANAGER_H__