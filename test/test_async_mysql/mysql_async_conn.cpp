#include "mysql_async_conn.h"

#include <errmsg.h>

namespace kim {

MysqlAsyncConn::MysqlAsyncConn(Log* logger) : m_logger(logger) {
    memset(&m_watcher, 0, sizeof(m_watcher));
}

MysqlAsyncConn::~MysqlAsyncConn() {
    for (auto& it : m_tasks) {
        SAFE_DELETE(it);
    }
    m_tasks.clear();
    SAFE_DELETE(m_cur_task);
    SAFE_DELETE(m_db_info);
}

bool MysqlAsyncConn::set_db_info(const db_info_t* db_info) {
    if (db_info == nullptr) {
        LOG_ERROR("invalid db info!");
        return false;
    }
    SAFE_DELETE(m_db_info);
    m_db_info = new db_info_t;
    m_db_info->port = db_info->port;
    m_db_info->max_conn_cnt = db_info->max_conn_cnt;
    m_db_info->host = db_info->host;
    m_db_info->db_name = db_info->db_name;
    m_db_info->password = db_info->password;
    m_db_info->charset = db_info->charset;
    m_db_info->user = db_info->user;
    return true;
}

bool MysqlAsyncConn::init(const db_info_t* db_info, struct ev_loop* loop) {
    if (!set_db_info(db_info)) {
        LOG_ERROR("invalid db info!");
        return false;
    }

    bool reconnect = true;
    unsigned int timeout = 3;
    mysql_init(&m_mysql);
    mysql_options(&m_mysql, MYSQL_OPT_NONBLOCK, 0);  // set async.
    mysql_options(&m_mysql, MYSQL_OPT_CONNECT_TIMEOUT, reinterpret_cast<char*>(&timeout));
    mysql_options(&m_mysql, MYSQL_OPT_COMPRESS, NULL);
    mysql_options(&m_mysql, MYSQL_OPT_LOCAL_INFILE, NULL);
    mysql_options(&m_mysql, MYSQL_OPT_RECONNECT, reinterpret_cast<char*>(&reconnect));

    m_loop = loop;
    connect_start();
    return true;
}

int MysqlAsyncConn::mysql_status_to_libev_event(int status) {
    int wait_event = 0;
    if (status & MYSQL_WAIT_READ) {
        wait_event |= EV_READ;
    }
    if (status & MYSQL_WAIT_WRITE) {
        wait_event |= EV_WRITE;
    }
    return wait_event;
}

int MysqlAsyncConn::libev_event_to_mysql_status(int event) {
    int status = 0;
    if (event & EV_READ) {
        status |= MYSQL_WAIT_READ;
    }
    if (event & EV_WRITE) {
        status |= MYSQL_WAIT_WRITE;
    }
    return status;
}

void MysqlAsyncConn::libev_io_cb(struct ev_loop* loop, ev_io* w, int event) {
    MysqlAsyncConn* c = static_cast<MysqlAsyncConn*>(w->data);
    c->wait_for_mysql(loop, w, event);
}

bool MysqlAsyncConn::wait_for_mysql(struct ev_loop* loop, ev_io* w, int event) {
    if (m_state == STATE::CONNECT_WAITING) {
        connect_wait(loop, w, event);
    } else if (m_state == STATE::EXECSQL_WAITING) {
        exec_sql_wait(loop, w, event);
    } else if (m_state == STATE::QUERY_WAITING) {
        query_wait(loop, w, event);
    } else if (m_state == STATE::STORE_WAITING) {
        LOG_DEBUG("wait_for_mysql state: STORE_WAITING");
        store_result_wait(loop, w, event);
    } else if (m_state == STATE::WAIT_OPERATE) {
        LOG_DEBUG("wait_for_mysql state: WAIT_OPERATE");
        SAFE_DELETE(m_cur_task);
        m_cur_task = fetch_next_task();
        if (m_cur_task != nullptr) {
            if (m_cur_task->oper == sql_task_t::OPERATE::SELECT) {
                LOG_DEBUG("query sql: %s", m_cur_task->sql.c_str());
                query_start();
            } else {
                exec_sql_start();
            }
        } else {
            LOG_DEBUG("stop task!");
            stop_task();
        }
    } else {
        LOG_ERROR("invalid state: %d", m_state);
        return false;
    }

    return true;
}

bool MysqlAsyncConn::connect_start() {
    MYSQL* ret = nullptr;
    int status = mysql_real_connect_start(
        &ret, &m_mysql, m_db_info->host.c_str(), m_db_info->user.c_str(),
        m_db_info->password.c_str(), m_db_info->db_name.c_str(), m_db_info->port, NULL, 0);
    if (status != 0) {
        LOG_DEBUG("mysql connecting! host: %s, port: %d, user: %s",
                  m_db_info->host.c_str(), m_db_info->port, m_db_info->user.c_str());
        set_state(STATE::CONNECT_WAITING);
        active_ev_io(status);
    } else {
        mysql_set_character_set(&m_mysql, m_db_info->charset.c_str()); /* mysql 5.0 lib */
        set_state(STATE::WAIT_OPERATE);
        LOG_INFO("mysql connectied! host: %s, port: %d, user: %s",
                 m_db_info->host.c_str(), m_db_info->port, m_db_info->user.c_str());
    }
    return true;
}

bool MysqlAsyncConn::connect_wait(struct ev_loop* loop, ev_io* watcher, int event) {
    MYSQL* ret;
    int status = libev_event_to_mysql_status(event);
    status = mysql_real_connect_cont(&ret, &m_mysql, status);
    if (status == 0) {
        LOG_INFO("connect done! %s:%d", m_db_info->host.c_str(), m_db_info->port);
        mysql_set_character_set(&m_mysql, m_db_info->charset.c_str()); /* mysql 5.0 lib */
        set_state(STATE::WAIT_OPERATE);
        wait_next_task();
        return true;
    }

    /* LT mode. it will callback again! */
    int error = mysql_errno(&m_mysql);
    const char* errstr = mysql_error(&m_mysql);
    if (error == 0) {
        // LOG_WARN("connect failed but no error! host: %s, port: %d.",
        //          m_db_info->host.c_str(), m_db_info->port);
        return true;
    }

    LOG_ERROR("connect failed! error: %d, errstr: %s, host: %s, port: %d.",
              error, errstr, m_db_info->host.c_str(), m_db_info->port);

    if (m_cur_task == nullptr) {
        m_cur_task = fetch_next_task();
    }
    if (m_cur_task != nullptr) {
        m_cur_task->error = error;
        m_cur_task->errstr = errstr;
        if (check_error_reconnect(m_cur_task)) {
            LOG_INFO("reconnect! host: %s, port: %d.", m_db_info->host.c_str(), m_db_info->port);
        } else {
            if (m_cur_task->fn_exec != nullptr) {
                m_cur_task->fn_exec(this, m_cur_task);
            }
            SAFE_DELETE(m_cur_task);
            wait_next_task();
        }
    }
    return false;
}

bool MysqlAsyncConn::check_error_reconnect(const sql_task_t* task) {
    if (task->error == CR_SERVER_GONE_ERROR ||
        task->error == CR_SERVER_LOST) {
        LOG_ERROR("reconnect error: %d, %s", task->error, task->errstr.c_str());
        connect_start();
        return true;
    }
    return false;
}

bool MysqlAsyncConn::query_start() {
    if (m_cur_task == nullptr) {
        return false;
    }

    int ret = 0;
    int status = mysql_real_query_start(
        &ret, &m_mysql, m_cur_task->sql.c_str(), m_cur_task->sql.size());
    if (ret != 0) {
        m_cur_task->error = mysql_errno(&m_mysql);
        m_cur_task->errstr = mysql_error(&m_mysql);
        if (check_error_reconnect(m_cur_task)) {
            LOG_ERROR("sql reconnect! host: %s, port: %d", m_db_info->host.c_str(), m_db_info->port);
            return false;
        }
        if (m_cur_task->fn_query != nullptr) {
            m_cur_task->fn_query(this, m_cur_task, m_query_res);
        }
        set_state(STATE::WAIT_OPERATE);
        wait_next_task();
        return true;
    }

    if (status != 0) {
        set_state(STATE::QUERY_WAITING);
        active_ev_io(status);
    } else {
        LOG_DEBUG("query done, sql: %s", m_cur_task->sql.c_str());
        store_result_start();
    }

    return true;
}

bool MysqlAsyncConn::query_wait(struct ev_loop* loop, ev_io* w, int event) {
    if (m_cur_task == nullptr) {
        return false;
    }

    int ret = 0;
    int status = libev_event_to_mysql_status(event);
    status = mysql_real_query_cont(&ret, &m_mysql, status);
    if (ret != 0) {
        m_cur_task->error = mysql_errno(&m_mysql);
        m_cur_task->errstr = mysql_error(&m_mysql);
        if (check_error_reconnect(m_cur_task)) {
            LOG_ERROR("sql reconnect! host: %s, port: %d", m_db_info->host.c_str(), m_db_info->port);
            return false;
        }
        if (m_cur_task->fn_query) {
            m_cur_task->fn_query(this, m_cur_task, m_query_res);
        }
        set_state(STATE::WAIT_OPERATE);
        wait_next_task();
        return false;
    }

    if (status != 0) {
        /* LT mode. it will callback again! */
        // LOG_DEBUG("wait_for_mysql state: QUERY_WAITING");
        set_state(STATE::QUERY_WAITING);
    } else {
        LOG_DEBUG("query done, sql: %s", m_cur_task->sql.c_str());
        store_result_start();
    }

    return true;
}

int MysqlAsyncConn::store_result_start() {
    if (m_cur_task == nullptr) {
        return 0;
    }
    int status = mysql_store_result_start(&m_query_res, &m_mysql);
    if (status != 0) {
        set_state(STATE::STORE_WAITING);
        active_ev_io(status);
    } else {
        LOG_DEBUG("store_result done.");
        if (m_cur_task->fn_query != nullptr) {
            m_cur_task->fn_query(this, m_cur_task, m_query_res);
        }
        set_state(STATE::WAIT_OPERATE);
        wait_next_task();
    }
    return 0;
}

bool MysqlAsyncConn::store_result_wait(struct ev_loop* loop, ev_io* watcher, int event) {
    if (m_cur_task == nullptr) {
        return false;
    }
    int status = libev_event_to_mysql_status(event);
    status = mysql_store_result_cont(&m_query_res, &m_mysql, status);
    if (status != 0) {
        /* LT mode. it will callback again! */
        set_state(STATE::STORE_WAITING);
    } else {
        if (m_cur_task && m_cur_task->fn_query) {
            m_cur_task->fn_query(this, m_cur_task, m_query_res);
        }
        set_state(STATE::WAIT_OPERATE);
        wait_next_task();
    }
    return true;
}

int MysqlAsyncConn::exec_sql_start() {
    if (m_cur_task == nullptr) {
        return 0;
    }
    LOG_DEBUG("exec sql: %s", m_cur_task->sql.c_str());

    int ret = 0;
    int status = mysql_real_query_start(
        &ret, &m_mysql, m_cur_task->sql.c_str(), m_cur_task->sql.size());
    if (ret != 0) {
        m_cur_task->error = mysql_errno(&m_mysql);
        m_cur_task->errstr = mysql_error(&m_mysql);
        if (check_error_reconnect(m_cur_task)) {
            LOG_ERROR("sql reconnect! host: %s, port: %d", m_db_info->host.c_str(), m_db_info->port);
            return 0;
        }
        if (m_cur_task->fn_exec != nullptr) {
            m_cur_task->fn_exec(this, m_cur_task);
        }
        set_state(STATE::WAIT_OPERATE);
        wait_next_task();
        return 1;
    }

    if (status != 0) {
        set_state(STATE::EXECSQL_WAITING);
        active_ev_io(status);
    } else {
        LOG_DEBUG("exec_sql done, sql: %s", m_cur_task->sql.c_str());
        if (m_cur_task->fn_exec != nullptr) {
            m_cur_task->fn_exec(this, m_cur_task);
        }
        set_state(STATE::WAIT_OPERATE);
        wait_next_task();
    }

    return 0;
}

bool MysqlAsyncConn::exec_sql_wait(struct ev_loop* loop, ev_io* watcher, int event) {
    if (m_cur_task == nullptr) {
        return false;
    }

    int ret = 0;
    int status = libev_event_to_mysql_status(event);
    status = mysql_real_query_cont(&ret, &m_mysql, status);
    if (ret != 0) {
        m_cur_task->error = mysql_errno(&m_mysql);
        m_cur_task->errstr = mysql_error(&m_mysql);
        if (check_error_reconnect(m_cur_task)) {
            LOG_INFO("reconnect! host: %s, port: %d.",
                     m_db_info->host.c_str(), m_db_info->port);
        } else {
            LOG_DEBUG("exec sql failed! sql: %s",
                      m_cur_task->error, m_cur_task->errstr.c_str());
            if (m_cur_task->fn_exec != nullptr) {
                m_cur_task->fn_exec(this, m_cur_task);
            }
            set_state(STATE::WAIT_OPERATE);
            wait_next_task();
        }
        return false;
    }

    if (status != 0) {
        /* LT mode. it will callback again! */
        set_state(STATE::EXECSQL_WAITING);
        // LOG_DEBUG("wait_for_mysql state: EXECSQL_WAITING");
    } else {
        LOG_DEBUG("exec sql done! sql: %s", m_cur_task->sql.c_str());
        if (m_cur_task->fn_exec != nullptr) {
            m_cur_task->fn_exec(this, m_cur_task);
        }
        set_state(STATE::WAIT_OPERATE);
        wait_next_task();
    }

    return true;
}

void MysqlAsyncConn::stop_task() {
    if (m_loop != nullptr) {
        ev_io_stop(m_loop, &m_watcher);
    }
}

void MysqlAsyncConn::add_task(sql_task_t* task) {
    if (task != nullptr) {
        LOG_DEBUG("add sql task, sql: %s", task->sql.c_str());
        m_tasks.push_back(task);
        wait_next_task();
    }
}

int MysqlAsyncConn::wait_next_task(int mysql_status) {
    if (task_size() > 0) {
        active_ev_io(mysql_status);
    }
    return 0;
}

sql_task_t* MysqlAsyncConn::fetch_next_task() {
    sql_task_t* task = nullptr;
    if (m_tasks.size() > 0) {
        LOG_DEBUG("task cnt: %d", m_tasks.size());
        task = m_tasks.front();
        m_tasks.pop_front();
    }
    return task;
}

void MysqlAsyncConn::active_ev_io(int mysql_status) {
    int wait_event = mysql_status_to_libev_event(mysql_status);
    if (ev_is_active(&m_watcher)) {
        ev_io_stop(m_loop, &m_watcher);
        ev_io_set(&m_watcher, m_watcher.fd, m_watcher.events | wait_event);
        ev_io_start(m_loop, &m_watcher);
    } else {
        int fd = mysql_get_socket(&m_mysql);
        ev_io_init(&m_watcher, libev_io_cb, fd, wait_event);
        ev_io_start(m_loop, &m_watcher);
    }
    m_watcher.data = this;
}

}  // namespace kim