#ifndef __KIM_MODULE_H__
#define __KIM_MODULE_H__

#include "base.h"
#include "cmd.h"
#include "error.h"
#include "request.h"
#include "util/so.h"

namespace kim {

/* Module is a container, which is used for cmd's route.*/

class Module : public Base, public So {
   public:
    Module() {}
    Module(Log* logger, INet* net, uint64_t id, const std::string& name);
    virtual ~Module();
    virtual void register_handle_func() {}

    bool init(Log* logger, INet* net, uint64_t id, const std::string& name = "");
    virtual Cmd::STATUS process_req(const Request& req) { return Cmd::STATUS::UNKOWN; }
    Cmd::STATUS execute_cmd(Cmd* cmd, const Request& req);
    Cmd::STATUS response_http(const fd_t& f, const std::string& data, int status_code = 200);
};

#define REGISTER_HANDLER(class_name)                                              \
   public:                                                                        \
    class_name() {}                                                               \
    class_name(Log* logger, INet* net, uint64_t id, const std::string& name = "") \
        : Module(logger, net, id, name) {                                         \
    }                                                                             \
    typedef Cmd::STATUS (class_name::*cmd_func)(const Request& req);              \
    virtual Cmd::STATUS process_req(const Request& req) {                         \
        if (req.is_http()) {                                                      \
            auto it = m_http_cmd_funcs.find(req.http_msg()->path());              \
            if (it == m_http_cmd_funcs.end()) {                                   \
                return Cmd::STATUS::UNKOWN;                                       \
            }                                                                     \
            return (this->*(it->second))(req);                                    \
        } else {                                                                  \
            auto it = m_cmd_funcs.find(req.msg_head()->cmd());                    \
            if (it == m_cmd_funcs.end()) {                                        \
                return Cmd::STATUS::UNKOWN;                                       \
            }                                                                     \
            return (this->*(it->second))(req);                                    \
        }                                                                         \
    }                                                                             \
                                                                                  \
   protected:                                                                     \
    std::unordered_map<std::string, cmd_func> m_http_cmd_funcs;                   \
    std::unordered_map<int, cmd_func> m_cmd_funcs;

#define HANDLE_HTTP_FUNC(path, func) \
    m_http_cmd_funcs[path] = &func;

#define HANDLE_PROTO_FUNC(id, func) \
    m_cmd_funcs[id] = &func;

#define HANDLE_CMD(_cmd)                                              \
    do {                                                              \
        _cmd* p = new _cmd(m_logger, m_net, m_net->new_seq(), #_cmd); \
        p->set_req(req);                                              \
        if (!p->init()) {                                             \
            LOG_ERROR("init cmd failed! %s", p->name());              \
            SAFE_DELETE(p);                                           \
            return Cmd::STATUS::ERROR;                                \
        }                                                             \
        return execute_cmd(p, req);                                   \
    } while (0);

}  // namespace kim

#endif  //__KIM_MODULE_H__