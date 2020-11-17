#include "zk_client.h"

#include "protobuf/sys/nodes.pb.h"
#include "util/util.h"

namespace kim {

static const char* cmd_to_string(zk_task_t::CMD cmd) {
    if (cmd == zk_task_t::CMD::UNKNOWN) {
        return "unknwon";
    } else if (cmd == zk_task_t::CMD::REGISTER) {
        return "register";
    } else if (cmd == zk_task_t::CMD::GET) {
        return "get";
    } else if (cmd == zk_task_t::CMD::NOTIFY_SESSION_CONNECTD) {
        return "session connected";
    } else if (cmd == zk_task_t::CMD::NOTIFY_SESSION_CONNECTING) {
        return "session connecting";
    } else if (cmd == zk_task_t::CMD::NOTIFY_SESSION_EXPIRED) {
        return "session expired";
    } else if (cmd == zk_task_t::CMD::NOTIFY_NODE_CREATED) {
        return "node created";
    } else if (cmd == zk_task_t::CMD::NOTIFY_NODE_DELETED) {
        return "node deleted";
    } else if (cmd == zk_task_t::CMD::NOTIFY_NODE_DATA_CAHNGED) {
        return "data change";
    } else if (cmd == zk_task_t::CMD::NOTIFY_NODE_CHILD_CAHNGED) {
        return "child change";
    } else {
        return "invalid cmd";
    }
}

ZkClient::ZkClient(Log* logger) : Bio(logger), m_zk(nullptr) {
}

ZkClient::~ZkClient() {
    bio_stop();
    SAFE_DELETE(m_zk);
    SAFE_DELETE(m_nodes);
}

bool ZkClient::init(const CJsonObject& config) {
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

    m_zk = new utility::zk_cpp;
    if (m_zk == nullptr) {
        LOG_ERROR("alloc zk_cpp failed!");
        return false;
    }

    m_nodes = new Nodes(m_logger);
    if (m_nodes == nullptr) {
        LOG_ERROR("alloc nodes obj failed!");
        return false;
    }

    /* set zk log firstly. */
    set_zk_log(zk_log_path, utility::zoo_log_lvl::zoo_log_lvl_debug);
    if (!connect(zk_servers)) {
        LOG_ERROR("init servers failed! servers: %s", zk_servers.c_str());
        return false;
    }

    if (!node_register()) {
        LOG_ERROR("register node to zookeeper failed!");
        return false;
    }

    return true;
}

bool ZkClient::connect(const std::string& servers) {
    LOG_DEBUG("servers: %s", servers.c_str());
    if (servers.empty()) {
        return false;
    }

    utility::zoo_rc ret = m_zk->connect(servers, on_zookeeper_watch_events, (void*)this);
    if (ret != utility::z_ok) {
        LOG_ERROR("try connect zk server failed, code[%d][%s]",
                  ret, utility::zk_cpp::error_string(ret));
        return false;
    }

    /* crearte bio thread. */
    if (!bio_init()) {
        LOG_ERROR("create thread failed!");
        return false;
    }
    return true;
}

bool ZkClient::reconnect() {
    std::string servers(m_config["zookeeper"]("servers"));
    utility::zoo_rc ret = m_zk->connect(servers, on_zookeeper_watch_events, (void*)this);
    if (ret != utility::z_ok) {
        LOG_ERROR("try reconnect zk server failed, code[%d][%s]",
                  ret, utility::zk_cpp::error_string(ret));
        return false;
    }

    if (!node_register()) {
        LOG_ERROR("register node to zookeeper failed!");
        return false;
    }

    return true;
}

bool ZkClient::node_register() {
    std::string node_path;
    std::string ip(m_config("node_host"));
    std::string node_type(m_config("node_type"));
    std::string node_root(m_config["zookeeper"]("root"));
    std::string server_name(m_config("server_name"));
    std::string node_name = format_str("%s-%s", server_name.c_str(), node_type.c_str());
    int port = str_to_int(m_config("node_port"));
    int worker_cnt = str_to_int(m_config("worker_cnt"));
    CJsonObject json_node, json_zk, json_value;

    if (node_type.empty() || node_root.empty() ||
        server_name.empty() || ip.empty() || port <= 0 || worker_cnt <= 0) {
        LOG_ERROR("invalid node info!");
        return false;
    }

    node_path = format_str(
        "%s/%s/%s", node_root.c_str(), node_type.c_str(), node_name.c_str());

    /* node info. */
    json_node.Add("path", node_path);
    json_node.Add("type", node_type);
    json_node.Add("ip", ip);
    json_node.Add("port", port);
    json_node.Add("worker_cnt", worker_cnt);
    json_node.Add("active_time", time_now());

    /* zk info. */
    json_zk.Add("root", node_root);
    json_zk.Add("old_path", m_nodes->get_my_zk_node_path());
    json_zk.Add("subscribe_node_type", m_config["zookeeper"]["subscribe_node_type"]);

    json_value.Add("node", json_node);
    json_value.Add("zookeeper", json_zk);
    // LOG_DEBUG("config: %s", json_value.ToFormattedString().c_str());
    return add_cmd_task(node_path, zk_task_t::CMD::REGISTER, json_value.ToString());
}

void ZkClient::set_zk_log(const std::string& path, utility::zoo_log_lvl level) {
    if (m_zk != nullptr) {
        m_zk->set_log_lvl(level);
        m_zk->set_log_stream(path);
    }
}

/* bio thread handle. */
void ZkClient::bio_process_cmd(zk_task_t* task) {
    LOG_TRACE("process task, path: %s, cmd: %d, cmd str: %s",
              task->path.c_str(), task->cmd, cmd_to_string(task->cmd));
    utility::zoo_rc ret = utility::zoo_rc::z_system_error;

    switch (task->cmd) {
        case zk_task_t::CMD::REGISTER: {
            ret = bio_register_node(task);
            break;
        }
        case zk_task_t::CMD::NOTIFY_SESSION_CONNECTD:
        case zk_task_t::CMD::NOTIFY_SESSION_CONNECTING:
        case zk_task_t::CMD::NOTIFY_SESSION_EXPIRED:
        case zk_task_t::CMD::NOTIFY_NODE_CREATED:
        case zk_task_t::CMD::NOTIFY_NODE_DELETED:
            ret = utility::zoo_rc::z_ok;
            break;
        case zk_task_t::CMD::GET:
        case zk_task_t::CMD::NOTIFY_NODE_DATA_CAHNGED:
            ret = m_zk->watch_data_change(task->path.c_str(), task->res.value);
            break;
        case zk_task_t::CMD::NOTIFY_NODE_CHILD_CAHNGED: {
            ret = m_zk->watch_children_event(task->path.c_str(), task->res.values);
            break;
        }
        default: {
            LOG_ERROR("invalid task oper: %d", task->cmd);
            break;
        }
    }

    task->res.error = ret;
    if (task->res.errstr.empty()) {
        task->res.errstr = utility::zk_cpp::error_string(ret);
    }
}

void ZkClient::on_repeat_timer() {
    if (m_is_connected || m_is_expired) {
        if (!m_is_registered && ++m_register_index > 9) {
            reconnect();
            m_register_index = 0;
        }
    }
    Bio::on_repeat_timer();
}

/* 
    json_value: 
    {
        "node": {
            "path": "/kimserver/gate/kimserver-gate",
            "type": "gate",
            "ip": "127.0.0.1",
            "port": "3344",
            "worker_cnt": "1",
            "active_time": 123.134
        },
        "zookeeper": {
            "root": "/kimserver",
            "old_path": "",
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
            "worker_cnt": 1,
            "active_time": 123.134
        }]
    }
*/
utility::zoo_rc ZkClient::bio_register_node(zk_task_t* task) {
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
    LOG_TRACE("node info: %s", json_value.ToFormattedString().c_str());
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
    LOG_TRACE("node info: %s", json_value["node"].ToString().c_str());
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
        ret = m_zk->watch_children_event(parent.c_str(), children);
        if (ret != utility::zoo_rc::z_ok) {
            if (ret == utility::zoo_rc::z_no_node) {
                continue;
            }
            LOG_ERROR("zk get chilren failed! path: %s, error: %d, errstr: %s",
                      parent.c_str(), (int)ret, utility::zk_cpp::error_string(ret));
            return ret;
        }

        /* get and watch children's data. */
        for (size_t i = 0; i < children.size(); i++) {
            path = format_str("%s/%s", parent.c_str(), children[i].c_str());
            ret = m_zk->watch_data_change(path.c_str(), out);
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

    /* if watch node failed, continue! */
    if (ret != utility::zoo_rc::z_ok) {
        ret = utility::zoo_rc::z_ok;
    }

    /* delele old. */
    path = json_value["zookeeper"]("old_path");
    if (!path.empty()) {
        m_zk->delete_node(path.c_str(), -1);
        LOG_INFO("delete old zk path: %s", path.c_str());
    }

    task->res.value = json_res.ToString();
    LOG_INFO("ret: %d, nodes data: %s", int(ret), json_res.ToFormattedString().c_str());
    return ret;
}

void ZkClient::on_zookeeper_watch_events(zhandle_t* zh, int type, int state, const char* path, void* privdata) {
    ZkClient* mgr = (ZkClient*)privdata;
    mgr->on_zk_watch_events(type, state, path, privdata);
}

void ZkClient::on_zk_watch_events(int type, int state, const char* path, void* privdata) {
    std::string path_str = path ? path : "";

    if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_CONNECTING_STATE) {
            add_cmd_task(path_str, zk_task_t::CMD::NOTIFY_SESSION_CONNECTING);
        } else if (state == ZOO_CONNECTED_STATE) {
            add_cmd_task(path_str, zk_task_t::CMD::NOTIFY_SESSION_CONNECTD);
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            add_cmd_task(path_str, zk_task_t::CMD::NOTIFY_SESSION_EXPIRED);
        } else {
            // nothing
        }
    } else if (type == ZOO_CREATED_EVENT) {
        add_cmd_task(path_str, zk_task_t::CMD::NOTIFY_NODE_CREATED);
    } else if (type == ZOO_DELETED_EVENT) {
        add_cmd_task(path_str, zk_task_t::CMD::NOTIFY_NODE_DELETED);
    } else if (type == ZOO_CHANGED_EVENT) {
        add_cmd_task(path_str, zk_task_t::CMD::NOTIFY_NODE_DATA_CAHNGED);
    } else if (type == ZOO_CHILD_EVENT) {
        add_cmd_task(path_str, zk_task_t::CMD::NOTIFY_NODE_CHILD_CAHNGED);
    } else {
        // nothing
    }
}

