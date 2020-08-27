#ifndef __KIM_MYSQL_TASK_H__
#define __KIM_MYSQL_TASK_H__

#include <mysql.h>

#include <iostream>

#include "mysql_result.h"

namespace kim {

/* mysql async task. */
class MysqlAsyncConn;
typedef struct sql_task_s sql_task_t;

/* mysql callback fn. */
typedef void(MysqlExecCallbackFn)(const MysqlAsyncConn*, sql_task_t* task);
typedef void(MysqlQueryCallbackFn)(const MysqlAsyncConn*, sql_task_t* task, MysqlResult* res);

typedef struct sql_task_s {
    enum class OPERATE {
        SELECT,
        EXEC,
    };
    std::string sql;
    OPERATE oper = OPERATE::SELECT;
    MysqlExecCallbackFn* fn_exec = nullptr;
    MysqlQueryCallbackFn* fn_query = nullptr;
    void* privdata = nullptr;
    int error = 0;
    std::string errstr;
} sql_task_t;

/* database link info. */
typedef struct db_info_s {
    int port = 0;
    int max_conn_cnt = 0;
    std::string host, db_name, password, charset, user;
} db_info_t;

}  // namespace kim

#endif  //__KIM_MYSQL_TASK_H__