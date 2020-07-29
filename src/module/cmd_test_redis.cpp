#include "cmd_test_redis.h"

#include "util/json/CJsonObject.hpp"
#include "util/util.h"

namespace kim {

enum E_STEP {
    E_STEP_PARSE_REQUEST = 0,
    E_STEP_REDIS_SET,
    E_STEP_REDIS_SET_CALLBACK,
    E_STEP_REDIS_GET,
    E_STEP_REDIS_GET_CALLBACK,
};

CmdTestRedis::CmdTestRedis(Log* logger, ICallback* net, uint64_t mid, uint64_t cid)
    : Cmd(logger, net, mid, cid) {
}

CmdTestRedis::~CmdTestRedis() {
    LOG_DEBUG("delete cmd hello");
}

Cmd::STATUS CmdTestRedis::on_callback(int err, void* data) {
    return execute(err, data);
}

Cmd::STATUS CmdTestRedis::on_timeout() {
    LOG_DEBUG("time out!");
    return Cmd::STATUS::COMPLETED;
}

Cmd::STATUS CmdTestRedis::execute(std::shared_ptr<Request> req) {
    return execute(ERR_OK, nullptr);
}

Cmd::STATUS CmdTestRedis::execute(int err, void* data) {
    int port = 6379;
    std::string host("127.0.0.1");

    switch (get_exec_step()) {
        case E_STEP_PARSE_REQUEST: {
            const HttpMsg* msg = m_req->get_http_msg();
            if (msg == nullptr) {
                return Cmd::STATUS::ERROR;
            }

            LOG_DEBUG("cmd hello, http path: %s, data: %s",
                      msg->path().c_str(), msg->body().c_str());

            CJsonObject req_data(msg->body());
            if (!req_data.Get("key", m_key) ||
                !req_data.Get("value", m_value)) {
                LOG_ERROR("invalid request data! pls check!");
                return response_http(ERR_FAILED, "invalid request data");
            }
            return execute_next_step(err, data);
        }
        case E_STEP_REDIS_SET: {
            LOG_DEBUG("step redis set, key: %s, value: %s", m_key.c_str(), m_value.c_str());
            std::string cmd_str = format_str("set %s %s", m_key.c_str(), m_value.c_str());
            Cmd::STATUS status = redis_send_to(host, port, cmd_str);
            if (status == Cmd::STATUS::ERROR) {
                return response_http(ERR_FAILED, "redis failed!");
            }
            return status;
        }
        case E_STEP_REDIS_SET_CALLBACK: {
            redisReply* reply = (redisReply*)data;
            if (err != ERR_OK || reply == nullptr ||
                reply->type != REDIS_REPLY_STATUS || strncmp(reply->str, "OK", 2) != 0) {
                LOG_ERROR("redis set data callback failed!");
                return response_http(ERR_FAILED, "redis set data failed!");
            }
            LOG_DEBUG("redis set callback result: %s", reply->str);
            return execute_next_step(err, data);
        }
        case E_STEP_REDIS_GET: {
            std::string cmd_str = format_str("get %s", m_key.c_str());
            Cmd::STATUS status = redis_send_to(host, port, cmd_str);
            if (status == Cmd::STATUS::ERROR) {
                return response_http(ERR_FAILED, "redis failed!");
            }
            return status;
        }
        case E_STEP_REDIS_GET_CALLBACK: {
            redisReply* reply = (redisReply*)data;
            if (err != ERR_OK || reply == nullptr || reply->type != REDIS_REPLY_STRING) {
                LOG_ERROR("redis get data callback failed!");
                return response_http(ERR_FAILED, "redis set data failed!");
            }
            LOG_DEBUG("redis get callback result: %s, type: %d", reply->str, reply->type);
            CJsonObject data;
            data.Add("key", m_key);
            data.Add("value", m_value);
            return response_http(ERR_OK, "success", data);
        }
        default: {
            return response_http(ERR_FAILED, "invalid step!");
        }
    }

    return Cmd::STATUS::COMPLETED;
}

}  // namespace kim