#ifndef __KIM_ZOOKEEPER_CLINET_H__
#define __KIM_ZOOKEEPER_CLINET_H__

#include "nodes.h"
#include "util/json/CJsonObject.hpp"
#include "util/log.h"
#include "zookeeper/bio.h"
#include "zookeeper/zk.h"

namespace kim {

class ZkClient : public Bio {
   public:
    ZkClient(Log* logger);
    virtual ~ZkClient();

    bool init(const CJsonObject& config);

    /* connect to zk servers. */
    bool connect(const std::string& servers);
    void close_log();
    void set_zk_log(const std::string& path, utility::zoo_log_lvl level = utility::zoo_log_lvl::zoo_log_lvl_info);

    /* callback by zookeeper-c-client. */
    static void on_zk_watch_events(zhandle_t* zh, int type, int state, const char* path, void* privdata);
    void on_zookeeper_watch_events(int type, int state, const char* path, void* privdata);

    /* call by timer. */
    void on_zookeeper_command(const kim::zk_task_t* task);
    void on_zookeeper_data_change(const kim::zk_task_t* task);
    void on_zookeeper_child_change(const kim::zk_task_t* task);
    void on_zookeeper_node_deleted(const kim::zk_task_t* task);

   public:
    bool node_register();
    /* timer. */
    void on_repeat_timer();
    void handle_acks();

    /* bio handler. */
    virtual void process_cmd_tasks(zk_task_t* task) override;

    /* call by timer. */
    void process_ack_tasks(zk_task_t* task);

   private:
    utility::zoo_rc bio_register_node(zk_task_t* task);

   private:
    Nodes* m_nodes;
    CJsonObject m_config;

    /* zk. */
    utility::zk_cpp* m_zk;
    FILE* m_zk_log_file = nullptr;
    void* m_watch_privdata = nullptr;
    utility::zoo_log_lvl m_zk_log_level = utility::zoo_log_lvl::zoo_log_lvl_info;
};

}  // namespace kim

#endif  //__KIM_ZOOKEEPER_CLINET_H__