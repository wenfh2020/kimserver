#include <iostream>

#include "nodes.h"
#include "protobuf/sys/nodes.pb.h"
#include "util/json/CJsonObject.hpp"
#include "util/util.h"

#define NODE_TYPE "logic"
#define USER_CNT 100000
#define WORKERS 4

kim::Nodes* nodes = nullptr;
std::map<std::string, kim::zk_node> g_znode_path;
typedef std::map<int, kim::node_t*> USER2NODE_MAP;
typedef std::map<kim::node_t*, int> NODE2USERS_MAP;

void add_node(int cur) {
    kim::zk_node znode;
    znode.set_path(format_str("%s.%d", "test_path", cur));
    znode.set_type(NODE_TYPE);
    znode.set_host(format_str("192.168.0.%d", cur));
    znode.set_port(1223 + cur);
    znode.set_worker_cnt(WORKERS);
    znode.set_active_time(time_now());
    nodes->add_zk_node(znode);
    g_znode_path[znode.path()] = znode;
}

void test_get_distribution() {
    add_node(1);

    kim::node_t* node;
    /* key: node, value: user count. */
    NODE2USERS_MAP calc;
    double begin, spend;

    begin = time_now();
    for (int i = 1; i < USER_CNT; i++) {
        node = nodes->get_node_in_hash(NODE_TYPE, i);
        if (node != nullptr) {
            calc[node] += 1;
        }
    }

    spend = time_now() - begin;
    printf("spend time: %f, avg: %f\n", spend, spend / USER_CNT);
    printf("node\tusers\n");

    for (auto& v : calc) {
        printf("%s\t%d\n", v.first->id.c_str(), v.second);
    }
}

bool get_distribution(USER2NODE_MAP& dist) {
    kim::node_t* node;
    for (int i = 1; i < USER_CNT; i++) {
        node = nodes->get_node_in_hash(NODE_TYPE, i);
        if (node != nullptr) {
            dist[i] = node;
        }
    }
    return (dist.size() != 0);
}

/* statistics the data which still stay in the same node, after add / delete node in cluster,*/
void show_result(int time, const USER2NODE_MAP& old, const USER2NODE_MAP& cur) {
    NODE2USERS_MAP old_res, hit_res;

    for (auto& o : old) {
        old_res[o.second] += 1;
        auto it = cur.find(o.first);
        if (it != cur.end() && it->second == o.second) {
            hit_res[o.second] += 1;
        }
    }

    for (auto& h : hit_res) {
        auto it = old_res.find(h.first);
        if (it != old_res.end()) {
            printf("%d\t%4f\n", time, ((float)h.second / (float)it->second));
        }
    }
}

void test_add_node() {
    USER2NODE_MAP old, cur;

    add_node(1);
    get_distribution(cur);

    printf("nodes\tchange\n");
    for (int i = 2; i <= 10; i++) {
        old = cur;
        add_node(i);
        get_distribution(cur);
        show_result(i, old, cur);
    }
}

void test_delete_node() {
    for (int i = 1; i <= 10; i++) {
        add_node(i);
    }

    int j = 0;
    USER2NODE_MAP old, cur;
    printf("nodes\tchange\n");

    for (auto& v : g_znode_path) {
        get_distribution(cur);
        old = cur;
        nodes->del_zk_node(v.second.path());
        get_distribution(cur);
        show_result(++j, old, cur);

        if (j == (g_znode_path.size() - 1)) {
            break;
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("invalid param!");
        return 0;
    }

    kim::Log* m_logger = new kim::Log;
    if (!m_logger->set_log_path("./kimserver.log")) {
        std::cerr << "set log path failed!" << std::endl;
        SAFE_DELETE(m_logger);
        return 1;
    }
    m_logger->set_level(kim::Log::LL_DEBUG);

    nodes = new kim::Nodes(m_logger);

    std::string cmd(argv[1]);
    if (cmd == "dist") {
        test_get_distribution();
    } else if (cmd == "add") {
        test_add_node();
    } else if (cmd == "delete") {
        test_delete_node();
    } else {
        printf("invalid cmd!");
    }

    return 0;
}