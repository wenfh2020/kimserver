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

#include "protobuf/sys/nodes.pb.h"
#include "server.h"
#include "util/hash.h"
#include "util/util.h"
#define gettid() syscall(SYS_gettid)
#include <google/protobuf/util/json_util.h>
#include <pthread.h>
using google::protobuf::util::JsonStringToMessage;

#define CONFIG (*m_config)

namespace kim {

ZkMgr::ZkMgr(Log* logger, CJsonObject* config)
    : m_logger(logger), m_config(config) {
}

ZkMgr::~ZkMgr() {
    SAFE_DELETE(m_zk_client);
}

bool ZkMgr::load_zk_client() {
    if (CONFIG["zookeeper"].IsEmpty()) {
        return false;
    }

    std::string zk_servers(CONFIG["zookeeper"]("servers"));
    std::string zk_log_path(CONFIG["zookeeper"]("log_path"));
    if (zk_servers.empty() || zk_log_path.empty()) {
        LOG_ERROR("invalid zookeeper param! servers: %s, zk log path: %s",
                  zk_servers.c_str(), zk_log_path.c_str());
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
    m_zk_client->set_zk_log(zk_log_path, utility::zoo_log_lvl::zoo_log_lvl_debug);

    if (!m_zk_client->connect(zk_servers)) {
        LOG_ERROR("init servers failed! servers: %s", zk_servers.c_str());
        SAFE_DELETE(m_zk_client);
        return false;
    }

    /* attach callback. */
    m_zk_client->attach_zk_watch_events(&on_zk_data_change, &on_zk_child_change, (void*)m_zk_client);
    return true;
}

void ZkMgr::on_repeat_timer() {
    /* requests. */
    watch_nodes(m_zk_oper);

    /* acks. */
}

void ZkMgr::watch_nodes(ZK_CMD oper) {
    // LOG_DEBUG("watch_nodes.........2 - %d, %p", pthread_self(), &m_zk_client);
    if (m_zk_client == nullptr ||
        m_zk_oper < ZK_CMD::CHECK_NODE || m_zk_oper >= ZK_CMD::END) {
        return;
    }

    std::string node_path;
    std::string node_type(CONFIG("node_type"));
    std::string node_root(CONFIG["zookeeper"]("root"));
    std::string server_name(CONFIG("server_name"));
    std::string node_name = format_str("%s-%s", server_name.c_str(), node_type.c_str());
    if (node_type.empty() || node_root.empty() || server_name.empty()) {
        LOG_ERROR("invalid params! node type: %s, node root: %s, server_name: %s",
                  node_type.c_str(), node_root.c_str(), server_name.c_str());
        return;
    }

    switch (m_zk_oper) {
        case ZK_CMD::CHECK_NODE: {
            // node_path = format_str("%s/%s", node_root.c_str(), node_type.c_str());
            // if (!m_zk_client->zk_exists(node_path, (void*)this)) {
            //     LOG_ERROR("zk check node: %s failed!, error: %d.",
            //               node_path.c_str(), m_zk_client->zk_get_state());
            //     return;
            // }
            // LOG_INFO("zk check node: %s", node_path.c_str());
            // break;
        }
        case ZK_CMD::REGISTER: {
            // node_path = format_str("%s/%s/%s", node_root.c_str(), node_type.c_str(), node_name.c_str());

            // zk_node znode;
            // znode.set_path(node_path);
            // znode.set_type(node_type);
            // znode.set_ip(CONFIG("bind"));
            // znode.set_port(std::stoi(CONFIG("port")));
            // znode.set_worker_cnt(std::stoi(CONFIG("worker_cnt")));

            // std::string json;
            // if (!proto_to_json(znode, json)) {
            //     LOG_ERROR("register zk node, proto covert to json failed!");
            //     return;
            // }
            // if (!m_zk_client->zk_create(node_path, json, 3, (void*)this)) {
            //     LOG_ERROR("register zk node: %s, failed! error: %d",
            //               node_path.c_str(), m_zk_client->zk_get_state());
            //     return;
            // }
            // LOG_INFO("register zk node: %s", node_path.c_str());
            break;
        }
        case ZK_CMD::WATCH_NODES: {
            // kim::CJsonObject& o = CONFIG["zookeeper"]["subscribe_node_type"];
            // for (int i = 0; i < o.GetArraySize(); i++) {
            //     node_path = format_str("%s/%s", node_root.c_str(), o(i).c_str());
            //     if (!m_zk_client->zk_watch_children(node_path, (void*)this)) {
            //         LOG_ERROR("watch node: %s chilren failed! error: %d",
            //                   node_path.c_str(), m_zk_client->zk_get_state());
            //     } else {
            //         LOG_INFO("watch children node: %s", node_path.c_str());
            //     }
            // }
            break;
        }
        case ZK_CMD::GET_DATA: {
            break;
        }
        default: {
            LOG_ERROR("invalid zk operation: %d", m_zk_oper);
            break;
        }
    }
}

void ZkMgr::on_zookeeper_commnad(const zk_task_t* task) {
    LOG_INFO(
        "cmd cb: oper [%s], path: [%s], value: [%s], flag: [%d], "
        "error: [%d], errstr: [%s], res value: [%s], privdata: [%p]",
        m_zk_client->task_oper_to_string(task->oper), task->path.c_str(),
        task->value.c_str(), task->flag,
        task->res.error, task->res.errstr.c_str(),
        task->res.value.c_str(), task->privdata);

    switch (task->oper) {
        case zk_task_t::CMD::EXISTS: {
            if (task->res.error != utility::zoo_rc::z_ok) {
                LOG_ERROR("check node error! node: %s, error: %d, errstr: %s",
                          task->path.c_str(), task->res.error, task->res.errstr.c_str());
                return;
            }
            LOG_INFO("node exists! node: %s", task->path.c_str());
            m_zk_oper = ZK_CMD::REGISTER;
            break;
        }
        case zk_task_t::CMD::CREATE: {
            if (task->res.error != utility::zoo_rc::z_ok) {
                LOG_ERROR("create node failed! node: %s, error: %d, errstr: %s",
                          task->path.c_str(), task->res.error, task->res.errstr.c_str());
                return;
            }
            LOG_INFO("create node success! node: %s", task->res.value.c_str());
            m_zk_oper = ZK_CMD::WATCH_NODES;
            break;
        }
        case zk_task_t::CMD::WATCH_CHILD: {
            if (task->res.error != utility::zoo_rc::z_ok) {
                LOG_ERROR("watch children failed! node: %s, error: %d, errstr: %s",
                          task->path.c_str(), task->res.error, task->res.errstr.c_str());
                return;
            }
            LOG_INFO("watch children success! node: %s", task->res.value.c_str());
            m_zk_oper = ZK_CMD::END;
            break;
        }
            //     case zk_task_t::CMD::WATCH_DATA:
            //     case zk_task_t::CMD::WATCH_CHILD:
            //     case zk_task_t::CMD::GET:
            //         printf("res value: %s\n", task->res.value.c_str());
            //         break;
            //     case zk_task_t::CMD::LIST: {
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