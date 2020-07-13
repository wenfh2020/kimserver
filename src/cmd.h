#ifndef __CMD_H__
#define __CMD_H__

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

    Cmd();
    Cmd(Request* req, uint64_t id, const std::string& name);
    Cmd(const Cmd&) = delete;
    Cmd& operator=(const Cmd&) = delete;
    virtual ~Cmd();

    void init(Request* req, uint64_t id, const std::string& name);

    virtual Cmd::STATUS time_out() { return Cmd::STATUS::OK; }
    virtual Cmd::STATUS call_back(const Request* req) { return Cmd::STATUS::OK; }

    uint64_t get_id() { return m_id; }
    Request* get_req() { return m_req; }

    void set_cmd_name(const std::string& name) { m_cmd_name = name; }
    std::string get_cmd_name() { return m_cmd_name; }

   private:
    uint64_t m_id = 0;
    Request* m_req = nullptr;
    std::string m_cmd_name;
};

};  // namespace kim

#endif  //__CMD_H__
