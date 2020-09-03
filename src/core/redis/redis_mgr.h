#ifndef __KIM_REDIS_MGR_H__
#define __KIM_REDIS_MGR_H__

#include "redis_context.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class RedisMgr {
   public:
    RedisMgr(Log* logger, struct ev_loop* loop);
    virtual ~RedisMgr();

   public:
    bool init(CJsonObject& config);
    bool send_to(const char* node, const std::vector<std::string>& rds_cmds,
                 redisCallbackFn* fn, void* privdata);
    void close(const char* node);

   private:
    RdsConnection* get_conn(const char* node);

   private:
    Log* m_logger = nullptr;
    struct ev_loop* m_loop = nullptr;
    std::unordered_map<std::string, RdsConnection*> m_conns;  // redis connections.
    std::unordered_map<std::string, std::pair<std::string, int> > m_addrs;
};

}  // namespace kim

#endif  // __KIM_REDIS_MGR_H__