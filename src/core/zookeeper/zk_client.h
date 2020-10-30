#ifndef __KIM_ZOOKEEPER_CLINET_H__
#define __KIM_ZOOKEEPER_CLINET_H__

#include "bio.h"
#include "util/json/CJsonObject.hpp"
#include "util/log.h"
#include "zk.h"

namespace kim {

/* callback fn. */
typedef void(ZkCallbackCmdFn)(const zk_task_t* task);
typedef void(ZkCallbackWatchDataChangeFn)(const std::string& path, const std::string& new_value, void* privdata);
typedef void(ZkCallbackChildChangeFn)(const std::string& path, const std::vector<std::string>& children, void* privdata);

class ZooKeeperClient : public Bio {
   public:
    ZooKeeperClient(Log* logger);
    virtual ~ZooKeeperClient();

    bool init(const CJsonObject& config);

    /* connect to zk servers. */
    bool connect(const std::string& servers);
    void close_log();
    void set_zk_log(const std::string& path, utility::zoo_log_lvl level = utility::zoo_log_lvl::zoo_log_lvl_info);
    const char* task_oper_to_string(zk_task_t::CMD oper);

    /* attach callback fn. */
    void attach_zk_watch_events(ZkCallbackWatchDataChangeFn* data_fn, ZkCallbackChildChangeFn* child_fn, void* watch_privdata);

    /* zookeeper callback fn. */
    static void on_zk_data_change(const std::string& path, const std::string& new_value, void* privdata);
    static void on_zk_child_change(const std::string& path, const std::vector<std::string>& children, void* privdata);

    /* callback by timer. */
    void on_zookeeper_commnad(const kim::zk_task_t* task);

    /* callback by zookeeper-c-client. */
    void on_zookeeper_data_change(const std::string& path, const std::string& new_value);
    void on_zookeeper_child_change(const std::string& path, const std::vector<std::string>& children);

   public:
    /* timer. */
    void on_repeat_timer();
    void handle_cmds();
    void handle_rsps();

    /* bio handler. */
    virtual void process_req_tasks(zk_task_t* task) override;

   private:
    utility::zoo_rc bio_register_node(zk_task_t* task);

   private:
    CJsonObject m_config;
    utility::zk_cpp* m_zk;
    FILE* m_zk_log_file = nullptr;
    void* m_watch_privdata = nullptr;
    utility::zoo_log_lvl m_zk_log_level = utility::zoo_log_lvl::zoo_log_lvl_info;
    zk_task_t::CMD m_zk_cmd = zk_task_t::CMD::REGISTER;
    zk_task_t::CMD m_zk_old = zk_task_t::CMD::UNKNOWN;

    /* callback fn. */
    ZkCallbackChildChangeFn* m_child_change_fn = nullptr;
    ZkCallbackWatchDataChangeFn* m_watch_data_fn = nullptr;
};

}  // namespace kim

#endif  //__KIM_ZOOKEEPER_CLINET_H__