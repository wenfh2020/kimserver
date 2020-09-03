#include "redis_mgr.h"

namespace kim {

RedisMgr::RedisMgr(Log* logger, struct ev_loop* loop)
    : m_logger(logger), m_loop(loop) {
}

RedisMgr::~RedisMgr() {
    m_addrs.clear();
    for (auto& it : m_conns) {
        SAFE_DELETE(it.second);
    }
    m_conns.clear();
}

bool RedisMgr::init(CJsonObject& config) {
    std::vector<std::string> nodes;
    config.GetKeys(nodes);

    int port;
    std::string host;

    for (const auto& it : nodes) {
        const CJsonObject& obj = config[it];
        host = obj("host");
        port = atoi(obj("port").c_str());
        if (host.empty() || port == 0) {
            LOG_ERROR("invalid redis node addr: %s", it.c_str());
            m_addrs.clear();
            return false;
        }
        m_addrs.insert({it, {host, port}});
    }

    return true;
}

void RedisMgr::close(const char* node) {
    if (node == nullptr) {
        return;
    }
    auto it = m_conns.find(node);
    if (it == m_conns.end()) {
        return;
    }
    SAFE_DELETE(it->second);
    m_conns.erase(it);
    m_addrs.erase(node);
}

RdsConnection* RedisMgr::get_conn(const char* node) {
    auto it = m_addrs.find(node);
    if (it == m_addrs.end()) {
        LOG_ERROR("invalid redis node: %s!", node);
        return nullptr;
    }

    RdsConnection* c = nullptr;
    int port = it->second.second;
    std::string host = it->second.first;
    std::string identity = format_addr(host, port);

    auto itr = m_conns.find(identity);
    if (itr != m_conns.end()) {
        c = itr->second;
        if (c->is_active()) {
            return c;
        }
    }

    if (c == nullptr) {
        c = new RdsConnection(m_logger, m_loop);
    }
    if (c == nullptr || !c->connect(host, port)) {
        SAFE_DELETE(c);
        LOG_ERROR("init redis conn failed! host: %s, port: %d.",
                  host.c_str(), port);
        return nullptr;
    }
    m_conns[identity] = c;
    return c;
}

bool RedisMgr::send_to(const char* node, const std::vector<std::string>& rds_cmds,
                       redisCallbackFn* fn, void* privdata) {
    if (node == nullptr || fn == nullptr || rds_cmds.size() == 0) {
        LOG_ERROR("invalid params!");
        return false;
    }

    /* async hiredis client pressure 10w+ qps. it's ok just one link. */
    RdsConnection* c = get_conn(node);
    if (c == nullptr) {
        LOG_ERROR("get redis conn failed! node: %s", node);
        return false;
    }
    return c->send_to(rds_cmds, fn, privdata);
}

}  // namespace kim