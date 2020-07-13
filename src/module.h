#ifndef __MODULE_H__
#define __MODULE_H__

#include "cmd.h"

namespace kim {

class Module {
   public:
    Module() {}
    virtual ~Module() {
        for (const auto& it : m_cmds) {
            delete it;
        }
    }

    virtual Cmd::STATUS process_message(HttpMsg* msg) { return Cmd::STATUS::UNKOWN; }

   protected:
    int m_version = 1;
    std::string m_path;
    std::set<Cmd*> m_cmds;
};

#define BEGIN_HTTP_MAP()                                \
   public:                                              \
    virtual Cmd::STATUS process_message(HttpMsg* msg) { \
        std::string path = msg->path();

#define HTTP_HANDLER(_path, _cmd)      \
    if (path == (_path)) {             \
        _cmd* p = new _cmd;            \
        auto it = m_cmds.insert(p);    \
        if (it.second == false) {      \
            delete p;                  \
            return Cmd::STATUS::ERROR; \
        }                              \
        return p->call_back(msg);      \
    }

#define END_HTTP_MAP()          \
    return Cmd::STATUS::UNKOWN; \
    }

#define END_HTTP_MAP_EX(base)          \
    return base::process_message(msg); \
    }

}  // namespace kim

#endif  //__MODULE_H__