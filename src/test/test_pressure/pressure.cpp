#include "pressure.h"

#include "net/anet.h"

namespace kim {

Pressure::Pressure(Log* logger, struct ev_loop* loop)
    : m_logger(logger), m_loop(loop) {
}

Pressure::~Pressure() {
    Connection* c;
    for (auto& it : m_conns) {
        c = it.second;
        cleanup_events(c);
        close(c->fd());
        SAFE_DELETE(c);
    }
    m_conns.clear();
}

bool Pressure::start(const char* host, int port, int users, int packets) {
    if (host == nullptr || port == 0 || users == 0 || packets == 0) {
        LOG_ERROR("invalid params!");
        return false;
    }

    LOG_DEBUG("start pressure, host: %s, port: %d, users: %d, packets: %d",
              host, port, users, packets);

    for (int i = 0; i < users; i++) {
        Connection* c = get_connect(host, port);
        if (c == nullptr) {
            LOG_ERROR("async connect failed, host: %s, port: %d", host, port);
            continue;
        }

        if (!attach_libev(c, packets)) {
            LOG_ERROR("attach libev failed! fd: %d", c->fd());
            del_connect(c);
            continue;
        }

        add_write_event(c);
    }

    m_packets = packets;
    m_begin_time = time_now();
    return true;
}

void Pressure::show_statics_result() {
    LOG_DEBUG("send cnt: %d, cur callback cnt: %d", m_send_cnt, m_cur_callback_cnt);

    if (m_send_cnt == m_cur_callback_cnt) {
        std::cout << "------" << std::endl
                  << "spend time: " << time_now() - m_begin_time << std::endl
                  << "avg:        " << m_send_cnt / (time_now() - m_begin_time) << std::endl;

        std::cout << "send cnt:         " << m_send_cnt << std::endl
                  << "callback cnt:     " << m_cur_callback_cnt << std::endl
                  << "ok callback cnt:  " << m_ok_callback_cnt << std::endl
                  << "err callback cnt: " << m_err_callback_cnt << std::endl;
    }
}

Connection* Pressure::get_connect(const char* host, int port) {
    if (host == nullptr || port == 0) {
        LOG_ERROR("invalid params!");
        return nullptr;
    }

    int fd;
    Connection* c;

    fd = anet_tcp_connect(m_errstr, host, port, false, &m_saddr, &m_saddr_len);
    if (fd == -1) {
        LOG_ERROR("client connect server failed! errstr: %s", m_errstr);
        return nullptr;
    }

    // connection.
    c = new Connection(m_logger, fd, new_seq());
    if (c == nullptr) {
        close(fd);
        LOG_ERROR("alloc connection failed! fd: %d", fd);
        return nullptr;
    }

    c->init(Codec::TYPE::PROTOBUF);
    c->set_state(Connection::STATE::CONNECTING);
    c->set_addr_info(&m_saddr, m_saddr_len);
    m_conns[fd] = c;
    LOG_INFO("new connection done! fd: %d", fd);
    return c;
}

bool Pressure::del_connect(Connection* c) {
    if (c == nullptr) {
        LOG_ERROR("invalid params!");
        return false;
    }
    auto it = m_conns.find(c->fd());
    if (it == m_conns.end()) {
        return false;
    }
    cleanup_events(c);
    close(c->fd());
    SAFE_DELETE(it->second);
    m_conns.erase(it);
    return true;
}

bool Pressure::check_connect(Connection* c) {
    if (!c->is_connected()) {
        if (!async_handle_connect(c)) {
            LOG_DEBUG("connect failed! fd: %d", c->fd());
            del_connect(c);
            return false;
        }
        if (!c->is_connected()) {
            LOG_DEBUG("connect next time! fd: %d", c->fd());
            return false;
        }
        LOG_DEBUG("connect ok! fd: %d", c->fd());
    }
    return true;
}

bool Pressure::send_packets(Connection* c) {
    if (c == nullptr || !c->is_connected()) {
        LOG_ERROR("invalid connection!");
        return false;
    }

    libev_events_t* e = (libev_events_t*)c->privdata();
    if ((e->stat.packets > 0 &&
         e->stat.send_cnt < e->stat.packets &&
         e->stat.send_cnt == e->stat.callback_cnt)) {
        for (int i = 0; i < 100 && i < e->stat.packets; i++) {
            if (e->stat.send_cnt >= e->stat.packets) {
                return true;
            }
            m_send_cnt++;
            e->stat.send_cnt++;
            if (!send_proto(c, 1)) {
                return false;
            }
            continue;
        }
        add_read_event(c);
    }

    LOG_DEBUG("packets: %d, send cnt: %d, callback cnt: %d",
              e->stat.packets, e->stat.send_cnt, e->stat.callback_cnt);
    return true;
}

bool Pressure::send_proto(Connection* c, int cmd) {
    LOG_DEBUG("send proto, fd: %d, cmd: %d!", c->fd(), cmd);

    MsgHead head;
    MsgBody body;

    if (cmd == 1) {
        body.set_data("hello!1223fdsfsdfdsfds45+");
        head.set_cmd(1);
        head.set_seq(new_seq());
        head.set_len(body.ByteSizeLong());
    } else {
        return false;
    }

    LOG_DEBUG("send seq: %d, body len: %d, data: <%s>",
              head.seq(),
              body.ByteSizeLong(),
              body.SerializeAsString().c_str());
    return send_proto(c, head, body);
}

bool Pressure::send_proto(Connection* c, MsgHead& head, MsgBody& body) {
    Codec::STATUS status = c->conn_write(head, body);
    switch (status) {
        case Codec::STATUS::ERR:
            LOG_ERROR("conn write data failed! fd: %d", c->fd());
            del_connect(c);
            return false;
        case Codec::STATUS::OK:
            LOG_DEBUG("conn write data done! fd: %d", c->fd());
            del_write_event(c);
            return true;
        case Codec::STATUS::PAUSE:
            LOG_DEBUG("conn write data next time! fd: %d", c->fd());
            add_write_event(c);
            return true;
        default:
            LOG_ERROR("invalid status: %d", status);
            return false;
    }
}

bool Pressure::attach_libev(Connection* c, int packets) {
    LOG_DEBUG("connection attach libev! conn: %p", c);
    if (c == nullptr || m_loop == nullptr) {
        return false;
    }

    libev_events_t* e = new libev_events_t;
    e->press = this;
    e->conn = c;
    e->loop = m_loop;
    e->rev.data = e;
    e->wev.data = e;
    e->stat.packets = packets;
    c->set_privdata(e);

    ev_io_init(&e->rev, libev_read_event, c->fd(), EV_READ);
    ev_io_init(&e->wev, libev_write_event, c->fd(), EV_WRITE);
    return true;
}

void Pressure::cleanup_events(Connection* c) {
    del_read_event(c);
    del_write_event(c);
}

void Pressure::libev_read_event(EV_P_ ev_io* watcher, int revents) {
    libev_events_t* e = (libev_events_t*)watcher->data;
    e->press->async_handle_read(e->conn);
}

void Pressure::libev_write_event(EV_P_ ev_io* watcher, int revents) {
    libev_events_t* e = (libev_events_t*)watcher->data;
    e->press->async_handle_write(e->conn);
}

void Pressure::add_write_event(Connection* c) {
    if (c == nullptr) {
        return;
    }
    libev_events_t* e = (libev_events_t*)c->privdata();
    struct ev_loop* loop = e->loop;
    ((void)loop);
    if (!e->writing) {
        e->writing = 1;
        ev_io_start(EV_A_ & e->wev);
    }
}

void Pressure::del_write_event(Connection* c) {
    libev_events_t* e = (libev_events_t*)c->privdata();
    struct ev_loop* loop = e->loop;
    ((void)loop);
    if (e->writing) {
        e->writing = 0;
        ev_io_stop(EV_A_ & e->wev);
    }
}

void Pressure::async_handle_write(Connection* c) {
    if (!check_connect(c)) {
        return;
    }

    if (!send_packets(c)) {
        return;
    }

    Codec::STATUS status = c->conn_write();
    switch (status) {
        case Codec::STATUS::ERR:
            del_connect(c);
            break;
        case Codec::STATUS::PAUSE:
            add_write_event(c);
            break;
        case Codec::STATUS::OK:
            del_write_event(c);
            break;
        default:
            break;
    }
}

void Pressure::add_read_event(Connection* c) {
    libev_events_t* e = (libev_events_t*)c->privdata();
    struct ev_loop* loop = e->loop;
    ((void)loop);
    if (!e->reading) {
        e->reading = 1;
        ev_io_start(EV_A_ & e->rev);
    }
}

void Pressure::del_read_event(Connection* c) {
    libev_events_t* e = (libev_events_t*)c->privdata();
    struct ev_loop* loop = e->loop;
    ((void)loop);
    if (e->reading) {
        e->reading = 0;
        ev_io_stop(EV_A_ & e->rev);
    }
}

void Pressure::async_handle_read(Connection* c) {
    if (!check_connect(c)) {
        return;
    }

    MsgHead head;
    MsgBody body;
    Codec::STATUS status;
    libev_events_t* e = (libev_events_t*)c->privdata();

    status = c->conn_read(head, body);
    while (status == Codec::STATUS::OK) {
        m_ok_callback_cnt++;
        m_cur_callback_cnt++;
        e->stat.callback_cnt++;
        LOG_DEBUG("xxxxcallback: %d", e->stat.callback_cnt);

        LOG_DEBUG("cmd: %d, seq: %d, len: %d, body len: %zu, %s",
                  head.cmd(), head.seq(), head.len(),
                  body.data().length(), body.data().c_str());
        show_statics_result();

        head.Clear();
        body.Clear();
        status = c->fetch_data(head, body);
    }

    if (status == Codec::STATUS::PAUSE) {
        LOG_DEBUG("wait next decoding......fd: %d", c->fd());
        show_statics_result();
        add_read_event(c);
        if (!send_packets(c)) {
            return;
        }
        return;
    }

    if (status == Codec::STATUS::ERR) {
        m_cur_callback_cnt++;
        m_err_callback_cnt++;
        e->stat.callback_cnt++;
        LOG_DEBUG("read data failed! fd: %d", c->fd());
        del_connect(c);
        show_statics_result();
    }
}

bool Pressure::async_handle_connect(Connection* c) {
    bool completed = false;
    if (anet_check_connect_done(
            c->fd(), c->sockaddr(), c->saddr_len(), completed) == ANET_ERR) {
        LOG_ERROR("connect failed! fd: %d", c->fd());
        return false;
    } else {
        if (completed) {
            LOG_DEBUG("connect done! fd: %d", c->fd());
            c->set_state(Connection::STATE::CONNECTED);
        } else {
            LOG_DEBUG("connect not completed! fd: %d", c->fd());
        }
        return true;
    }
}

}  // namespace kim