void ZkClient::timer_process_ack(zk_task_t* task) {
    switch (task->cmd) {
        case zk_task_t::CMD::REGISTER:
            on_zk_register(task);
            break;
        case zk_task_t::CMD::GET:
            on_zk_get_data(task);
            break;
        case zk_task_t::CMD::NOTIFY_SESSION_CONNECTING:
            on_zk_session_connecting(task);
            break;
        case zk_task_t::CMD::NOTIFY_SESSION_CONNECTD:
            on_zk_session_connected(task);
            break;
        case zk_task_t::CMD::NOTIFY_SESSION_EXPIRED:
            on_zk_session_expired(task);
            break;
        case zk_task_t::CMD::NOTIFY_NODE_CREATED:
            on_zk_node_created(task);
            break;
        case zk_task_t::CMD::NOTIFY_NODE_DELETED:
            on_zk_node_deleted(task);
            break;
        case zk_task_t::CMD::NOTIFY_NODE_DATA_CAHNGED:
            on_zk_data_change(task);
            break;
        case zk_task_t::CMD::NOTIFY_NODE_CHILD_CAHNGED:
            on_zk_child_change(task);
            break;
        default:
            break;
    }
}

void ZkClient::on_zk_register(const zk_task_t* task) {
    if (task->res.error != utility::z_ok) {
        LOG_ERROR("on zk register failed!, path: %s, error: %d, errstr: %s",
                  task->path.c_str(), task->res.error, task->res.errstr.c_str());
        return;
    }

    CJsonObject res;
    if (!res.Parse(task->res.value)) {
        LOG_ERROR("parase ack data failed! path: %s", task->path.c_str());
        return;
    }

    m_nodes->clear();
    m_register_index = 0;
    m_is_registered = true;

    /* set new. */
    m_nodes->set_my_zk_node_path(res("my_zk_path"));
    LOG_TRACE("ack data: %s", res.ToFormattedString().c_str());

    zk_node znode;
    CJsonObject& json_nodes = res["nodes"];

    for (int i = 0; i < json_nodes.GetArraySize(); i++) {
        if (!json_to_proto(json_nodes[i].ToString(), znode)) {
            LOG_ERROR("json to proto failed!");
            continue;
        }
        if (!m_nodes->add_zk_node(znode)) {
            LOG_ERROR("add zk node failed! path: %s", json_nodes[i]("path").c_str());
            continue;
        }
        LOG_INFO("add zk node done! path: %s", json_nodes[i]("path").c_str());
    }

    LOG_INFO("on zk register done! path: %s", task->path.c_str());
    m_nodes->print_debug_nodes_info();
}

