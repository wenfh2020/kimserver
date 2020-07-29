#ifndef __CMD_TEST_REDIS_H__
#define __CMD_TEST_REDIS_H__

#include "../cmd.h"

namespace kim {

class CmdTestRedis : public Cmd {
   public:
    CmdTestRedis(Log* logger, ICallback* net, uint64_t mid, uint64_t id);
    virtual ~CmdTestRedis();

   public:
    virtual Cmd::STATUS on_timeout() override;
    virtual Cmd::STATUS on_callback(int err, void* data) override;
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req) override;

   protected:
    virtual Cmd::STATUS execute(int err, void* data = nullptr) override;

   private:
    std::string m_key, m_value;
};

}  // namespace kim

#endif  //__CMD_TEST_REDIS_H__