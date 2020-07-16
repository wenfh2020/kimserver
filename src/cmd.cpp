#include "cmd.h"

namespace kim {

Cmd::~Cmd() {
}

void Cmd::init(Log* logger, INet* net) {
    m_net = net;
    m_logger = logger;
}

// void Cmd::init(Request* req, uint64_t id, const std::string& name) {
//     m_req = req;
//     m_id = id;
//     m_cmd_name = name;
// }

};  // namespace kim