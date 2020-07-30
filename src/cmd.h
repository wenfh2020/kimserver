#ifndef __CMD_H__
#define __CMD_H__

#include "../util/json/CJsonObject.hpp"
#include "callback.h"
#include "error.h"
#include "request.h"

#define MAX_TIMER_CNT 3

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
    virtual Cmd::STATUS on_timeout();
    virtual Cmd::STATUS on_callback(int err, void* data);
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req);

   public:
    virtual Cmd::STATUS response_http(_cstr& data, int status_code = 200);
    virtual Cmd::STATUS response_http(int err, _cstr& errstr, const CJsonObject& data, int status_code = 200);
    virtual Cmd::STATUS response_http(int err, _cstr& errstr, int status_code = 200);
    virtual Cmd::STATUS redis_send_to(_cstr& host, int port, _csvector& rds_cmds);

    uint64_t get_id() { return m_id; }
    void set_cmd_name(_cstr& name) { m_cmd_name = name; }
    _cstr& get_cmd_name() const { return m_cmd_name; }
    int set_max_timeout_cnt() { return m_max_timeout_cnt; }

    void set_req(std::shared_ptr<Request> req) { m_req = req; }
    std::shared_ptr<Request> get_req() const { return m_req; }

    ev_timer* get_timer() { return m_timer; }
    void set_timer(ev_timer* w) { m_timer = w; }

    // async step. -- status machine.
    void set_exec_step(int step) { m_step = step; }
    int get_exec_step() { return m_step; }
    void set_next_step() { m_step++; }
    Cmd::STATUS execute_next_step(int err, void* data);

   protected:
    virtual Cmd::STATUS execute_steps(int err, void* data);

   protected:
    uint64_t m_id = 0;
    uint64_t m_module_id = 0;
    Log* m_logger = nullptr;
    ICallback* m_callback = nullptr;

    std::shared_ptr<Request> m_req = nullptr;
    std::string m_cmd_name;

    ev_timer* m_timer = nullptr;
    int m_step = 0;  // async step.
    int m_cur_timeout_cnt = 0;
    int m_max_timeout_cnt = MAX_TIMER_CNT;
};

};  // namespace kim

#endif  //__CMD_H__
