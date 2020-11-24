#ifndef __CMD_TEST_MYSQL_H__
#define __CMD_TEST_MYSQL_H__

#include "cmd.h"
#include "db/mysql_result.h"
#include "session_test.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class CmdTestMysql : public Cmd {
   public:
    enum E_STEP {
        STEP_PARSE_REQUEST = 0,
        STEP_DATABASE_INSERT,
        STEP_DATABASE_INSERT_CALLBACK,
        STEP_DATABASE_QUERY,
        STEP_DATABASE_QUERY_CALLBACK,
    };

    CmdTestMysql(Log* logger, INet* net, uint64_t id, const std::string& name = "")
        : Cmd(logger, net, id, name) {
    }

   private:
    char m_sql[256];
    std::string m_key, m_value, m_oper;
    bool m_is_session = false;

   protected:
    Cmd::STATUS execute_steps(int err, void* data) {
        switch (get_exec_step()) {
            case STEP_PARSE_REQUEST: {
                const HttpMsg* msg = m_req->http_msg();
                if (msg == nullptr) {
                    response_http(ERR_FAILED, "invalid request!");
                    return Cmd::STATUS::ERROR;
                }

                LOG_DEBUG("cmd test db, http path: %s, data: %s",
                          msg->path().c_str(), msg->body().c_str());

                CJsonObject req(msg->body());
                m_key = req("id");
                m_oper = req("oper");
                m_value = req("value");
                m_is_session = (req("session") == "true");

                if (m_key.empty() || m_value.empty()) {
                    LOG_ERROR("invalid request data! pls check!");
                    response_http(ERR_FAILED, "invalid request data");
                    return Cmd::STATUS::ERROR;
                }

                return (m_oper == "read")
                           ? execute_next_step(err, data, STEP_DATABASE_QUERY)
                           : execute_next_step(err, data);
            }

            case STEP_DATABASE_INSERT: {
                LOG_DEBUG("step: db insert, value: %s", m_value.c_str());
                snprintf(m_sql, sizeof(m_sql),
                         "insert into mytest.test_async_mysql (value) values ('%s');",
                         m_value.c_str());

                Cmd::STATUS status = db_exec("test", m_sql);
                if (status == Cmd::STATUS::ERROR) {
                    response_http(ERR_FAILED, "insert data failed!");
                    return status;
                }

                set_next_step();
                return status;
            }

            case STEP_DATABASE_INSERT_CALLBACK: {
                LOG_DEBUG("step: db insert callback!");
                if (err != ERR_OK) {
                    LOG_ERROR("db inert callback failed! eror: %d");
                    response_http(ERR_FAILED, "db insert data failed!");
                    return Cmd::STATUS::ERROR;
                }

                if (m_oper == "write") {
                    if (!response_http(ERR_OK, "db write data done!")) {
                        return Cmd::STATUS::ERROR;
                    }
                    return Cmd::STATUS::COMPLETED;
                } else {
                    return execute_next_step(err, data);
                }
            }

            case STEP_DATABASE_QUERY: {
                LOG_DEBUG("step: db query, id: %s.", m_key.c_str());
                if (m_is_session) {
                    // query from session.
                    SessionTest* s = dynamic_cast<SessionTest*>(net()->get_session(m_key));
                    if (s != nullptr) {
                        LOG_DEBUG("query data from session ok, id: %s, value: %s",
                                  m_key.c_str(), s->value().c_str())
                        response_http(ERR_OK, "query data from session ok!");
                        return Cmd::STATUS::COMPLETED;
                    }
                }

                snprintf(m_sql, sizeof(m_sql),
                         "select value from mytest.test_async_mysql where id = %s;",
                         m_key.c_str());

                Cmd::STATUS status = db_query("test", m_sql);
                if (status == Cmd::STATUS::ERROR) {
                    response_http(ERR_FAILED, "query data failed!");
                    return status;
                }

                set_next_step();
                return status;
            }

            case STEP_DATABASE_QUERY_CALLBACK: {
                LOG_DEBUG("step: db query callback!");
                if (err != ERR_OK) {
                    LOG_ERROR("db query callback failed! error: %d");
                    response_http(ERR_FAILED, "db insert data failed!");
                    return Cmd::STATUS::ERROR;
                }

                int size;
                MysqlResult* res;
                vec_row_t query_data;
                SessionTest* session = nullptr;

                res = static_cast<MysqlResult*>(data);
                if (res == nullptr || (size = res->result_data(query_data)) == 0) {
                    LOG_ERROR("query no data!");
                    response_http(ERR_FAILED, "query no data in db!");
                    return Cmd::STATUS::ERROR;
                }

                if (m_is_session) {
                    session = get_alloc_session<SessionTest>(m_key);
                }

                // col - value.
                for (size_t i = 0; i < query_data.size(); i++) {
                    map_row_t& items = query_data[i];
                    for (const auto& it : items) {
                        if (session != nullptr) {
                            // restore data in session.
                            session->set_value(it.second);
                        }
                        LOG_DEBUG("col: %s, data: %s", it.first.c_str(), it.second.c_str());
                    }
                }
                response_http(ERR_OK, "db query done!");
                return Cmd::STATUS::COMPLETED;
            }

            default: {
                LOG_ERROR("invalid step");
                response_http(ERR_FAILED, "invalid step!");
                return Cmd::STATUS::ERROR;
            }
        }
    }
};

}  // namespace kim

#endif  // __CMD_TEST_MYSQL_H__