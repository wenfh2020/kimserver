#include "redis_context.h"

namespace kim {

RdsConnection::RdsConnection(const std::string& host, int port, redisAsyncContext* c)
    : m_host(host), m_port(port), m_conn(c) {
}

RdsConnection::~RdsConnection() {
}

}  // namespace kim