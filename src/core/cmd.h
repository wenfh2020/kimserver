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
    Cmd(Log* logger, INet* n, uint64_t mid, uint64_t id, const std::string& name = "");
    virtual ~Cmd();

   public:
    virtual bool init() { return true; }
    virtual Cmd::STATUS on_timeout();
    virtual Cmd::STATUS on_callback(int err, void* data);
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req);

   public:
    virtual Cmd::STATUS response_http(const std::string& data, int status_code = 200);
    virtual Cmd::STATUS response_http(int err, const std::string& errstr, const CJsonObject& data, int status_code = 200);
    virtual Cmd::STATUS response_http(int err, const std::string& errstr, int status_code = 200);
    virtual Cmd::STATUS redis_send_to(const char* node, const std::vector<std::string>& argv);
    virtual Cmd::STATUS db_exec(const char* node, const char* sql);
    virtual Cmd::STATUS db_query(const char* node, const char* sql);

    uint64_t module_id() { return m_module_id; }
    CJsonObject& config() { return m_net->config(); }

    void set_req(std::shared_ptr<Request> req) { m_req = req; }
    std::shared_ptr<Request> req() const { return m_req; }

    // async step. -- status machine.
    void set_exec_step(int step) { m_step = step; }
    int get_exec_step() { return m_step; }
    void set_next_step(int step = -1) { m_step = (step != -1) ? step : ++m_step; }
    Cmd::STATUS execute_next_step(int err, void* data, int step = -1);

   protected:
    virtual Cmd::STATUS execute_steps(int err, void* data);

   protected:
    int m_step = 0;  // async step.
    uint64_t m_module_id = 0;
    std::shared_ptr<Request> m_req = nullptr;
};

}  // namespace kim

#endif  //__KIM_CMD_H__
