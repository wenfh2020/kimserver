#ifndef __KIM_ZOOKEEPER_CLINET_H__
#define __KIM_ZOOKEEPER_CLINET_H__

#include "bio.h"
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

    void set_zk_log(const std::string& path, utility::zoo_log_lvl level = utility::zoo_log_lvl::zoo_log_lvl_info);
    void close_log();

    /* connect to zk servers. */
    bool connect(const std::string& servers);

    /* attach callback fn. */
    void attach_zk_cmd_event(ZkCallbackCmdFn* fn) { m_cmd_fn = fn; }
    void attach_zk_watch_events(ZkCallbackWatchDataChangeFn* data_fn, ZkCallbackChildChangeFn* child_fn, void* watch_privdata);

    /* zk api. */
    utility::z_state zk_get_state(); /* no network operation. */
    bool zk_create(const std::string& path, const std::string& value, int flag, void* privdata);
    bool zk_delelte(const std::string& path, void* privdata);
    bool zk_exists(const std::string& path, void* privdata);
    bool zk_list(const std::string& path, void* privdata);
    bool zk_get(const std::string& path, void* privdata);
    bool zk_set(const std::string& path, const std::string& value, void* privdata);
    bool zk_watch_data(const std::string& path, void* privdata);
    bool zk_watch_children(const std::string& path, void* privdata);

   public:
    /* bio handler. */
    virtual void process_req_tasks(zk_task_t* task) override;
    void process_rsp_tasks();

   private:
    utility::zk_cpp* m_zk;
    FILE* m_zk_log_file = nullptr;
    void* m_watch_privdata = nullptr;
    utility::zoo_log_lvl m_zk_log_level = utility::zoo_log_lvl::zoo_log_lvl_info;

    /* callback fn. */
    ZkCallbackCmdFn* m_cmd_fn = nullptr;
    ZkCallbackChildChangeFn* m_child_change_fn = nullptr;
    ZkCallbackWatchDataChangeFn* m_watch_data_fn = nullptr;
};

}  // namespace kim

#endif  //__KIM_ZOOKEEPER_CLINET_H__