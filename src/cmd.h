#ifndef __CMD_H__
#define __CMD_H__

#include "../util/json/CJsonObject.hpp"
#include "callback.h"
#include "error.h"
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
    Cmd(Log* logger, ICallback* cb, uint64_t mid, uint64_t cid);
    Cmd(const Cmd&) = delete;
    Cmd& operator=(const Cmd&) = delete;
    virtual ~Cmd();

   public:
    virtual Cmd::STATUS on_timeout() { return Cmd::STATUS::COMPLETED; }
    virtual Cmd::STATUS on_callback(int err, void* data) { return Cmd::STATUS::ERROR; }
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req) { return Cmd::STATUS::ERROR; }
    virtual Cmd::STATUS response_http(const std::string& data, int status_code = 200);
    virtual Cmd::STATUS response_http(int err, const std::string& errstr,
                                      const CJsonObject& data, int status_code = 200);
    virtual Cmd::STATUS response_http(int err, const std::string& errstr,
                                      int status_code = 200);
    virtual Cmd::STATUS redis_send_to(const std::string& host, int port, const std::string& data);

    uint64_t get_id() { return m_id; }

    void set_req(std::shared_ptr<Request> req) { m_req = req; }
    std::shared_ptr<Request> get_req() const { return m_req; }

    void set_cmd_name(const std::string& name) { m_cmd_name = name; }
    const std::string& get_cmd_name() const { return m_cmd_name; }

    ev_timer* get_timer() { return m_timer; }
    void set_timer(ev_timer* w) { m_timer = w; }

    // async step.
    void set_exec_step(int step) { m_step = step; }
    int get_exec_step() { return m_step; }
    void set_next_step() { m_step++; }
    Cmd::STATUS execute_next_step(int err, void* data);

   protected:
    virtual Cmd::STATUS execute(int err, void* data);

   protected:
    uint64_t m_id = 0;
    uint64_t m_module_id = 0;
    Log* m_logger = nullptr;
    ICallback* m_callback = nullptr;

    std::shared_ptr<Request> m_req = nullptr;
    std::string m_cmd_name;
    int m_errno = 0;
    std::string m_error;

    ev_timer* m_timer = nullptr;
    int m_step = 0;  // async step.
};

};  // namespace kim

#endif  //__CMD_H__
