#ifndef __CMD_TEST_REDIS_H__
#define __CMD_TEST_REDIS_H__

#include "../cmd.h"
#include "../util/json/CJsonObject.hpp"

namespace kim {

class CmdTestRedis : public Cmd {
   public:
    enum E_STEP {
        ES_PARSE_REQUEST = 0,
        ES_REDIS_SET,
        ES_REDIS_SET_CALLBACK,
        ES_REDIS_GET,
        ES_REDIS_GET_CALLBACK,
    };

    CmdTestRedis(Log* logger, INet* net, uint64_t mid, uint64_t id, _cstr& name = "")
        : Cmd(logger, net, mid, id, name) {
    }

    bool init() {
        m_redis_host = config()["redis"]["test"]("host");
        m_redis_port = atoi(config()["redis"]["test"]("port").c_str());
        LOG_DEBUG("redis host: %s, port: %d", m_redis_host.c_str(), m_redis_port);
        return true;
    }

   protected:
    Cmd::STATUS execute_steps(int err, void* data) {
        switch (get_exec_step()) {
            case ES_PARSE_REQUEST: {
                const HttpMsg* msg = m_req->get_http_msg();
                if (msg == nullptr) {
                    return Cmd::STATUS::ERROR;
                }

                LOG_DEBUG("cmd test redis, http path: %s, data: %s",
                          msg->path().c_str(), msg->body().c_str());

                CJsonObject req_data(msg->body());
                if (!req_data.Get("key", m_key) ||
                    !req_data.Get("value", m_value)) {
                    LOG_ERROR("invalid request data! pls check!");
                    return response_http(ERR_FAILED, "invalid request data");
                }
                return execute_next_step(err, data);
            }
            case ES_REDIS_SET: {
                LOG_DEBUG("step redis set, key: %s, value: %s", m_key.c_str(), m_value.c_str());
                std::vector<std::string> rds_cmds{"set", m_key, m_value};
                Cmd::STATUS status = redis_send_to(m_redis_host, m_redis_port, rds_cmds);
                if (status == Cmd::STATUS::ERROR) {
                    return response_http(ERR_FAILED, "redis failed!");
                }
                return status;
            }
            case ES_REDIS_SET_CALLBACK: {
                redisReply* reply = (redisReply*)data;
                if (err != ERR_OK || reply == nullptr ||
                    reply->type != REDIS_REPLY_STATUS || strncmp(reply->str, "OK", 2) != 0) {
                    LOG_ERROR("redis set data callback failed!");
                    return response_http(ERR_FAILED, "redis set data callback failed!");
                }
                LOG_DEBUG("redis set callback result: %s", reply->str);
                return execute_next_step(err, data);
            }
            case ES_REDIS_GET: {
                std::vector<std::string> rds_cmds{"get", m_key};
                Cmd::STATUS status = redis_send_to(m_redis_host, m_redis_port, rds_cmds);
                if (status == Cmd::STATUS::ERROR) {
                    return response_http(ERR_FAILED, "redis failed!");
                }
                return status;
            }
            case ES_REDIS_GET_CALLBACK: {
                redisReply* reply = (redisReply*)data;
                if (err != ERR_OK || reply == nullptr || reply->type != REDIS_REPLY_STRING) {
                    LOG_ERROR("redis get data callback failed!");
                    return response_http(ERR_FAILED, "redis set data failed!");
                }
                LOG_DEBUG("redis get callback result: %s, type: %d", reply->str, reply->type);
                CJsonObject rsp_data;
                rsp_data.Add("key", m_key);
                rsp_data.Add("value", m_value);
                return response_http(ERR_OK, "success", rsp_data);
            }
            default: {
                LOG_ERROR("invalid step");
                return response_http(ERR_FAILED, "invalid step!");
            }
        }
    }

   private:
    std::string m_key, m_value;
    std::string m_redis_host;
    int m_redis_port;
};

}  // namespace kim

#endif  //__CMD_TEST_REDIS_H__