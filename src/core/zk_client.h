#ifndef __KIM_ZOOKEEPER_CLIENT_H__
#define __KIM_ZOOKEEPER_CLIENT_H__

#include "net.h"
#include "nodes.h"
#include "util/json/CJsonObject.hpp"
#include "util/log.h"
#include "zookeeper/zk.h"
#include "zookeeper/zk_bio.h"

namespace kim {

class ZkClient : public Bio {
   public:
    ZkClient(Log* logger, INet* net);
    virtual ~ZkClient();

    bool init(const CJsonObject& config);

   public:
    /* connect to zk servers. */
    bool connect(const std::string& servers);
    /* when notify expired, reconnect to zookeeper. */
    bool reconnect();
    /* register to zookeeper. */
    bool node_register();
    /* set zk log before connect. */
    void set_zk_log(const std::string& path, utility::zoo_log_lvl level = utility::zoo_log_lvl_info);

    /* call by bio (sync). */
    virtual void bio_process_cmd(zk_task_t* task) override;
    /* timer (async). */
    virtual void on_repeat_timer() override;
    /* call by timer (async). */
    virtual void timer_process_ack(zk_task_t* task) override;

    /* callback by zookeeper-client-c. */
    static void on_zookeeper_watch_events(zhandle_t* zh, int type, int state, const char* path, void* privdata);
    void on_zk_watch_events(int type, int state, const char* path, void* privdata);

    /* call by timer. */
    void on_zk_register(const kim::zk_task_t* task);
    void on_zk_get_data(const kim::zk_task_t* task);
    void on_zk_data_change(const kim::zk_task_t* task);
    void on_zk_child_change(const kim::zk_task_t* task);
    void on_zk_node_deleted(const kim::zk_task_t* task);
    void on_zk_node_created(const kim::zk_task_t* task);
    void on_zk_session_connected(const kim::zk_task_t* task);
    void on_zk_session_connecting(const kim::zk_task_t* task);
    void on_zk_session_expired(const kim::zk_task_t* task);

   private:
    utility::zoo_rc bio_register_node(zk_task_t* task);

   private:
    INet* m_net = nullptr;
    /* config. */
    CJsonObject m_config;
    /* ketama nodes manager. */
    Nodes* m_nodes = nullptr;

    /* zk. */
    utility::zk_cpp* m_zk = nullptr;
    bool m_is_connected = false;
    bool m_is_registered = false;
    bool m_is_expired = false;
    /* for reconnect. */
    int m_register_index = 0;
};

}  // namespace kim

#endif  //__KIM_ZOOKEEPER_CLIENT_H__