#include "nodes.h"

#include "util/hash.h"
#include "util/util.h"

namespace kim {

Nodes::Nodes(Log* logger, int vnode_cnt, HASH_ALGORITHM ha)
    : m_logger(logger), m_vnode_cnt(vnode_cnt), m_ha(ha) {
}

Nodes::~Nodes() {
    for (auto& it : m_nodes) {
        delete it.second;
    }
    m_nodes.clear();
}

bool Nodes::add_zk_node(const zk_node& node) {
    if (node.path().empty() || node.ip().empty() || node.port() == 0 ||
        node.type().empty() || node.worker_cnt() == 0) {
        LOG_ERROR(
            "add zk node failed, invalid node data! "
            "node path: %s, ip: %s,  port: %d, type: %s, worker_cnt: %d",
            node.path().c_str(), node.ip().c_str(), node.port(),
            node.type().c_str(), node.worker_cnt());
        return false;
    }

    auto it = m_zk_nodes.find(node.path());
    if (it == m_zk_nodes.end()) {
        m_zk_nodes[node.path()] = node;
        for (size_t i = 1; i <= node.worker_cnt(); i++) {
            add_node(node.path(), node.type(), node.ip(), node.port(), i);
        }
    } else {
        auto old = it->second;
        if (old.SerializeAsString() != node.SerializeAsString()) {
            m_zk_nodes[node.path()] = node;
            for (size_t i = 1; i < old.worker_cnt(); i++) {
                del_node(format_nodes_id(old.path(), old.ip(), old.port(), i));
            }
            for (size_t i = 1; i <= node.worker_cnt(); i++) {
                add_node(node.path(), node.type(), node.ip(), node.port(), i);
            }
            LOG_DEBUG("update zk node info done! path: %s", node.path().c_str());
        }
    }

    return true;
}

bool Nodes::del_zk_node(const std::string& path) {
    auto it = m_zk_nodes.find(path);
    if (it == m_zk_nodes.end()) {
        return false;
    }

    /* delete nodes. */
    const zk_node& znode = it->second;
    for (size_t i = 1; i <= znode.worker_cnt(); i++) {
        del_node(format_nodes_id(znode.path(), znode.ip(), znode.port(), i));
    }

    LOG_INFO("delete zk node, path: %s, type: %s, ip: %s, port: %d, worker_cnt: %d",
             znode.path().c_str(), znode.type().c_str(),
             znode.ip().c_str(), znode.port(), znode.worker_cnt());
    m_zk_nodes.erase(it);
    return true;
}

void Nodes::get_zk_diff_nodes(std::vector<std::string>& in,
                              std::vector<std::string>& adds, std::vector<std::string>& dels) {
    std::vector<std::string> vec;
    for (auto& v : m_zk_nodes) {
        vec.push_back(v.first);
    }

    adds = diff_cmp(in, vec);
    dels = diff_cmp(vec, in);
    LOG_DEBUG("cur nodes size: %lu, add size: %lu, dels size: %lu",
              vec.size(), adds.size(), dels.size());
}

bool Nodes::add_node(const std::string& path, const std::string& node_type,
                     const std::string& ip, int port, int worker) {
    LOG_INFO("add node, node type: %s, ip: %s, port: %d, worker: %d",
             node_type.c_str(), ip.c_str(), port, worker);

    std::string node_id = format_nodes_id(path, ip, port, worker);
    if (m_nodes.find(node_id) != m_nodes.end()) {
        LOG_DEBUG("node (%s) has been added!", node_id.c_str());
        return true;
    }

    node_t* node;
    std::vector<uint32_t> vnodes;
    VNODE2NODE_MAP& vnode2node = m_vnodes[node_type];
    size_t old_vnode_cnt = vnode2node.size();

    vnodes = gen_vnodes(node_id);
    node = new node_t{node_id, node_type, ip, port, vnodes};

    for (auto& v : vnodes) {
        if (!vnode2node.insert({v, node}).second) {
            LOG_WARN(
                "duplicate virtual nodes! "
                "vnode: %lu, node type: %s, ip: %s, port: %d, worker: %d.",
                v, node_type.c_str(), ip.c_str(), port, worker);
            continue;
        }
    }

    if (vnode2node.size() == old_vnode_cnt) {
        LOG_ERROR("add virtual nodes failed! node id: %s, node type: %s",
                  node->id.c_str(), node->type.c_str());
        SAFE_DELETE(node);
        return false;
    }

    m_nodes[node_id] = node;
    return true;
}

bool Nodes::del_node(const std::string& node_id) {
    LOG_DEBUG("delete node: %s", node_id.c_str());
    auto it = m_nodes.find(node_id);
    if (it == m_nodes.end()) {
        return false;
    }

    /* clear vnode. */
    node_t* node = it->second;
    auto itr = m_vnodes.find(node->type);
    if (itr != m_vnodes.end()) {
        for (auto& v : node->vnodes) {
            itr->second.erase(v);
        }
    }

    delete node;
    m_nodes.erase(it);
    return true;
}

node_t* Nodes::get_node_in_hash(const std::string& node_type, int obj) {
    return get_node_in_hash(node_type, std::to_string(obj));
}

node_t* Nodes::get_node_in_hash(const std::string& node_type, const std::string& obj) {
    auto it = m_vnodes.find(node_type);
    if (it == m_vnodes.end()) {
        return nullptr;
    }

    uint32_t hash_key = hash(obj);
    const VNODE2NODE_MAP& vnode2node = it->second;
    if (vnode2node.size() == 0) {
        LOG_WARN(
            "can't not find node in virtual nodes. node type: %s, obj: %s, hash key: %lu",
            node_type.c_str(), obj.c_str(), hash_key);
        return nullptr;
    }

    auto itr = vnode2node.lower_bound(hash_key);
    if (itr == vnode2node.end()) {
        itr = vnode2node.begin();
    }
    return itr->second;
}

uint32_t Nodes::hash(const std::string& obj) {
    if (m_ha == HASH_ALGORITHM::FNV1_64) {
        return hash_fnv1_64(obj.c_str(), obj.size());
    } else if (m_ha == HASH_ALGORITHM::MURMUR3_32) {
        return murmur3_32(obj.c_str(), obj.size(), 0x000001b3);
    } else {
        return hash_fnv1a_64(obj.c_str(), obj.size());
    }
}

std::vector<uint32_t> Nodes::gen_vnodes(const std::string& node_id) {
    std::string s;
    int hash_point = 4;
    std::vector<uint32_t> vnodes;

    for (int i = 0; i < m_vnode_cnt / hash_point; i++) {
        s = md5(format_str("%d@%s#%d", m_vnode_cnt - i, node_id.c_str(), i));
        for (int j = 0; j < hash_point; j++) {
            uint32_t v = ((uint32_t)(s[3 + j * hash_point] & 0xFF) << 24) |
                         ((uint32_t)(s[2 + j * hash_point] & 0xFF) << 16) |
                         ((uint32_t)(s[1 + j * hash_point] & 0xFF) << 8) |
                         (s[j * hash_point] & 0xFF);
            vnodes.push_back(v);
        }
    }
    return vnodes;
}

}  // namespace kim
