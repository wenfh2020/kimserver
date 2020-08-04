#ifndef __MODULE_H__
#define __MODULE_H__

#include "base.h"
#include "cmd.h"
#include "error.h"
#include "request.h"

namespace kim {

class Module : public Base {
   public:
    Module() {}
    Module(Log* logger, INet* net, uint64_t id, _cstr& name);
    virtual ~Module();
    virtual void register_handle_func() {}

    virtual Cmd::STATUS process_message(std::shared_ptr<Request> req) {
        return Cmd::STATUS::UNKOWN;
    }

    bool init(Log* logger, INet* net, uint64_t id);
    void set_version(int ver) { m_version = ver; }
    void set_file_path(_cstr& path) { m_file_path = path; }
    Cmd::STATUS execute_cmd(Cmd* cmd, std::shared_ptr<Request> req);
    Cmd::STATUS on_timeout(Cmd* cmd);
    Cmd::STATUS on_callback(wait_cmd_info_t* index, int err, void* data);
    Cmd::STATUS response_http(std::shared_ptr<Connection> c, _cstr& data, int status_code = 200);

    bool del_cmd(Cmd* cmd);

   protected:
    int m_version = 1;
    std::string m_file_path;
    std::unordered_map<uint64_t, Cmd*> m_cmds;
};

#define REGISTER_HANDLER(class_name)                                           \
   public:                                                                     \
    class_name(Log* logger, INet* net, uint64_t id, _cstr& name = "")          \
        : Module(logger, net, id, name) {                                      \
    }                                                                          \
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

#define REGISTER_FUNC(path, func) \
    m_cmd_funcs[path] = &func;

#define HANDLE_CMD(_cmd)                                                        \
    const HttpMsg* msg = req->get_http_msg();                                   \
    if (msg == nullptr) {                                                       \
        return Cmd::STATUS::ERROR;                                              \
    }                                                                           \
    std::string path = msg->path();                                             \
    _cmd* p = new _cmd(m_logger, m_net, get_id(), m_net->get_new_seq(), #_cmd); \
    p->set_req(req);                                                            \
    Cmd::STATUS status = execute_cmd(p, req);                                   \
    if (status != Cmd::STATUS::RUNNING) {                                       \
        SAFE_DELETE(p);                                                         \
    }                                                                           \
    return status;

}  // namespace kim

#endif  //__MODULE_H__