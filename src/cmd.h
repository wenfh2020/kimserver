#ifndef __CMD_H__
#define __CMD_H__

#include "callback.h"
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

    Cmd(Log* logger, ICallback* cb, uint64_t id);
    Cmd(const Cmd&) = delete;
    Cmd& operator=(const Cmd&) = delete;
    virtual ~Cmd() {}

   public:
    virtual Cmd::STATUS on_timeout() { return Cmd::STATUS::COMPLETED; }
    virtual Cmd::STATUS on_call_back(int err, void* data) { return Cmd::STATUS::OK; }
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req) { return Cmd::STATUS::OK; }
    virtual Cmd::STATUS response_http(const std::string& data, int status_code = 200);

    uint64_t get_id() { return m_id; }

    void set_req(std::shared_ptr<Request> req) { m_req = req; }
    std::shared_ptr<Request> get_req() const { return m_req; }

    void set_cmd_name(const std::string& name) { m_cmd_name = name; }
    const std::string& get_cmd_name() const { return m_cmd_name; }

    ev_timer* get_timer() { return m_timer; }
    void set_timer(ev_timer* w) { m_timer = w; }

   protected:
    uint64_t m_id = 0;
    Log* m_logger = nullptr;
    ICallback* m_callback = nullptr;

    std::shared_ptr<Request> m_req = nullptr;
    std::string m_cmd_name;
    int m_errno = 0;
    std::string m_error;

    ev_timer* m_timer = nullptr;
};

};  // namespace kim

#endif  //__CMD_H__