void ZkClient::on_zk_node_deleted(const kim::zk_task_t* task) {
    LOG_INFO("zk node: %s deleted!", task->path.c_str());
    m_nodes->del_zk_node(task->path);
}

void ZkClient::on_zk_node_created(const kim::zk_task_t* task) {
    LOG_INFO("zk node: %s created!", task->path.c_str());
}

void ZkClient::on_zk_session_connected(const kim::zk_task_t* task) {
    LOG_INFO("session conneted!");
    m_is_connected = true;
    m_is_expired = false;
}

void ZkClient::on_zk_session_connecting(const kim::zk_task_t* task) {
    LOG_INFO("session conneting! path: %s", task->path.c_str());
    m_is_connected = false;
}

void ZkClient::on_zk_session_expired(const kim::zk_task_t* task) {
    LOG_INFO("session expired! path: %s", task->path.c_str());
    m_is_connected = false;
    m_is_registered = false;
    m_is_expired = true;
    reconnect();
}

void ZkClient::on_zk_get_data(const kim::zk_task_t* task) {
    if (task->res.error != utility::z_ok) {
        LOG_ERROR("on zk get data failed!, path: %s, error: %d, errstr: %s",
                  task->path.c_str(), task->res.error, task->res.errstr.c_str());
        return;
    }

    LOG_INFO("on zk get node data! path: %s, new value: %s.",
             task->path.c_str(), task->res.value.c_str());

    zk_node node;
    if (!json_to_proto(task->res.value, node)) {
        LOG_ERROR("invalid node info! path: %s", task->path.c_str());
        return;
    }
    m_nodes->add_zk_node(node);
    m_nodes->print_debug_nodes_info();
}

