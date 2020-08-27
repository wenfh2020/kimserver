#include "mysql_async_conn.h"

#include <errmsg.h>

#define MAX_DISCONNECT_TIME 20
#define SET_ERR_INFO(x, e, s)                \
    if (x != nullptr) {                      \
        x->error = e;                        \
        x->errstr = (s) != nullptr ? s : ""; \
    }

namespace kim {

MysqlAsyncConn::MysqlAsyncConn(Log* logger) : m_logger(logger) {
    memset(&m_timer, 0, sizeof(m_timer));
    memset(&m_watcher, 0, sizeof(m_watcher));
}

MysqlAsyncConn::~MysqlAsyncConn() {
    clear_tasks();
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

void MysqlAsyncConn::set_connected(bool ok) {
    if (ok) {
        m_is_connected = true;
        m_reconnect_cnt = 0;
    } else {
        m_is_connected = false;
        stop_task();
        set_state(STATE::NO_CONNECTED);
    }
}

bool MysqlAsyncConn::init(const db_info_t* db_info, struct ev_loop* loop) {
    if (!set_db_info(db_info) || loop == nullptr) {
        LOG_ERROR("invalid db info or ev loop!");
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
    load_timer();
    return true;
}

bool MysqlAsyncConn::load_timer() {
    if (m_loop == nullptr) {
        return false;
    }
    ev_timer_init(&m_timer, libev_timer_cb, 1.0, 1.0);
    ev_timer_start(m_loop, &m_timer);
    m_timer.data = this;
    return true;
}

void MysqlAsyncConn::libev_timer_cb(struct ev_loop* loop, ev_timer* w, int events) {
    MysqlAsyncConn* c = static_cast<MysqlAsyncConn*>(w->data);
    c->on_timer_callback(loop, w, events);
}

void MysqlAsyncConn::on_timer_callback(struct ev_loop* loop, ev_timer* w, int events) {
    ping_start();
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

void MysqlAsyncConn::wait_for_mysql(struct ev_loop* loop, ev_io* w, int event) {
    switch (m_state) {
        case STATE::CONNECT_WAITING:
            connect_wait(loop, w, event);
            break;
        case STATE::WAIT_OPERATE:
            operate_wait();
            break;
        case STATE::QUERY_WAITING:
            query_wait(loop, w, event);
            break;
        case STATE::EXECSQL_WAITING:
            exec_sql_wait(loop, w, event);
            break;
        case STATE::STORE_WAITING:
            store_result_wait(loop, w, event);
            break;
        case STATE::PING_WAITING:
            ping_wait(loop, w, event);
            break;
        default:
            LOG_ERROR("invalid state: %d", m_state);
            break;
    }
}

void MysqlAsyncConn::connect_start() {
    MYSQL* ret = nullptr;
    int status = mysql_real_connect_start(
        &ret, &m_mysql, m_db_info->host.c_str(), m_db_info->user.c_str(),
        m_db_info->password.c_str(), m_db_info->db_name.c_str(), m_db_info->port, NULL, 0);

    if (status != 0) {
        LOG_DEBUG("mysql connecting! host: %s, port: %d, user: %s",
                  m_db_info->host.c_str(), m_db_info->port, m_db_info->user.c_str());
        set_state(STATE::CONNECT_WAITING);
        active_ev_io(status);
        return;
    }

    int error = mysql_errno(&m_mysql);
    const char* errstr = mysql_error(&m_mysql);
    if (ret != nullptr) {
        mysql_set_character_set(&m_mysql, m_db_info->charset.c_str()); /* mysql 5.0 lib */
        set_state(STATE::WAIT_OPERATE);
        LOG_INFO("mysql connecting! host: %s, port: %d, user: %s",
                 m_db_info->host.c_str(), m_db_info->port, m_db_info->user.c_str());
    } else {
        set_connected(false);
        LOG_ERROR("connect failed! error: %d, errstr: %s, host: %s, port: %d.",
                  error, errstr, m_db_info->host.c_str(), m_db_info->port);
    }

    LOG_DEBUG("ret: %p, status: %d, error: %d, errstr: %s", ret, status, error, errstr);
}

void MysqlAsyncConn::connect_wait(struct ev_loop* loop, ev_io* watcher, int event) {
    MYSQL* ret;
    int error, status;

    status = libev_event_to_mysql_status(event);
    status = mysql_real_connect_cont(&ret, &m_mysql, status);
    error = mysql_errno(&m_mysql);
    const char* errstr = mysql_error(&m_mysql);
    // LOG_DEBUG("ret: %p, status: %d, error: %d, errstr: %s", ret, status, error, errstr);
    if (status == 0) {
        set_connected(ret != nullptr);
        if (ret != nullptr) {
            LOG_INFO("connect done! %s:%d, error: %d, errstr: %s",
                     m_db_info->host.c_str(), m_db_info->port, error, errstr);
            mysql_set_character_set(&m_mysql, m_db_info->charset.c_str()); /* mysql 5.0 lib */
            set_state(STATE::WAIT_OPERATE);
            wait_next_task();
        } else {
            LOG_ERROR("connect failed! error: %d, errstr: %s, host: %s, port: %d.",
                      error, errstr, m_db_info->host.c_str(), m_db_info->port);
            set_connected(false);
        }
    }
}

void MysqlAsyncConn::query_start() {
    if (m_cur_task == nullptr) {
        return;
    }
    LOG_DEBUG("start query sql: %s", m_cur_task->sql.c_str());

    int ret = 0;
    int status = mysql_real_query_start(
        &ret, &m_mysql, m_cur_task->sql.c_str(), m_cur_task->sql.size());

    if (status != 0) {
        /* LT mode. it will callback again! */
        LOG_DEBUG("query done fsdfs, sql: %s", m_cur_task->sql.c_str());
        set_state(STATE::QUERY_WAITING);
        active_ev_io(status);
        return;
    }

    if (ret == 0) {
        store_result_start();
    } else {
        handle_task();
    }
}

void MysqlAsyncConn::query_wait(struct ev_loop* loop, ev_io* w, int event) {
    int ret, status;
    status = libev_event_to_mysql_status(event);
    status = mysql_real_query_cont(&ret, &m_mysql, status);

    if (status != 0) {
        /* LT mode. it will callback again! */
        set_state(STATE::QUERY_WAITING);
        return;
    }

    if (ret != 0) {
        handle_task();
    } else {
        store_result_start();
    }
}

void MysqlAsyncConn::store_result_start() {
    int status = mysql_store_result_start(&m_query_res, &m_mysql);
    if (status != 0) {
        set_state(STATE::STORE_WAITING);
        active_ev_io(status);
        return;
    }
    handle_task();
}

void MysqlAsyncConn::store_result_wait(struct ev_loop* loop, ev_io* watcher, int event) {
    if (m_cur_task == nullptr) {
        return;
    }
    int status = libev_event_to_mysql_status(event);
    status = mysql_store_result_cont(&m_query_res, &m_mysql, status);
    if (status != 0) {
        /* LT mode. it will callback again! */
        set_state(STATE::STORE_WAITING);
        return;
    }
    handle_task();
}

void MysqlAsyncConn::operate_wait() {
    LOG_DEBUG("wait_for_mysql state: WAIT_OPERATE");
    SAFE_DELETE(m_cur_task);
    m_cur_task = fetch_next_task();
    if (m_cur_task != nullptr) {
        if (m_cur_task->oper == sql_task_t::OPERATE::SELECT) {
            query_start();
        } else {
            exec_sql_start();
        }
    } else {
        stop_task();
    }
}

void MysqlAsyncConn::exec_sql_start() {
    if (m_cur_task == nullptr) {
        return;
    }
    LOG_DEBUG("exec sql: %s", m_cur_task->sql.c_str());

    int ret = 0;
    int status = mysql_real_query_start(
        &ret, &m_mysql, m_cur_task->sql.c_str(), m_cur_task->sql.size());
    if (status != 0) {
        // continue to watch the events.
        set_state(STATE::EXECSQL_WAITING);
        active_ev_io(status);
        return;
    }

    if (ret == 0) {
        LOG_DEBUG("exec_sql done, sql: %s", m_cur_task->sql.c_str());
    }
    handle_task();
}

void MysqlAsyncConn::exec_sql_wait(struct ev_loop* loop, ev_io* watcher, int event) {
    if (m_cur_task == nullptr) {
        return;
    }

    int ret = 0;
    int status = libev_event_to_mysql_status(event);
    status = mysql_real_query_cont(&ret, &m_mysql, status);
    if (status != 0) {
        /* LT mode. it will callback again! */
        set_state(STATE::EXECSQL_WAITING);
        return;
    }

    if (ret == 0) {
        LOG_DEBUG("exec sql done! sql: %s", m_cur_task->sql.c_str());
    }
    handle_task();
}

void MysqlAsyncConn::ping_start() {
    if (m_is_connected) {
        return;
    }
    LOG_DEBUG("ping start: %d, %d...", m_is_connected, m_tasks.size());

    int ret, status;
    status = mysql_ping_start(&ret, &m_mysql);
    if (status != 0) {
        set_state(STATE::PING_WAITING);
        active_ev_io(status);
        return;
    }

    if (ret == 0) {
        set_connected(true);
    } else {
        int error = mysql_errno(&m_mysql);
        const char* errstr = mysql_error(&m_mysql);
        LOG_INFO("ping falied! error: %d, errstr: %s", error, errstr);
        handle_task();
    }
}

void MysqlAsyncConn::ping_wait(struct ev_loop* loop, ev_io* w, int event) {
    int ret = 0;
    int status = libev_event_to_mysql_status(event);
    status = mysql_ping_cont(&ret, &m_mysql, status);
    if (status != 0) {
        /* LT mode. it will callback again! */
        set_state(STATE::PING_WAITING);
        return;
    }

    if (ret == 0) {
        if (!m_is_connected) {
            set_connected(true);
            LOG_INFO("reconnect done! %s:%d", m_db_info->host.c_str(), m_db_info->port);
            handle_task();
        }
    } else {
        set_connected(false);
        LOG_INFO("ping failed! %s:%d, error: %d, errstr: %s",
                 m_db_info->host.c_str(), m_db_info->port,
                 mysql_errno(&m_mysql), mysql_error(&m_mysql));
    }
}

void MysqlAsyncConn::stop_task() {
    if (m_loop == nullptr) {
        return;
    }
    if (ev_is_active(&m_watcher)) {
        LOG_DEBUG("no task, stop event!");
        ev_io_stop(m_loop, &m_watcher);
    }
}

void MysqlAsyncConn::clear_tasks() {
    stop_task();
    if (m_cur_task != nullptr && m_cur_task->error == 0) {
        SET_ERR_INFO(m_cur_task, -1, "terminated!");
    }
    callback(m_cur_task);
    SAFE_DELETE(m_cur_task);
    for (auto& it : m_tasks) {
        SET_ERR_INFO(it, -1, "terminated!");
        callback(it);
        SAFE_DELETE(it);
    }
    m_tasks.clear();
}

bool MysqlAsyncConn::add_task(sql_task_t* task) {
    if (!m_is_connected || task == nullptr) {
        return false;
    }
    LOG_DEBUG("add sql task, sql: %s", task->sql.c_str());
    m_tasks.push_back(task);
    wait_next_task();
    return true;
}

bool MysqlAsyncConn::wait_next_task(int mysql_status) {
    if (task_size() > 0) {
        active_ev_io(mysql_status);
        return true;
    }
    return false;
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

void MysqlAsyncConn::handle_task() {
    int error = mysql_errno(&m_mysql);
    const char* errstr = mysql_error(&m_mysql);
    SET_ERR_INFO(m_cur_task, error, errstr);
    callback(m_cur_task);
    if (error == CR_SERVER_LOST || error == CR_SERVER_GONE_ERROR) {
        set_connected(false);
        LOG_ERROR("reconnect error: %d, %s", error, errstr);
        if (m_reconnect_cnt++ == MAX_DISCONNECT_TIME) {
            clear_tasks();
        } else {
            connect_start();
        }
    } else {
        if (m_is_connected) {
            set_state(STATE::WAIT_OPERATE);
            wait_next_task();
        }
    }
}

void MysqlAsyncConn::callback(sql_task_t* task) {
    if (task == nullptr) {
        return;
    }

    if (task->fn_query != nullptr) {
        m_mysql_result.init(&m_mysql, m_query_res);
        task->fn_query(this, task, &m_mysql_result);
        if (m_query_res != nullptr) {
            mysql_free_result(m_query_res);
            m_query_res = nullptr;
        }
        task->fn_query = nullptr;
    }

    if (task->fn_exec != nullptr) {
        task->fn_exec(this, task);
        task->fn_exec = nullptr;
    }
}

}  // namespace kim