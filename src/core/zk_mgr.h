#ifndef __KIM_ZK_MGR_H__
#define __KIM_ZK_MGR_H__

#include "util/json/CJsonObject.hpp"
#include "util/log.h"
#include "zookeeper/zk_client.h"

/*
节点发现。
配置管理，持久节点。
父子通信。
负载统计。
*/

namespace kim {

class ZkMgr {
   public:
    enum class ZK_OPERATION {
        CHECK_NODE = 0,
        REGISTER,
        WATCH_NODES,
        GET_DATA,
        END,
    };

   public:
    ZkMgr(Log* logger, CJsonObject* config);
    virtual ~ZkMgr();

    bool load_zk_client();
    void watch_nodes(ZK_OPERATION oper); /* watch nodes by zookeeper client. */

    void on_repeat_timer();

    /* zookeeper callback fn. */
    static void on_zk_commnad(const kim::zk_task_t* task);
    static void on_zk_data_change(const std::string& path, const std::string& new_value, void* privdata);
    static void on_zk_child_change(const std::string& path, const std::vector<std::string>& children, void* privdata);

    void on_zookeeper_commnad(const kim::zk_task_t* task);
    void on_zookeeper_data_change(const std::string& path, const std::string& new_value);
    void on_zookeeper_child_change(const std::string& path, const std::vector<std::string>& children);

   private:
    Log* m_logger = nullptr;
    CJsonObject* m_config;
    ZooKeeperClient* m_zk_client = nullptr;
    ZK_OPERATION m_zk_oper = ZK_OPERATION::CHECK_NODE;
};

}  // namespace kim

#endif  //__KIM_ZK_MGR_H__