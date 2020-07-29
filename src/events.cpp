#include "events.h"

#include <ev.h>
#include <hiredis/adapters/libev.h>
#include <hiredis/async.h>
#include <hiredis/hiredis.h>
#include <signal.h>
#include <unistd.h>

#include "context.h"
#include "module.h"
#include "util/util.h"

namespace kim {

Events::Events(Log* logger) : m_logger(logger) {
}

Events::~Events() {
    destory();
}

bool Events::create() {
    m_ev_loop = ev_loop_new(EVFLAG_FORKCHECK | EVFLAG_SIGNALFD);
    if (m_ev_loop == nullptr) {
        LOG_ERROR("new libev loop failed!");
        return false;
    }

    return true;
}

void Events::destory() {
    if (m_ev_loop != nullptr) {
        ev_loop_destroy(m_ev_loop);
        m_ev_loop = NULL;
    }
}

void Events::run() {
    if (m_ev_loop != nullptr) {
        ev_run(m_ev_loop, 0);
    }
}

void Events::end_ev_loop() {
    if (m_ev_loop != nullptr) {
        ev_break(m_ev_loop, EVBREAK_ALL);
    }
}

void Events::create_signal_event(int signum, void* privdata) {
    LOG_DEBUG("create_signal_event, sig: %d", signum);
    if (m_ev_loop == nullptr) {
        return;
    }

    ev_signal* sig = new ev_signal();
    ev_signal_init(sig, on_signal_callback, signum);
    sig->data = privdata;
    ev_signal_start(m_ev_loop, sig);
}

bool Events::setup_signal_events(void* privdata) {
    LOG_DEBUG("setup_signal_events()");
    if (privdata == nullptr) {
        return false;
    }

    int signals[] = {SIGCHLD, SIGILL, SIGBUS, SIGFPE, SIGKILL};
    for (unsigned int i = 0; i < sizeof(signals) / sizeof(int); i++) {
        create_signal_event(signals[i], privdata);
    }
    return true;
}

void Events::on_signal_callback(struct ev_loop* loop, ev_signal* s, int revents) {
    if (s == nullptr || s->data == nullptr) {
        return;
    }

    ICallback* cb = static_cast<ICallback*>(s->data);
    (s->signum == SIGCHLD) ? cb->on_child_terminated(s) : cb->on_terminated(s);
}

ev_io* Events::add_read_event(int fd, ev_io* w, void* privdata) {
    if (w == nullptr) {
        w = (ev_io*)malloc(sizeof(ev_io));
        if (w == nullptr) {
            LOG_ERROR("alloc ev_io failed!");
            return nullptr;
        }
        memset(w, 0, sizeof(ev_io));
    }

    if (ev_is_active(w)) {
        ev_io_stop(m_ev_loop, w);
        ev_io_set(w, fd, w->events | EV_READ);
        ev_io_start(m_ev_loop, w);
    } else {
        ev_io_init(w, on_io_callback, fd, EV_READ);
        ev_io_start(m_ev_loop, w);
    }
    w->data = privdata;

    LOG_DEBUG("add read ev io, fd: %d", fd);
    return w;
}

ev_io* Events::add_write_event(int fd, ev_io* w, void* privdata) {
    if (w == nullptr) {
        w = (ev_io*)malloc(sizeof(ev_io));
        if (w == nullptr) {
            LOG_ERROR("alloc ev_io failed!");
            return nullptr;
        }
        memset(w, 0, sizeof(ev_io));
    }

    if (ev_is_active(w)) {
        ev_io_stop(m_ev_loop, w);
        ev_io_set(w, w->fd, w->events | EV_WRITE);
        ev_io_start(m_ev_loop, w);
    } else {
        ev_io_init(w, on_io_callback, fd, EV_WRITE);
        ev_io_start(m_ev_loop, w);
    }
    LOG_DEBUG("add write ev io, fd: %d", fd);
    return w;
}

bool Events::del_write_event(ev_io* w) {
    if (w == nullptr) {
        return false;
    }
    if (w->events | EV_WRITE) {
        ev_io_stop(m_ev_loop, w);
        ev_io_set(w, w->fd, w->events & (~EV_WRITE));
        ev_io_start(m_ev_loop, w);
    }
    return true;
}

ev_timer* Events::add_io_timer(double secs, ev_timer* w, void* privdata) {
    return add_timer_event(secs, w, on_io_timer_callback, privdata);
}

ev_timer* Events::add_repeat_timer(double secs, ev_timer* w, void* privdata) {
    return add_timer_event(secs, w, on_repeat_timer_callback, privdata, secs);
}

ev_timer* Events::add_cmd_timer(double secs, ev_timer* w, void* privdata) {
    return add_timer_event(secs, w, on_cmd_timer_callback, privdata, secs);
}

ev_timer* Events::add_timer_event(double secs, ev_timer* w, timer_cb tcb, void* privdata, int repeat_secs) {
    if (w == nullptr) {
        w = (ev_timer*)malloc(sizeof(ev_timer));
        if (w == nullptr) {
            LOG_ERROR("alloc timer failed!");
            return nullptr;
        }
        memset(w, 0, sizeof(ev_timer));
    }

    if (ev_is_active(w)) {
        ev_timer_stop(m_ev_loop, w);
        ev_timer_set(w, secs + ev_time() - ev_now(m_ev_loop), repeat_secs);
        ev_timer_start(m_ev_loop, w);
    } else {
        ev_timer_init(w, tcb, secs + ev_time() - ev_now(m_ev_loop), repeat_secs);
        ev_timer_start(m_ev_loop, w);
    }
    w->data = privdata;

    LOG_DEBUG("start timer, seconds: %f", secs);
    return w;
}

bool Events::restart_timer(double secs, ev_timer* w, void* privdata) {
    if (w == nullptr) {
        return false;
    }
    ev_timer_stop(m_ev_loop, w);
    ev_timer_set(w, secs + ev_time() - ev_now(m_ev_loop), 0);
    ev_timer_start(m_ev_loop, w);
    w->data = privdata;

    LOG_DEBUG("restart timer, seconds: %f", secs);
    return true;
}

bool Events::del_io_event(ev_io* w) {
    if (w == nullptr) {
        return false;
    }

    LOG_DEBUG("delete event, fd: %d", w->fd);

    ev_io_stop(m_ev_loop, w);
    w->data = NULL;
    SAFE_FREE(w);
    return true;
}

bool Events::del_timer_event(ev_timer* w) {
    if (w == nullptr) {
        return false;
    }

    LOG_DEBUG("delete timer event");
    ev_timer_stop(m_ev_loop, w);
    w->data = nullptr;
    SAFE_FREE(w);
    return true;
}

bool Events::stop_event(ev_io* w) {
    if (w == nullptr) {
        return false;
    }
    ev_io_stop(m_ev_loop, w);
    return true;
}

void Events::on_io_callback(struct ev_loop* loop, ev_io* w, int events) {
    if (w->data == nullptr) {
        return;
    }

    int fd = w->fd;
    ICallback* cb = static_cast<ICallback*>(w->data);

    if (events & EV_READ) {
        cb->on_io_read(fd);
    }

    if (events & EV_WRITE) {
        cb->on_io_write(fd);
    }

    /* when error happen (read / write),
     * handle EV_READ / EV_WRITE events will be ok. */
    if (events & EV_ERROR) {
        cb->on_io_error(fd);
    }
}

void Events::on_io_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    std::shared_ptr<Connection> c = static_cast<ConnectionData*>(w->data)->m_conn;
    ICallback* cb = static_cast<ICallback*>(c->get_private_data());
    cb->on_io_timer(w->data);
}

