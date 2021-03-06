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
        OK,
        RUNNING,
        COMPLETED,
        ERROR,
    };

    Cmd(Log* logger, INet* net, uint64_t id, const std::string& name);
    virtual ~Cmd();

    CJsonObject& config() { return m_net->config(); }

    void set_req(const Request& req);
    const Request* req() const { return m_req; }
    Request* req() { return m_req; }

    /* async step. -- status machine. */
    void set_cur_step(int step) { m_step = step; }
    int get_cur_step() { return m_step; }
    void set_next_step(int step = -1) { m_step = (step != -1) ? step : (m_step + 1); }
    Cmd::STATUS execute_next_step(int err, void* data, int step = -1);

   public:
    virtual bool init() { return true; }
    virtual Cmd::STATUS on_timeout();
    virtual Cmd::STATUS on_callback(int err, void* data);
    virtual Cmd::STATUS execute(const Request& req);

   public:
    virtual bool response_http(const std::string& data, int status_code = 200);
    virtual bool response_http(int err, const std::string& errstr, const CJsonObject& data, int status_code = 200);
    virtual bool response_http(int err, const std::string& errstr, int status_code = 200);
    virtual bool response_tcp(int err, const std::string& errstr, const std::string& data = "");

    virtual Cmd::STATUS redis_send_to(const char* node, const std::vector<std::string>& argv);
    virtual Cmd::STATUS db_exec(const char* node, const char* sql);
    virtual Cmd::STATUS db_query(const char* node, const char* sql);

   protected:
    virtual Cmd::STATUS execute_steps(int err, void* data);

   protected:
    int m_step = 0;  // async step.
    Request* m_req = nullptr;
};

}  // namespace kim

#endif  //__KIM_CMD_H__
