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
    virtual ~Module();
    virtual void register_handle_func() {}

    virtual Cmd::STATUS process_message(std::shared_ptr<Request> req) {
        return Cmd::STATUS::UNKOWN;
    }

    void set_id(uint64_t id) { m_id = id; }
    uint64_t get_id() { return m_id; }

    bool init(Log* logger, INet* net);
    void set_version(int ver) { m_version = ver; }
    void set_name(const std::string& name) { m_name = name; }
    std::string get_name() { return m_name; }
    void set_file_path(const std::string& path) { m_file_path = path; }

    Cmd::STATUS response_http(std::shared_ptr<Connection> c,
                              const std::string& data, int status_code = 200);

   protected:
    uint64_t m_id;
    Log* m_logger = nullptr;
    INet* m_net = nullptr;
    int m_version = 1;
    std::string m_name;
    std::string m_file_path;
    std::unordered_map<uint64_t, Cmd*> m_cmds;
};

#define REGISTER_HANDLER(class_name)                                           \
   public:                                                                     \
    typedef Cmd::STATUS (class_name::*cmd_func)(std::shared_ptr<Request> req); \
    virtual Cmd::STATUS process_message(std::shared_ptr<Request> req) {        \
        const HttpMsg* msg = req->get_http_msg();                              \
        if (msg == nullptr) {                                                  \
            return Cmd::STATUS::ERROR;                                         \
        }                                                                      \
        auto it = m_cmd_funcs.find(msg->path());                               \
        if (it == m_cmd_funcs.end()) {                                         \
            return Cmd::STATUS::UNKOWN;                                        \
        }                                                                      \
        return (this->*(it->second))(req);                                     \
    }                                                                          \
                                                                               \
   protected:                                                                  \
    std::unordered_map<std::string, cmd_func> m_cmd_funcs;

#define REGISTER_HANDLE_FUNC(path, func) \
    m_cmd_funcs[path] = &func;

#define HANDLE_CMD(_cmd)                                   \
    const HttpMsg* msg = req->get_http_msg();              \
    if (msg == nullptr) {                                  \
        return Cmd::STATUS::ERROR;                         \
    }                                                      \
    std::string path = msg->path();                        \
    _cmd* p = new _cmd(m_logger, m_net, m_net->get_seq()); \
    p->set_req(req);                                       \
    p->set_cmd_name(#_cmd);                                \
    Cmd::STATUS status = p->execute(req);                  \
    if (status == Cmd::STATUS::RUNNING) {                  \
        auto it = m_cmds.insert({m_net->get_seq(), p});    \
        if (!it.second) {                                  \
            LOG_ERROR("cmd duplicate in m_cmds!");         \
            delete p;                                      \
            return Cmd::STATUS::ERROR;                     \
        }                                                  \
        return status;                                     \
    }                                                      \
    delete p;                                              \
    return status;

}  // namespace kim

#endif  //__MODULE_H__