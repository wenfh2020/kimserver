#include "zk_client.h"

#include "protobuf/sys/nodes.pb.h"
#include "util/util.h"

#define __NO_LOG__

namespace kim {

const char* ZooKeeperClient::task_oper_to_string(zk_task_t::CMD oper) {
    switch (oper) {
        case zk_task_t::CMD::EXISTS:
            return "exists";
        case zk_task_t::CMD::SET:
            return "set";
        case zk_task_t::CMD::GET:
            return "get";
        case zk_task_t::CMD::WATCH_DATA:
            return "watch_data";
        case zk_task_t::CMD::WATCH_CHILD:
            return "watch_child";
        case zk_task_t::CMD::CREATE:
            return "create";
        case zk_task_t::CMD::DELETE:
            return "delete";
        case zk_task_t::CMD::LIST:
            return "list";
        default:
            return "invalid";
    }
}

ZooKeeperClient::ZooKeeperClient(Log* logger) : Bio(logger), m_zk(nullptr) {
}

ZooKeeperClient::~ZooKeeperClient() {
    close_log();
}

void ZooKeeperClient::close_log() {
    if (m_zk_log_file != nullptr) {
        fclose(m_zk_log_file);
        m_zk_log_file = nullptr;
    }
}

bool ZooKeeperClient::init(const CJsonObject& config) {
    m_config = config;
    if (m_config["zookeeper"].IsEmpty()) {
        LOG_ERROR("no zookeeper config!");
        return false;
    }

    std::string zk_servers(m_config["zookeeper"]("servers"));
    std::string zk_log_path(m_config["zookeeper"]("log_path"));
    if (zk_servers.empty() || zk_log_path.empty()) {
        LOG_ERROR("invalid zookeeper param! servers: %s, zk log path: %s",
                  zk_servers.c_str(), zk_log_path.c_str());
        return false;
    }

    set_zk_log(zk_log_path, utility::zoo_log_lvl::zoo_log_lvl_debug);
    if (!connect(zk_servers)) {
        LOG_ERROR("init servers failed! servers: %s", zk_servers.c_str());
        return false;
    }
    attach_zk_watch_events(&on_zk_data_change, &on_zk_child_change, (void*)this);
    return true;
}

/* 
    json_value: 
    {
        "node": {
            "path": "/kimserver/gate/kimserver-gate",
            "type": "gate",
            "ip": "127.0.0.1",
            "port": "3344",
            "worker_cnt": "1"
        },
        "zookeeper": {
            "root": "/kimserver",
            "subscribe_node_type": ["gate", "logic"]
        }
    }

    json_res:
    {
        "my_zk_path": "/kimserver/gate/kimserver-gate000000001",
        "nodes": [{
            "path": "/kimserver/gate/kimserver-gate000000001",
            "ip": "127.0.0.1",
            "port": 3344,
            "type": "gate",
            "worker_cnt": 1
        }]
    }
*/
utility::zoo_rc ZooKeeperClient::bio_register_node(zk_task_t* task) {
    std::vector<std::string> children;
    std::vector<utility::zoo_acl_t> acl;
    CJsonObject json_value, json_data, json_res;
    std::string path, root, out, parent(task->path);
    utility::zoo_rc ret = utility::zoo_rc::z_system_error;
    parent = parent.substr(0, parent.find_last_of("/"));

    if (!json_value.Parse(task->value)) {
        LOG_ERROR("value convert json object failed!");
        return ret;
    }

    root = json_value["zookeeper"]("root");
    CJsonObject& node_types = json_value["zookeeper"]["subscribe_node_type"];
    LOG_DEBUG("node info: %s", json_value.ToFormattedString().c_str());
    if (!node_types.IsArray()) {
        LOG_ERROR("node types is not array!");
        return ret;
    }

    /* check parent. */
    ret = m_zk->exists_node(parent.c_str(), nullptr, true);
    if (ret != utility::zoo_rc::z_ok) {
        LOG_ERROR("zk parent node: %s not exists! error: %d, errstr: %s",
                  parent.c_str(), (int)ret, utility::zk_cpp::error_string(ret));
        return ret;
    }

    /* check node. */
    ret = m_zk->exists_node(task->path.c_str(), nullptr, true);
    if (ret != utility::zoo_rc::z_no_node) {
        LOG_ERROR("zk node: %s exists! error: %d, errstr: %s",
                  task->path.c_str(), (int)ret, utility::zk_cpp::error_string(ret));
        return ret;
    }

    /* create node. */
    acl.push_back(utility::zk_cpp::create_world_acl(utility::zoo_perm_all));
    ret = m_zk->create_sequance_ephemeral_node(task->path.c_str(), "", acl, path);
    if (ret != utility::zoo_rc::z_ok) {
        LOG_ERROR("create zk node: %s failed! error: %d, errstr: %s",
                  task->path.c_str(), (int)ret, utility::zk_cpp::error_string(ret));
        return ret;
    }
    LOG_INFO("create node: %s done!", path.c_str());

    /* set node data. */
    json_value["node"].Replace("path", path);
    LOG_DEBUG("node info: %s", json_value["node"].ToString().c_str());
    ret = m_zk->set_node(path.c_str(), json_value["node"].ToString(), -1);
    if (ret != utility::zoo_rc::z_ok) {
        LOG_ERROR("set zk node data failed! path, error: %d, errstr: %s",
                  path.c_str(), (int)ret, utility::zk_cpp::error_string(ret));
        return ret;
    }

    json_res.Add("my_zk_path", path);
    json_res.Add("nodes", kim::CJsonObject("[]"));

    for (int i = 0; i < node_types.GetArraySize(); i++) {
        children.clear();
        parent = format_str("%s/%s", root.c_str(), node_types(i).c_str());

        /* get and watch children change. */
        ret = m_zk->watch_children_event(parent.c_str(), m_child_change_fn, &children);
        if (ret != utility::zoo_rc::z_ok) {
            if (ret == utility::zoo_rc::z_no_node) {
                continue;
            }
            LOG_ERROR("zk get chilren failed! path: %s, error: %d, errstr: %s",
                      parent.c_str(), (int)ret, utility::zk_cpp::error_string(ret));
            return ret;
        }

        /* get and watch children's data. */
        for (int i = 0; i < children.size(); i++) {
            path = format_str("%s/%s", parent.c_str(), children[i].c_str());
            ret = m_zk->watch_data_change(path.c_str(), m_watch_data_fn, &out);
            if (ret != utility::zoo_rc::z_ok) {
                LOG_ERROR("zk get get node: %s failed! error: %d, errstr: %s",
                          path.c_str(), (int)ret, utility::zk_cpp::error_string(ret));
                continue;
            }
            if (out.empty() || !json_data.Parse(out)) {
                LOG_ERROR("invalid node data, pls set json! path: %s", path.c_str());
                continue;
            }
            json_res["nodes"].Add(json_data);
        }
    }

    task->res.value = json_res.ToString();
    LOG_INFO("nodes data: %s", json_res.ToFormattedString().c_str());
    return ret;
}

void ZooKeeperClient::handle_cmds() {
    if (m_zk == nullptr || m_zk_old == m_zk_cmd ||
        m_zk_cmd < zk_task_t::CMD::REGISTER ||
        m_zk_cmd >= zk_task_t::CMD::END) {
        return;
    }

    /* watch child chage. */
    if (m_child_change_fn == nullptr || m_watch_data_fn == nullptr) {
        LOG_ERROR("pls attach watch callback fnc!");
        return;
    }

    std::string node_path;
    std::string node_type(m_config("node_type"));
    std::string node_root(m_config["zookeeper"]("root"));
    std::string server_name(m_config("server_name"));
    std::string node_name = format_str("%s-%s", server_name.c_str(), node_type.c_str());
    if (node_type.empty() || node_root.empty() || server_name.empty()) {
        LOG_ERROR("invalid params! node type: %s, node root: %s, server_name: %s",
                  node_type.c_str(), node_root.c_str(), server_name.c_str());
        return;
    }

    switch (m_zk_cmd) {
        case zk_task_t::CMD::REGISTER: {
            node_path = format_str(
                "%s/%s/%s", node_root.c_str(), node_type.c_str(), node_name.c_str());
            CJsonObject json_node, json_value;
            json_node.Add("path", node_path);
            json_node.Add("type", m_config("node_type"));
            json_node.Add("ip", m_config("bind"));
            json_node.Add("port", m_config("port"));
            json_node.Add("worker_cnt", m_config("worker_cnt"));
            json_value.Add("node", json_node);
            json_value.Add("zookeeper", m_config["zookeeper"]);
            // LOG_DEBUG("config: %s", json_value.ToFormattedString().c_str());
            add_req_task(node_path, zk_task_t::CMD::REGISTER, (void*)this, json_value.ToString());
            m_zk_old = m_zk_cmd;
            break;
        }
        default: {
            break;
        }
    }
}

void ZooKeeperClient::handle_rsps() {
    zk_task_t* task = nullptr;

    pthread_mutex_lock(&m_mutex);
    if (m_rsp_tasks.size() > 0) {
        task = *m_rsp_tasks.begin();
        m_rsp_tasks.erase(m_rsp_tasks.begin());
    }
    pthread_mutex_unlock(&m_mutex);

    if (task != nullptr) {
        on_zookeeper_commnad(task);
        SAFE_DELETE(task);
    }
}

void ZooKeeperClient::set_zk_log(const std::string& path, utility::zoo_log_lvl level) {
    FILE* fp = fopen(path.c_str(), "a");
    if (fp == nullptr) {
        LOG_ERROR("open file failed! path: %s", path.c_str());
        return;
    }

    close_log();
    m_zk_log_file = fp;
    m_zk_log_level = level;

    if (m_zk != nullptr) {
        m_zk->set_log_lvl(m_zk_log_level);
        m_zk->set_log_stream(m_zk_log_file);
    }
}

void ZooKeeperClient::attach_zk_watch_events(ZkCallbackWatchDataChangeFn* data_fn,
                                             ZkCallbackChildChangeFn* child_fn, void* watch_privdata) {
    m_watch_privdata = watch_privdata;
    if (m_zk != nullptr) {
        m_zk->set_watch_privdata(watch_privdata);
    }
    m_watch_data_fn = data_fn;
    m_child_change_fn = child_fn;
}

bool ZooKeeperClient::connect(const std::string& servers) {
    LOG_INFO("servers: %s", servers.c_str());
    if (servers.empty()) {
        return false;
    }

    SAFE_DELETE(m_zk);
    m_zk = new utility::zk_cpp;
    if (m_zk == nullptr) {
        LOG_ERROR("alloc zk_cpp failed!");
        return false;
    }

    m_zk->set_log_lvl(m_zk_log_level);
    m_zk->set_log_stream(m_zk_log_file);

    if (m_zk != nullptr && m_watch_privdata != nullptr) {
        m_zk->set_watch_privdata(m_watch_privdata);
    }

    utility::zoo_rc ret = m_zk->connect(servers);
    if (ret != utility::z_ok) {
        LOG_ERROR("try connect zk server failed, code[%d][%s]",
                  ret, utility::zk_cpp::error_string(ret));
        return false;
    }

    if (!bio_init()) {
        LOG_ERROR("create thread failed!");
        return false;
    }
    return true;
}

void ZooKeeperClient::process_req_tasks(zk_task_t* task) {
    LOG_DEBUG("process task, path: %s, oper: %d", task->path.c_str(), task->oper);
    utility::zoo_rc ret = utility::zoo_rc::z_system_error;

    switch (task->oper) {
        case zk_task_t::CMD::REGISTER: {
            ret = bio_register_node(task);
            break;
        }
        default: {
            LOG_ERROR("invalid task oper: %d", task->oper);
            break;
        }
    }

    task->res.error = ret;
    if (task->res.errstr.empty()) {
        task->res.errstr = utility::zk_cpp::error_string(ret);
    }
}

void ZooKeeperClient::on_zookeeper_commnad(const zk_task_t* task) {
    LOG_INFO(
        "cmd cb: oper [%s], path: [%s], value: [%s], flag: [%d], "
        "error: [%d], errstr: [%s], res value: [%s], privdata: [%p]",
        task_oper_to_string(task->oper), task->path.c_str(),
        task->value.c_str(), task->flag,
        task->res.error, task->res.errstr.c_str(),
        task->res.value.c_str(), task->privdata);

    switch (task->oper) {
        case zk_task_t::CMD::REGISTER:
            /* code */
            break;

        default:
            break;
    }
}

void ZooKeeperClient::on_zookeeper_data_change(const std::string& path, const std::string& new_val) {
    printf("--------\n");
    printf("on_zk_data_change, path[%s] new_data[%s]\n",
           path.c_str(), new_val.c_str());
}

void ZooKeeperClient::on_zookeeper_child_change(const std::string& path, const std::vector<std::string>& children) {
    printf("--------\n");
    printf("on_zk_child_change, path[%s] new_child_count[%d]\n",
           path.c_str(), (int32_t)children.size());

    for (size_t i = 0; i < children.size(); i++) {
        printf("%zu, %s\n", i, children[i].c_str());
    }
}

void ZooKeeperClient::on_zk_data_change(const std::string& path, const std::string& new_val, void* privdata) {
    ZooKeeperClient* mgr = (ZooKeeperClient*)privdata;
    mgr->on_zookeeper_data_change(path, new_val);
}

void ZooKeeperClient::on_zk_child_change(const std::string& path, const std::vector<std::string>& children, void* privdata) {
    ZooKeeperClient* mgr = (ZooKeeperClient*)privdata;
    mgr->on_zookeeper_child_change(path, children);
}

void ZooKeeperClient::on_repeat_timer() {
    /* requests. */
    handle_cmds();
    handle_rsps();

    /* acks. */
}

}  // namespace kim