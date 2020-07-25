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

    virtual Cmd::STATUS time_out() { return Cmd::STATUS::OK; }
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req) { return Cmd::STATUS::OK; }
    virtual Cmd::STATUS call_back(int err, void* data) { return Cmd::STATUS::OK; }
    virtual Cmd::STATUS response_http(const std::string& data, int status_code = 200);

    uint64_t get_id() { return m_id; }

    void set_req(std::shared_ptr<Request> req) { m_req = req; }
    std::shared_ptr<Request> get_req() { return m_req; }

    void set_net(INet* net);
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
    std::shared_ptr<Request> m_req = nullptr;
    std::string m_cmd_name;
    Log* m_logger = nullptr;
    INet* m_net = nullptr;

    int m_errno = 0;
    std::string m_error;
};

};  // namespace kim

#endif  //__CMD_H__
