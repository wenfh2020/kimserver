#ifndef __CMD_TEST_REDIS_H__
#define __CMD_TEST_REDIS_H__

#include "../cmd.h"

namespace kim {

class CmdTestRedis : public Cmd {
   public:
    CmdTestRedis(Log* logger, INet* net, uint64_t mid, uint64_t id);
    virtual ~CmdTestRedis() { LOG_DEBUG("delete cmd test redis.") }

   protected:
    virtual Cmd::STATUS execute_steps(int err, void* data = nullptr) override;

   private:
    std::string m_key, m_value;
};

}  // namespace kim

#endif  //__CMD_TEST_REDIS_H__