void Events::on_repeat_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    ICallback* cb = static_cast<ICallback*>(w->data);
    cb->on_repeat_timer(w->data);
}

void Events::on_cmd_timer_callback(struct ev_loop* loop, ev_timer* w, int revents) {
    cmd_index_data_t* data = static_cast<cmd_index_data_t*>(w->data);
    ICallback* cb = static_cast<ICallback*>(data->callback);
    cb->on_cmd_timer(w->data);
}

redisAsyncContext* Events::redis_connect(const std::string& host, int port, void* privdata) {
    redisAsyncContext* c = redisAsyncConnect(host.c_str(), port);
    if (c == nullptr || c->err) {
        LOG_ERROR("connect redis failed! errno: %d, error: %s, host: %s, port: %d",
                  c->err, c->errstr, host.c_str(), port);
        return nullptr;
    }

    c->data = privdata;
    redisLibevAttach(m_ev_loop, c);
    redisAsyncSetConnectCallback(c, on_redis_connect);
    redisAsyncSetDisconnectCallback(c, on_redis_disconnect);
    return c;
}

bool Events::redis_send_to(redisAsyncContext* c, const std::string& data, void* privdata) {
    if (c == nullptr || data.empty()) {
        return false;
    }
    std::vector<std::string> args = split_str(data, ' ');
    size_t arglen[args.size()];
    const char* argv[args.size()];
    for (int i = 0; i < args.size(); i++) {
        argv[i] = args[i].c_str();
        arglen[i] = args[i].length();
    }
    int ret = redisAsyncCommandArgv(c, on_redis_callback, privdata, args.size(), argv, arglen);
    if (ret != REDIS_OK) {
        LOG_ERROR("redis send to failed! ret: %d, errno: %d, error: %s",
                  ret, c->err, c->errstr);
    }
    return (ret == REDIS_OK);
}

void Events::on_redis_connect(const redisAsyncContext* c, int status) {
    cmd_index_data_t* index = static_cast<cmd_index_data_t*>(c->data);
    index->callback->on_redis_connect(c, status);
}

void Events::on_redis_disconnect(const redisAsyncContext* c, int status) {
    cmd_index_data_t* index = static_cast<cmd_index_data_t*>(c->data);
    index->callback->on_redis_disconnect(c, status);
}

void Events::on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) {
    cmd_index_data_t* index = static_cast<cmd_index_data_t*>(privdata);
    index->callback->on_redis_callback(c, reply, privdata);
}

}  // namespace kim