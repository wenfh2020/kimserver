#include "cmd.h"

namespace kim {

Cmd::Cmd(Log* logger) : m_logger(logger) {}

Cmd::~Cmd() {
}

void Cmd::init(Request* req, uint64_t id, const std::string& name) {
    m_req = req;
    m_id = id;
    m_cmd_name = name;
}

};  // namespace kim