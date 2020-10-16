#include "zk_mgr.h"

namespace kim {

ZookeeperMgr::ZookeeperMgr(Log* logger) : Bio(logger), m_zk(nullptr) {
}

ZookeeperMgr::~ZookeeperMgr() {
    close_log();
}

void ZookeeperMgr::close_log() {
    if (m_zk_log_file != nullptr) {
        fclose(m_zk_log_file);
        m_zk_log_file = nullptr;
    }
}

void ZookeeperMgr::set_zk_log(const std::string& path, utility::zoo_log_lvl level) {
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

void ZookeeperMgr::attach_zk_watch_events(ZkCallbackWatchDataChangeFn* data_fn,
                                          ZkCallbackChildChangeFn* child_fn, void* watch_privdata) {
    m_watch_privdata = watch_privdata;
    if (m_zk != nullptr) {
        m_zk->set_watch_privdata(watch_privdata);
    }
    m_watch_data_fn = data_fn;
    m_child_change_fn = child_fn;
}

bool ZookeeperMgr::connect(const std::string& servers) {
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

bool ZookeeperMgr::zk_create(const std::string& path,
                             const std::string& value, int flag, void* privdata) {
    if (path.empty() || privdata == nullptr || flag < 0 || flag > 3) {
        LOG_ERROR("invalid param!");
        return false;
    }

    LOG_DEBUG("zk_create path: %s, value: %s, flag: %d",
              path.c_str(), value.c_str(), flag);
    return add_task(path, zk_task_t::OPERATE::CREATE, privdata, value, flag);
}

bool ZookeeperMgr::zk_delelte(const std::string& path, void* privdata) {
    LOG_DEBUG("zk_delelte path: %s", path.c_str());
    if (path.empty() || privdata == nullptr) {
        LOG_ERROR("invalid param!");
        return false;
    }
    return add_task(path, zk_task_t::OPERATE::DELETE, privdata);
}

bool ZookeeperMgr::zk_exists(const std::string& path, void* privdata) {
    LOG_DEBUG("zk_exists path: %s", path.c_str());
    if (path.empty() || privdata == nullptr) {
        LOG_ERROR("invalid param!");
        return false;
    }
    return add_task(path, zk_task_t::OPERATE::EXISTS, privdata);
}

bool ZookeeperMgr::zk_list(const std::string& path, void* privdata) {
    LOG_DEBUG("zk_list path: %s", path.c_str());
    if (path.empty() || privdata == nullptr) {
        LOG_ERROR("invalid param!");
        return false;
    }
    return add_task(path, zk_task_t::OPERATE::LIST, privdata);
}

bool ZookeeperMgr::zk_get(const std::string& path, void* privdata) {
    LOG_DEBUG("zk_get path: %s", path.c_str());
    if (path.empty() || privdata == nullptr) {
        LOG_ERROR("invalid param!");
        return false;
    }
    return add_task(path, zk_task_t::OPERATE::GET, privdata);
}

bool ZookeeperMgr::zk_set(const std::string& path, const std::string& value, void* privdata) {
    LOG_DEBUG("zk_set path: %s, value: %s", path.c_str(), value.c_str());
    if (path.empty() || privdata == nullptr) {
        LOG_ERROR("invalid param!");
        return false;
    }
    return add_task(path, zk_task_t::OPERATE::SET, privdata, value);
}

bool ZookeeperMgr::zk_watch_data(const std::string& path, void* privdata) {
    LOG_DEBUG("zk_watch_data path: %s", path.c_str());
    if (path.empty() || privdata == nullptr) {
        LOG_ERROR("invalid param!");
        return false;
    }
    return add_task(path, zk_task_t::OPERATE::WATCH_DATA, privdata);
}

bool ZookeeperMgr::zk_watch_children(const std::string& path, void* privdata) {
    LOG_DEBUG("zk_watch_children path: %s", path.c_str());
    if (path.empty() || privdata == nullptr) {
        LOG_ERROR("invalid param!");
        return false;
    }
    return add_task(path, zk_task_t::OPERATE::WATCH_CHILD, privdata);
}

void ZookeeperMgr::process_tasks(zk_task_t* task) {
    LOG_DEBUG("process task, path: %s, oper: %d", task->path.c_str(), task->oper);
    utility::zoo_rc ret = utility::zoo_rc::z_system_error;

    switch (task->oper) {
        case zk_task_t::OPERATE::EXISTS: {
            ret = m_zk->exists_node(task->path.c_str(), nullptr, task->watch);
            break;
        }
        case zk_task_t::OPERATE::GET: {
            ret = m_zk->get_node(task->path.c_str(), task->res.value, nullptr, task->watch);
            break;
        }
        case zk_task_t::OPERATE::SET: {
            ret = m_zk->set_node(task->path.c_str(), task->value, -1);
            break;
        }
        case zk_task_t::OPERATE::CREATE: {
            std::string rpath = task->path;
            ret = utility::z_ok;
            if (task->flag == 0) {
                std::vector<utility::zoo_acl_t> acl;
                acl.push_back(utility::zk_cpp::create_world_acl(utility::zoo_perm_all));
                ret = m_zk->create_persistent_node(task->path.c_str(), task->value, acl);
            } else if (task->flag == 1) {
                std::vector<utility::zoo_acl_t> acl;
                acl.push_back(utility::zk_cpp::create_world_acl(utility::zoo_perm_all));
                ret = m_zk->create_ephemeral_node(task->path.c_str(), task->value, acl);
            } else if (task->flag == 2) {
                std::vector<utility::zoo_acl_t> acl;
                acl.push_back(utility::zk_cpp::create_world_acl(utility::zoo_perm_all));
                ret = m_zk->create_sequence_node(task->path.c_str(), task->value, acl, rpath);
                if (ret == utility::z_ok) {
                    task->res.value = rpath;
                }
            } else if (task->flag == 3) {
                std::vector<utility::zoo_acl_t> acl;
                acl.push_back(utility::zk_cpp::create_world_acl(utility::zoo_perm_all));
                ret = m_zk->create_sequance_ephemeral_node(task->path.c_str(), task->value, acl, rpath);
                if (ret == utility::z_ok) {
                    task->res.value = rpath;
                }
            } else {
                LOG_ERROR("invalid create path task flag: %d", task->flag);
            }
            break;
        }
        case zk_task_t::OPERATE::DELETE: {
            ret = m_zk->delete_node(task->path.c_str(), -1);
            break;
        }
        case zk_task_t::OPERATE::LIST: {
            ret = m_zk->get_children(task->path.c_str(), task->res.values, task->watch);
            break;
        }
        case zk_task_t::OPERATE::WATCH_DATA: {
            if (m_watch_data_fn == nullptr) {
                LOG_ERROR("pls attach m_watch_data_fn! path: %s", task->path.c_str());
                task->res.errstr = "no attach callback func.";
                break;
            }
            ret = m_zk->watch_data_change(task->path.c_str(), m_watch_data_fn, &task->res.value);
            break;
        }
        case zk_task_t::OPERATE::WATCH_CHILD: {
            if (m_child_change_fn == nullptr) {
                LOG_ERROR("pls attach m_child_change_fn! path: %s", task->path.c_str());
                task->res.errstr = "no attach callback func.";
                break;
            }
            ret = m_zk->watch_children_event(task->path.c_str(), m_child_change_fn, &task->res.values);
            break;
        }
        default: {
            LOG_ERROR("invalid task oper: %d", task->oper);
            break;
        }
    }

    if (m_cmd_fn == nullptr) {
        LOG_ERROR("pls call attach_cmd_cb_fn().");
        return;
    }

    task->res.error = ret;
    if (task->res.errstr.empty()) {
        task->res.errstr = utility::zk_cpp::error_string(ret);
    }
    m_cmd_fn(task);
}

}  // namespace kim