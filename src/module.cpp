#include "module.h"

namespace kim {

Module::~Module() {
    for (const auto& it : m_cmds) {
        delete it;
    }
}

bool Module::init(Log* logger) {
    m_logger = logger;
    return true;
}

}  // namespace kim