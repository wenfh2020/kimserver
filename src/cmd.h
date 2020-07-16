#ifndef __CMD_H__
#define __CMD_H__

#include "net.h"
#include "request.h"

namespace kim {

class Cmd {
   public:
    enum class STATUS {
        UNKOWN = 0,
        OK = 1,
        RUNNING = 2,
        COMPLETED = 3,
        ERROR = 4,
    };

    Cmd() {}
    Cmd(const Cmd&) = delete;
    Cmd& operator=(const Cmd&) = delete;
    virtual ~Cmd();

    void init(Log* logger, INet* net);
    // void init(Request* req, uint64_t id, const std::string& name);

    virtual Cmd::STATUS time_out() { return Cmd::STATUS::OK; }
    virtual Cmd::STATUS call_back(Request* req) { return Cmd::STATUS::OK; }

    // virtual Cmd::STATUS send_to(Request* req) { return Cmd::STATUS::OK; }

    uint64_t get_id() { return m_id; }
    Request* get_req() { return m_req; }

    void set_net(INet* net) { m_net = net; }
    void set_logger(Log* logger) { m_logger = logger; }
    void set_cmd_name(const std::string& name) { m_cmd_name = name; }
    std::string get_cmd_name() { return m_cmd_name; }

    void set_errno(int err) { m_errno = err; }
    void set_error(const std::string& error) { m_error = error; }
    void set_error_info(int err, const std::string& error) {
        m_errno = err;
        m_error = error;
    }

   protected:
    uint64_t m_id = 0;
    Request* m_req = nullptr;
    std::string m_cmd_name;
    Log* m_logger = nullptr;
    INet* m_net = nullptr;

    int m_errno = 0;
    std::string m_error;
};

};  // namespace kim

#endif  //__CMD_H__
