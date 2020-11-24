#ifndef __CMD_TEST_REDIS_H__
#define __CMD_TEST_REDIS_H__

#include "cmd.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class CmdTestRedis : public Cmd {
   public:
    enum E_STEP {
        STEP_PARSE_REQUEST = 0,
        STEP_REDIS_SET,
        STEP_REDIS_SET_CALLBACK,
        STEP_REDIS_GET,
        STEP_REDIS_GET_CALLBACK,
    };

    CmdTestRedis(Log* logger, INet* net, uint64_t id, const std::string& name = "")
        : Cmd(logger, net, id, name) {
    }

   protected:
    Cmd::STATUS execute_steps(int err, void* data) {
        switch (get_exec_step()) {
            case STEP_PARSE_REQUEST: {
                const HttpMsg* msg = m_req->http_msg();
                if (msg == nullptr) {
                    return Cmd::STATUS::ERROR;
                }

                LOG_DEBUG("cmd test redis, http path: %s, data: %s",
                          msg->path().c_str(), msg->body().c_str());

                CJsonObject req(msg->body());
                m_key = req("key");
                m_oper = req("oper");
                m_value = req("value");

                if (m_key.empty() || m_value.empty()) {
                    LOG_ERROR("invalid request data! pls check!");
                    response_http(ERR_FAILED, "invalid request data");
                    return Cmd::STATUS::ERROR;
                }

                return (m_oper == "read")
                           ? execute_next_step(err, data, STEP_REDIS_GET)
                           : execute_next_step(err, data);
            }

            case STEP_REDIS_SET: {
                LOG_DEBUG("step redis set, key: %s, value: %s", m_key.c_str(), m_value.c_str());
                std::vector<std::string> argv{"set", m_key, m_value};
                Cmd::STATUS status = redis_send_to("test", argv);
                if (status == Cmd::STATUS::ERROR) {
                    response_http(ERR_FAILED, "redis failed!");
                    return Cmd::STATUS::ERROR;
                }
                set_next_step();
                return status;
            }

            case STEP_REDIS_SET_CALLBACK: {
                redisReply* reply = (redisReply*)data;
                if (err != ERR_OK || reply == nullptr ||
                    reply->type != REDIS_REPLY_STATUS || strncmp(reply->str, "OK", 2) != 0) {
                    LOG_ERROR("redis set data callback failed!");
                    response_http(ERR_FAILED, "redis set data callback failed!");
                    return Cmd::STATUS::ERROR;
                }
                LOG_DEBUG("redis set callback result: %s", reply->str);

                if (m_oper == "write") {
                    if (!response_http(ERR_OK, "redis write data done!")) {
                        return Cmd::STATUS::ERROR;
                    }
                    return Cmd::STATUS::COMPLETED;
                } else {
                    return execute_next_step(err, data);
                }
            }

            case STEP_REDIS_GET: {
                std::vector<std::string> argv{"get", m_key};
                Cmd::STATUS status = redis_send_to("test", argv);
                if (status == Cmd::STATUS::ERROR) {
                    response_http(ERR_FAILED, "redis failed!");
                    return Cmd::STATUS::ERROR;
                }
                set_next_step();
                return status;
            }

            case STEP_REDIS_GET_CALLBACK: {
                redisReply* reply = (redisReply*)data;
                if (err != ERR_OK || reply == nullptr || reply->type != REDIS_REPLY_STRING) {
                    LOG_ERROR("redis get data callback failed!");
                    response_http(ERR_FAILED, "redis set data failed!");
                    return Cmd::STATUS::ERROR;
                }
                LOG_DEBUG("redis get callback result: %s, type: %d", reply->str, reply->type);

                CJsonObject rsp;
                rsp.Add("key", m_key);
                rsp.Add("value", m_value);
                if (!response_http(ERR_OK, "ok", rsp)) {
                    return Cmd::STATUS::ERROR;
                }
                return Cmd::STATUS::COMPLETED;
            }

            default: {
                LOG_ERROR("invalid step");
                response_http(ERR_FAILED, "invalid step!");
                return Cmd::STATUS::ERROR;
            }
        }
    }

   private:
    std::string m_key, m_value, m_oper;
};

}  // namespace kim

#endif  //__CMD_TEST_REDIS_H__