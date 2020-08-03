#include "redis_context.h"

#include "util/util.h"

namespace kim {

RdsConnection::RdsConnection(Log* logger, INet* net, _cstr& host, int port, redisAsyncContext* c)
    : m_logger(logger), m_net(net), m_host(host), m_port(port), m_conn(c) {
    m_identity = format_addr(host, port);
}

RdsConnection::~RdsConnection() {
    clear_wait_cmd_infos();
}

void RdsConnection::clear_wait_cmd_infos() {
    for (auto& it : m_wait_cmds) {
        SAFE_DELETE(it.second);
    }
    m_wait_cmds.clear();
}

wait_cmd_info_t* RdsConnection::add_wait_cmd_info(uint64_t module_id, uint64_t cmd_id) {
    LOG_DEBUG("add cmd info, module id: %llu, cmd id: %d", module_id, cmd_id)
    wait_cmd_info_t* info = find_wait_cmd_info(cmd_id);
    if (info != nullptr) {
        return info;
    }

    info = new wait_cmd_info_t(m_net, module_id, cmd_id);
    if (info == nullptr) {
        LOG_ERROR("alloc wait_cmd_info_t failed!");
        return nullptr;
    }
    m_wait_cmds[cmd_id] = info;
    return info;
}

bool RdsConnection::del_wait_cmd_info(uint64_t cmd_id) {
    auto it = m_wait_cmds.find(cmd_id);
    if (it == m_wait_cmds.end()) {
        return false;
    }
    SAFE_DELETE(it->second);
    m_wait_cmds.erase(it);
    return true;
}

wait_cmd_info_t* RdsConnection::find_wait_cmd_info(uint64_t cmd_id) {
    wait_cmd_info_t* info = nullptr;
    auto it = m_wait_cmds.find(cmd_id);
    if (it != m_wait_cmds.end()) {
        info = it->second;
    }
    return info;
}

}  // namespace kim