void ZkClient::on_zk_data_change(const kim::zk_task_t* task) {
    if (task->res.error != utility::z_ok) {
        LOG_ERROR("on zk data change failed!, path: %s, error: %d, errstr: %s",
                  task->path.c_str(), task->res.error, task->res.errstr.c_str());
        return;
    }

    LOG_INFO("on zk node data change! path: %s, new value: %s.",
             task->path.c_str(), task->res.value.c_str());

    zk_node node;
    if (!json_to_proto(task->res.value, node)) {
        LOG_ERROR("invalid node info! path: %s", task->path.c_str());
        return;
    }
    m_nodes->add_zk_node(node);
    m_nodes->print_debug_nodes_info();
}

void ZkClient::on_zk_child_change(const kim::zk_task_t* task) {
    if (task->res.error != utility::z_ok) {
        LOG_ERROR("on zk child change failed!, path: %s, error: %d, errstr: %s",
                  task->path.c_str(), task->res.error, task->res.errstr.c_str());
        return;
    }

    LOG_INFO("on zk child change! path: %s, chilren size: %d",
             task->path.c_str(), task->res.values.size());

    std::string child, type;
    std::vector<std::string> children;
    std::vector<std::string> new_paths, del_paths;
    const std::vector<std::string>& res = task->res.values;

    for (size_t i = 0; i < res.size(); i++) {
        child = format_str("%s/%s", task->path.c_str(), res[i].c_str());
        children.push_back(child);
        LOG_DEBUG("child change, num: %d, %s", i, child.c_str());
    }

    type = task->path.substr(task->path.find_last_of("/") + 1);
    m_nodes->get_zk_diff_nodes(type, children, new_paths, del_paths);

    /* add new nodes. */
    if (new_paths.size() > 0) {
        for (size_t i = 0; i < new_paths.size(); i++) {
            /* get / watch new node data. */
            add_cmd_task(new_paths[i], zk_task_t::CMD::GET);
        }
    }

    /* delete paths. */
    if (del_paths.size() > 0) {
        for (size_t i = 0; i < del_paths.size(); i++) {
            m_nodes->del_zk_node(del_paths[i]);
        }
    }

    m_nodes->print_debug_nodes_info();
}

}  // namespace kim