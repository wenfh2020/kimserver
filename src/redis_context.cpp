#include "redis_context.h"

#include "util/util.h"

namespace kim {

RdsConnection::RdsConnection(_cstr& host, int port, redisAsyncContext* c)
    : m_host(host), m_port(port), m_conn(c) {
    m_identity = format_addr(host, port);
}

RdsConnection::~RdsConnection() {
}

}  // namespace kim