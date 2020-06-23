#include "context.h"

namespace kim {

Connection::Connection(int fd, uint64_t id)
    : m_fd(fd), m_id(id), m_private_data(NULL) {
}

}  // namespace kim