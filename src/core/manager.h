#ifndef __MANAGER_H__
#define __MANAGER_H__

#include "net.h"
#include "network.h"
#include "nodes.h"
#include "server.h"
#include "zk_client.h"

namespace kim {

class Manager {
   public:
    Manager();
    virtual ~Manager();

    bool init(const char* conf_path);
    void destory();
    void run();

    /* libev callback. */
    static void on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents);
    static void on_repeat_timer_callback(struct ev_loop* loop, ev_timer* w, int revents);

    void on_terminated(ev_signal* s);
    void on_child_terminated(ev_signal* s);
    void on_repeat_timer(void* privdata);

   private:
    bool load_logger();
    bool load_network();
    bool load_timer();
    bool load_config(const char* path);
    bool load_zk_mgr();

    void create_workers();                /* fork children. */
    bool create_worker(int worker_index); /* creates the specified index process. */
    bool restart_worker(pid_t pid);       /* restart the specified pid process. */
    void restart_workers();               /* delay restart of a process that has been shut down. */
    std::string worker_name(int index);

   private:
    Log* m_logger = nullptr;     /* logger. */
    Network* m_net = nullptr;    /* net work. */
    ev_timer* m_timer = nullptr; /* repeat timer. */

    CJsonObject m_conf, m_old_conf;   /* config. */
    node_info m_node_info;            /* cluster node. */
    std::list<int> m_restart_workers; /* workers waiting to restart. restore worker's index. */
    ZkClient* m_zk_client = nullptr;  /* zookeeper client. */
};

}  // namespace kim

#endif  //__MANAGER_H__