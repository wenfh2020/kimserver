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

   public:
    /* connect to zk servers. */
    bool connect(const std::string& servers);
    /* my node register to zookeeper. */
    bool node_register();
    void set_zk_log(const std::string& path, utility::zoo_log_lvl level = utility::zoo_log_lvl::zoo_log_lvl_info);

    /* call by timer. */
    virtual void process_ack(zk_task_t* task) override;
    /* call by bio. */
    virtual void process_cmd(zk_task_t* task) override;

    /* callback by zookeeper-c-client. */
    static void on_zookeeper_watch_events(zhandle_t* zh, int type, int state, const char* path, void* privdata);
    void on_zk_watch_events(int type, int state, const char* path, void* privdata);

    /* call by timer. */
    void on_zk_register(const kim::zk_task_t* task);
    void on_zk_get_data(const kim::zk_task_t* task);
    void on_zk_data_change(const kim::zk_task_t* task);
    void on_zk_child_change(const kim::zk_task_t* task);
    void on_zk_node_deleted(const kim::zk_task_t* task);

   private:
    utility::zoo_rc bio_register_node(zk_task_t* task);

   private:
    CJsonObject m_config;
    Nodes* m_nodes = nullptr;

    /* zk. */
    utility::zk_cpp* m_zk;
};

}  // namespace kim

#endif  //__KIM_ZOOKEEPER_CLINET_H__