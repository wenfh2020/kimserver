#ifndef __KIM_CMD_H__
#define __KIM_CMD_H__

#include "base.h"
#include "error.h"
#include "net.h"
#include "request.h"
#include "timer.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class Cmd : public Timer, public Base {
   public:
    enum class STATUS {
        UNKOWN = 0,
        OK = 1,
        RUNNING = 2,
        COMPLETED = 3,
        ERROR = 4,
    };
    Cmd(Log* logger, INet* net, uint64_t mid, uint64_t id, _cstr& name = "");
    virtual ~Cmd();

   public:
    virtual bool init() { return true; }
    virtual Cmd::STATUS on_timeout();
    virtual Cmd::STATUS on_callback(int err, void* data);
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req);

   public:
    virtual Cmd::STATUS response_http(_cstr& data, int status_code = 200);
    virtual Cmd::STATUS response_http(int err, _cstr& errstr, const CJsonObject& data, int status_code = 200);
    virtual Cmd::STATUS response_http(int err, _cstr& errstr, int status_code = 200);
    virtual Cmd::STATUS redis_send_to(_cstr& host, int port, _csvector& rds_cmds);

    uint64_t get_module_id() { return m_module_id; }
    CJsonObject& config() { return m_net->get_config(); }

    void set_req(std::shared_ptr<Request> req) { m_req = req; }
    std::shared_ptr<Request> get_req() const { return m_req; }

    // async step. -- status machine.
    void set_exec_step(int step) { m_step = step; }
    int get_exec_step() { return m_step; }
    void set_next_step() { m_step++; }
    Cmd::STATUS execute_next_step(int err, void* data);

   protected:
    virtual Cmd::STATUS execute_steps(int err, void* data);

   protected:
    uint64_t m_module_id = 0;
    std::shared_ptr<Request> m_req = nullptr;
    int m_step = 0;  // async step.
};

};  // namespace kim

#endif  //__KIM_CMD_H__
