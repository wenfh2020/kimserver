#ifndef __MODULE_H__
#define __MODULE_H__

#include "cmd.h"
#include "net.h"
#include "request.h"

namespace kim {

typedef struct module_data_s {
    int m_version = 1;
    std::string m_path;
} module_data_t;

class Module {
   public:
    Module() {}
    Module(Log* logger) : m_logger(logger) {}
    virtual ~Module();

    virtual Cmd::STATUS process_message(std::shared_ptr<Request> req) { return Cmd::STATUS::UNKOWN; }

    bool init(Log* logger, INet* net);
    void set_version(int ver) { m_version = ver; }
    void set_name(const std::string& name) { m_name = name; }
    std::string get_name() { return m_name; }
    void set_file_path(const std::string& path) { m_file_path = path; }

   protected:
    int m_version = 1;
    std::string m_name;
    std::string m_file_path;
    std::set<Cmd*> m_cmds;
    Log* m_logger = nullptr;
    INet* m_net = nullptr;
};

#define BEGIN_HTTP_MAP()                                                \
   public:                                                              \
    virtual Cmd::STATUS process_message(std::shared_ptr<Request> req) { \
        const HttpMsg* msg = req->get_http_msg();                       \
        if (msg == nullptr) {                                           \
            return Cmd::STATUS::ERROR;                                  \
        }                                                               \
        std::string path = msg->path();

#define HTTP_HANDLER(_path, _cmd, _name)        \
    if (path == (_path)) {                      \
        _cmd* p = new _cmd;                     \
        p->init(m_logger, m_net);               \
        p->set_cmd_name(_name);                 \
        p->set_req(req);                        \
        Cmd::STATUS status = p->call_back(req); \
        if (status == Cmd::STATUS::RUNNING) {   \
            auto it = m_cmds.insert(p);         \
            if (!it.second) {                   \
                delete p;                       \
                return Cmd::STATUS::ERROR;      \
            }                                   \
            return status;                      \
        }                                       \
        delete p;                               \
        return status;                          \
    }

#define END_HTTP_MAP()          \
    return Cmd::STATUS::UNKOWN; \
    }

#define END_HTTP_MAP_EX(base)          \
    return base::process_message(req); \
    }

}  // namespace kim

#endif  //__MODULE_H__