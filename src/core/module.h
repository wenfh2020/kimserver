#ifndef __KIM_MODULE_H__
#define __KIM_MODULE_H__

#include "base.h"
#include "cmd.h"
#include "error.h"
#include "request.h"

namespace kim {

/* Module is a container, which is used for cmd's route.*/

class Module : public Base {
   public:
    Module() {}
    Module(Log* logger, INet* net, uint64_t id, const std::string& name);
    virtual ~Module();
    virtual void register_handle_func() {}

    virtual Cmd::STATUS process_message(std::shared_ptr<Request> req) {
        return Cmd::STATUS::UNKOWN;
    }

    bool init(Log* logger, INet* net, uint64_t id, const std::string& name = "");
    Cmd::STATUS execute_cmd(Cmd* cmd, std::shared_ptr<Request> req);
    Cmd::STATUS response_http(std::shared_ptr<Connection> c, const std::string& data, int status_code = 200);

    // callback.
    Cmd::STATUS on_timeout(Cmd* cmd);
    Cmd::STATUS on_callback(wait_cmd_info_t* index, int err, void* data);

    // so manager.
    void set_so_handle(void* handle) { m_so_handle = handle; }
    void* get_so_handle() { return m_so_handle; }
    void set_so_path(const std::string& path) { m_so_path = path; }
    const std::string& get_so_path() const { return m_so_path; }
    const char* get_so_path() { return m_so_path.c_str(); }

   protected:
    std::string m_so_path;        // module so path.
    void* m_so_handle = nullptr;  // for dlopen ptr.
};

#define REGISTER_HANDLER(class_name)                                              \
   public:                                                                        \
    class_name() {}                                                               \
    class_name(Log* logger, INet* net, uint64_t id, const std::string& name = "") \
        : Module(logger, net, id, name) {                                         \
    }                                                                             \
    typedef Cmd::STATUS (class_name::*cmd_func)(std::shared_ptr<Request> req);    \
    virtual Cmd::STATUS process_message(std::shared_ptr<Request> req) {           \
        if (req->is_http_req()) {                                                 \
            const HttpMsg* msg = req->get_http_msg();                             \
            if (msg == nullptr) {                                                 \
                return Cmd::STATUS::ERROR;                                        \
            }                                                                     \
            auto it = m_http_cmd_funcs.find(msg->path());                         \
            if (it == m_http_cmd_funcs.end()) {                                   \
                return Cmd::STATUS::UNKOWN;                                       \
            }                                                                     \
            return (this->*(it->second))(req);                                    \
        } else {                                                                  \
            const MsgHead* head = req->get_msg_head();                            \
            const MsgBody* body = req->get_msg_body();                            \
            if (head == nullptr || body == nullptr) {                             \
                return Cmd::STATUS::ERROR;                                        \
            }                                                                     \
            auto it = m_cmd_funcs.find(head->cmd());                              \
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

#define REGISTER_HTTP_FUNC(path, func) \
    m_http_cmd_funcs[path] = &func;

#define REGISTER_FUNC(id, func) \
    m_cmd_funcs[id] = &func;

#define HANDLE_CMD(_cmd)                                                            \
    do {                                                                            \
        _cmd* p = new _cmd(m_logger, m_net, get_id(), m_net->get_new_seq(), #_cmd); \
        p->set_req(req);                                                            \
        if (!p->init()) {                                                           \
            LOG_ERROR("init cmd failed! %s", p->get_name());                        \
            SAFE_DELETE(p);                                                         \
            return Cmd::STATUS::ERROR;                                              \
        }                                                                           \
        Cmd::STATUS status = execute_cmd(p, req);                                   \
        if (status != Cmd::STATUS::RUNNING) {                                       \
            SAFE_DELETE(p);                                                         \
        }                                                                           \
        return status;                                                              \
    } while (0);

}  // namespace kim

#endif  //__KIM_MODULE_H__