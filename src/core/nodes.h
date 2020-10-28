#ifndef __KIM_NODES_H__
#define __KIM_NODES_H__

#include "protobuf/sys/nodes.pb.h"
#include "server.h"
#include "util/log.h"

namespace kim {

/* 
 * nodes manager. 
 * node id: "ip:port.worker_index"
 */

typedef struct node_s {
    std::string id;               /* node id: "ip:port.worker_index" */
    std::string type;             /* node type. */
    std::string ip;               /* node ip. */
    int port;                     /* node port. */
    std::vector<uint32_t> vnodes; /* virtual nodes which point to me. */
    double active_time;           /* time for checking online. */
} node_t;

class Nodes {
   public:
    enum class HASH_ALGORITHM {
        FNV1A_64 = 0,
        FNV1_64 = 1,
        MURMUR3_32 = 2,
    };

   public:
    Nodes(Log* logger, int vnode_cnt = 200, HASH_ALGORITHM ha = HASH_ALGORITHM::FNV1A_64);
    virtual ~Nodes();

    /* nodes. */
    bool add_zk_node(const zk_node& node);
    bool del_zk_node(const std::string& path);

    /* ketama algorithm for node's distribution. */
    bool add_node(const std::string& node_type, const std::string& ip, int port, int worker);
    bool del_node(const std::string& node_id);
    node_t* get_node(const std::string& node_id);
    node_t* get_node_in_hash(const std::string& node_type, int obj);
    node_t* get_node_in_hash(const std::string& node_type, const std::string& obj);

   protected:
    uint32_t hash(const std::string& obj);
    std::vector<uint32_t> gen_vnodes(const std::string& node_id);

   private:
    Log* m_logger = nullptr;
    int m_vnode_cnt = 200;
    HASH_ALGORITHM m_ha = HASH_ALGORITHM::FNV1A_64;

    /* key: zk path, value: zk node info. */
    std::unordered_map<std::string, zk_node> m_zk_nodes;
    /* key: node_id, value: node info. */
    std::unordered_map<std::string, node_t*> m_nodes;

    /* key: vnode(hash) -> node. */
    typedef std::map<uint32_t, node_t*> VNODE2NODE_MAP;
    /* key: node type, value: (vnodes -> node) */
    std::unordered_map<std::string, VNODE2NODE_MAP> m_vnodes;
};

}  // namespace kim

#endif  //__KIM_NODES_H__