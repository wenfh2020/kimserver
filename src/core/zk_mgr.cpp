/* 
 * 在里面全部处理完逻辑，不要放在 mannager.cpp 中去了。
 * config.
 * nodes
 */
#include "zk_mgr.h"

#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "server.h"
#define gettid() syscall(SYS_gettid)
#include <pthread.h>

#define CONFIG (*m_config)

namespace kim {

const char* task_oper_to_string(zk_task_t::OPERATE oper) {
    switch (oper) {
        case zk_task_t::OPERATE::EXISTS:
            return "exists";
        case zk_task_t::OPERATE::SET:
            return "set";
        case zk_task_t::OPERATE::GET:
            return "get";
        case zk_task_t::OPERATE::WATCH_DATA:
            return "watch_data";
        case zk_task_t::OPERATE::WATCH_CHILD:
            return "watch_child";
        case zk_task_t::OPERATE::CREATE:
            return "create";
        case zk_task_t::OPERATE::DELETE:
            return "delete";
        case zk_task_t::OPERATE::LIST:
            return "list";
        default:
            return "invalid";
    }
}

ZkMgr::ZkMgr(Log* logger, CJsonObject* config) : m_logger(logger), m_config(config) {
}

ZkMgr::~ZkMgr() {
    SAFE_DELETE(m_zk_client);
}

bool ZkMgr::load_zk_client() {
    if (CONFIG["zookeeper"].IsEmpty()) {
        return false;
    }

    std::string servers(CONFIG["zookeeper"]("servers"));
    std::string log_path(CONFIG["zookeeper"]("log_path"));
    if (servers.empty() || log_path.empty()) {
        LOG_ERROR("invalid zookeeper param! servers: %s, log_path: %s",
                  servers.c_str(), log_path.c_str());
        return false;
    }

    if (m_zk_client == nullptr) {
        m_zk_client = new ZooKeeperClient(m_logger);
        if (m_zk_client == nullptr) {
            LOG_ERROR("alloc zk mgr failed!");
            return false;
        }
    }

    /* log for zk-c-client. */
    m_zk_client->set_zk_log(log_path, utility::zoo_log_lvl::zoo_log_lvl_debug);

    if (!m_zk_client->connect(servers)) {
        LOG_ERROR("init servers failed! servers: %s", servers.c_str());
        SAFE_DELETE(m_zk_client);
        return false;
    }

    /* attach callback. */
    m_zk_client->attach_zk_cmd_event(&on_zk_commnad);
    m_zk_client->attach_zk_watch_events(&on_zk_data_change, &on_zk_child_change, (void*)m_zk_client);
    return true;
}

void ZkMgr::on_repeat_timer() {
    /* requests. */
    watch_nodes(m_zk_oper);

    /* acks. */
}

void ZkMgr::watch_nodes(ZK_OPERATION oper) {
    // LOG_DEBUG("watch_nodes.........2 - %d, %p", pthread_self(), &m_zk_client);
    if (m_zk_client == nullptr ||
        m_zk_oper < ZK_OPERATION::CHECK_NODE ||
        m_zk_oper >= ZK_OPERATION::END) {
        return;
    }

    char path[256];
    std::string node(CONFIG("node_type"));
    std::string root(CONFIG["zookeeper"]("root"));
    std::string myname(CONFIG["zookeeper"]("myname"));
    if (node.empty() || root.empty() || myname.empty()) {
        LOG_ERROR("invalid params! node type: %s, zk root: %s, myname: %s",
                  node.c_str(), root.c_str(), myname.c_str());
        return;
    }

    switch (m_zk_oper) {
        case ZK_OPERATION::CHECK_NODE: {
            snprintf(path, sizeof(path), "%s/%s", root.c_str(), node.c_str());
            if (!m_zk_client->zk_exists(path, (void*)this)) {
                LOG_ERROR("zk check node: %s failed!", path);
            }
            LOG_INFO("zk check node: %s", path);
            break;
        }
        case ZK_OPERATION::REGISTER: {
            snprintf(path, sizeof(path), "%s/%s/%s", root.c_str(), node.c_str(), myname.c_str());
            if (!m_zk_client->zk_create(path, "test", 3, (void*)this)) {
                LOG_ERROR("register node: %s, failed!", path);
            }
            LOG_INFO("zk register node: %s", path);
            break;
        }
        case ZK_OPERATION::WATCH_NODES: {
            kim::CJsonObject& o = CONFIG["zookeeper"]["subscribe_node_type"];
            for (int i = 0; i < o.GetArraySize(); i++) {
                snprintf(path, sizeof(path), "%s/%s", root.c_str(), o(i).c_str());
                if (!m_zk_client->zk_watch_children(path, (void*)this)) {
                    LOG_ERROR("watch node: %s chilren failed!", path);
                } else {
                    LOG_INFO("watch children node: %s", path);
                }
            }
            break;
        }
        case ZK_OPERATION::GET_DATA: {
            break;
        }
        default: {
            LOG_ERROR("invalid zk operation: %d", m_zk_oper);
            break;
        }
    }
}

void ZkMgr::on_zookeeper_commnad(const zk_task_t* task) {
    LOG_DEBUG(
        "cmd cb: oper [%s], path: [%s], value: [%s], flag: [%d], "
        "error: [%d], errstr: [%s], res value: [%s], res values: [%lu],"
        "privdata: [%p]\n",
        task_oper_to_string(task->oper), task->path.c_str(),
        task->value.c_str(), task->flag,
        task->res.error, task->res.errstr.c_str(),
        task->res.value.c_str(), task->res.values.size(),
        task->privdata);

    switch (task->oper) {
        case zk_task_t::OPERATE::EXISTS: {
            if (task->res.error != utility::zoo_rc::z_ok) {
                LOG_ERROR("check node error! node: %s, error: %d, errstr: %s",
                          task->path.c_str(), task->res.error, task->res.errstr.c_str());
                return;
            }
            /* if error not exists delete node. */
            LOG_INFO("node exists! node: %s", task->path.c_str());
            m_zk_oper = ZK_OPERATION::REGISTER;
            break;
        }
        case zk_task_t::OPERATE::CREATE: {
            if (task->res.error != utility::zoo_rc::z_ok) {
                LOG_ERROR("create node failed! node: %s, error: %d, errstr: %s",
                          task->path.c_str(), task->res.error, task->res.errstr.c_str());
                return;
            }
            LOG_INFO("create node success! node: %s", task->res.value.c_str());
            m_zk_oper = ZK_OPERATION::WATCH_NODES;
            break;
        }
        case zk_task_t::OPERATE::WATCH_CHILD: {
            if (task->res.error != utility::zoo_rc::z_ok) {
                LOG_ERROR("watch children failed! node: %s, error: %d, errstr: %s",
                          task->path.c_str(), task->res.error, task->res.errstr.c_str());
                return;
            }
            LOG_INFO("watch children success! node: %s", task->res.value.c_str());
            m_zk_oper = ZK_OPERATION::END;
            break;
        }
            //     case zk_task_t::OPERATE::WATCH_DATA:
            //     case zk_task_t::OPERATE::WATCH_CHILD:
            //     case zk_task_t::OPERATE::GET:
            //         printf("res value: %s\n", task->res.value.c_str());
            //         break;
            //     case zk_task_t::OPERATE::LIST: {
            //         std::string list;
            //         list.append("[");
            //         for (size_t i = 0; i < task->res.values.size(); i++) {
            //             //printf("%s\n", children[i].c_str());
            //             list.append(task->res.values[i]).append(", ");
            //         }
            //         list.append("]");
            //         printf("%s\n", list.c_str());
            //         break;
            //     }
        default:
            printf("invalid task oper: %d\n", task->oper);
            break;
    }
}

void ZkMgr::on_zk_commnad(const zk_task_t* task) {
    ZkMgr* mgr = (ZkMgr*)task->privdata;
    mgr->on_zookeeper_commnad(task);
}

void ZkMgr::on_zookeeper_data_change(const std::string& path, const std::string& new_value) {
    printf("--------\n");
    printf("on_zk_data_change, path[%s] new_data[%s]\n",
           path.c_str(), new_value.c_str());
}

void ZkMgr::on_zookeeper_child_change(const std::string& path, const std::vector<std::string>& children) {
    printf("--------\n");
    printf("on_zk_child_change, path[%s] new_child_count[%d]\n",
           path.c_str(), (int32_t)children.size());

    for (size_t i = 0; i < children.size(); i++) {
        printf("%zu, %s\n", i, children[i].c_str());
    }
}

void ZkMgr::on_zk_data_change(const std::string& path, const std::string& new_value, void* privdata) {
    ZkMgr* mgr = (ZkMgr*)privdata;
    mgr->on_zookeeper_data_change(path, new_value);
}

void ZkMgr::on_zk_child_change(const std::string& path, const std::vector<std::string>& children, void* privdata) {
    ZkMgr* mgr = (ZkMgr*)privdata;
    mgr->on_zookeeper_child_change(path, children);
}

}  // namespace kim