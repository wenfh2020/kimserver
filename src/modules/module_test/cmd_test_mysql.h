#ifndef __CMD_TEST_MYSQL_H__
#define __CMD_TEST_MYSQL_H__

#include "cmd.h"
#include "db/mysql_result.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class CmdTestMysql : public Cmd {
   public:
    enum E_STEP {
        ES_PARSE_REQUEST = 0,
        ES_DATABASE_INSERT,
        ES_DATABASE_INSERT_CALLBACK,
        ES_DATABASE_QUERY,
        ES_DATABASE_QUERY_CALLBACK,
    };

    CmdTestMysql(Log* logger, INet* net,
                 uint64_t mid, uint64_t id, const std::string& name = "")
        : Cmd(logger, net, mid, id, name) {
    }

   protected:
    Cmd::STATUS execute_steps(int err, void* data) {
        switch (get_exec_step()) {
            case ES_PARSE_REQUEST: {
                const HttpMsg* msg = m_req->get_http_msg();
                if (msg == nullptr) {
                    response_http(ERR_FAILED, "invalid request!");
                    return Cmd::STATUS::ERROR;
                }

                LOG_DEBUG("cmd test database, http path: %s, data: %s",
                          msg->path().c_str(), msg->body().c_str());

                CJsonObject req_data(msg->body());
                m_key = req_data("id");
                m_value = req_data("value");
                m_oper = req_data("oper");
                if (m_key.empty() || m_value.empty()) {
                    LOG_ERROR("invalid request data! pls check!");
                    return response_http(ERR_FAILED, "invalid request data");
                }

                if (m_oper == "read") {
                    return execute_cur_step(ES_DATABASE_QUERY);
                }
                return execute_next_step(err, data);
            }
            case ES_DATABASE_INSERT: {
                LOG_DEBUG("step: database insert, value: %s", m_value.c_str());
                snprintf(m_sql, sizeof(m_sql), "insert into mytest.test_async_mysql (value) values ('%s');", m_value.c_str());
                Cmd::STATUS status = db_exec("test", m_sql);
                if (status == Cmd::STATUS::ERROR) {
                    response_http(ERR_FAILED, "insert data failed!");
                    return status;
                }
                set_next_step();
                return status;
            }
            case ES_DATABASE_INSERT_CALLBACK: {
                LOG_DEBUG("step: database insert callback!");
                if (err != ERR_OK) {
                    LOG_ERROR("database inert callback failed! error: %d");
                    return response_http(ERR_FAILED, "database insert data failed!");
                }

                if (m_oper == "write") {
                    return response_http(ERR_OK, "database write data done!");
                }

                return execute_next_step(err, data);
            }
            case ES_DATABASE_QUERY: {
                LOG_DEBUG("step: database query, id: %s.", m_key.c_str());
                snprintf(m_sql, sizeof(m_sql), "select value from mytest.test_async_mysql where id = %s;", m_key.c_str());
                Cmd::STATUS status = db_query("test", m_sql);
                if (status == Cmd::STATUS::ERROR) {
                    response_http(ERR_FAILED, "query data failed!");
                    return status;
                }
                set_next_step();
                return status;
            }
            case ES_DATABASE_QUERY_CALLBACK: {
                LOG_DEBUG("step: database query callback!");
                if (err != ERR_OK) {
                    LOG_ERROR("database query callback failed! error: %d");
                    return response_http(ERR_FAILED, "database insert data failed!");
                }
                MysqlResult* res = static_cast<MysqlResult*>(data);
                if (res != nullptr) {
                    vec_row_t query_data;
                    int size = res->get_result_data(query_data);
                    if (size != 0) {
                        for (size_t i = 0; i < query_data.size(); i++) {
                            map_row_t& items = query_data[i];
                            for (const auto& it : items) {
                                LOG_DEBUG("col: %s, data: %s", it.first.c_str(), it.second.c_str());
                            }
                        }
                    }
                }
                return response_http(ERR_OK, "database query data done!");
            }
            default: {
                LOG_ERROR("invalid step");
                return response_http(ERR_FAILED, "invalid step!");
            }
        }
    }

   private:
    char m_sql[256];
    std::string m_key, m_value, m_oper;
};

}  // namespace kim

#endif  // __CMD_TEST_MYSQL_H__