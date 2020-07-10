#include "cmd.h"

namespace kim {

Cmd::Cmd(Request* req, uint64_t id) : m_id(id), m_req(req) {
}

Cmd::~Cmd() {
}

};  // namespace kim