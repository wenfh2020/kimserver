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
    if (users == 0 || packets == 0 || host == nullptr || port == 0) {
        LOG_ERROR("invalid params!");
        return false;
    }

    LOG_DEBUG("start pressure, host: %s, port: %d, users: %d, packets: %d",
              host, port, users, packets);

    for (int i = 0; i < users; i++) {
        // connect.
        Connection* c = get_connect(host, port);
        if (c == nullptr) {
            LOG_ERROR("async connect failed, host: %s, port: %d", host, port);
            continue;
        }

        // add events.
        if (!attach_libev(c)) {
            LOG_ERROR("attach libev failed! fd: %d", c->fd());
            del_connect(c);
            continue;
        }
    }

    m_packets = packets;
    m_begin_time = time_now();
    return true;
}

Connection* Pressure::get_connect(const char* host, int port) {
    if (host == nullptr || port == 0) {
        LOG_ERROR("invalid params!");
        return nullptr;
    }

    int fd;
    Connection* c;

    // if (m_saddr_len != 0) {
    fd = anet_tcp_connect(m_errstr, host, port, false, &m_saddr, &m_saddr_len);
    if (fd == -1) {
        LOG_ERROR("client connect server failed! errstr: %s", m_errstr);
        return nullptr;
    } else {
        fd = socket(PF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            LOG_ERROR("create socket failed!");
            return nullptr;
        }

        if (anet_no_block(m_errstr, fd) == -1) {
            LOG_ERROR("set no block failed!");
            return nullptr;
        }

        bool completed = false;
        if (anet_check_connect_done(fd, &m_saddr, m_saddr_len, completed) == ANET_ERR) {
            LOG_ERROR("connect failed! fd: %d", fd);
            return nullptr;
        } else {
            if (completed) {
                LOG_DEBUG("connect done! fd: %d", fd);
            } else {
                LOG_DEBUG("connect not completed! fd: %d", fd);
            }
        }
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
}  // namespace kim

bool Pressure::attach_libev(Connection* c) {
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
    c->set_privdata(e);
    ev_io_init(&e->rev, libev_read_event, c->fd(), EV_READ);
    ev_io_init(&e->wev, libev_write_event, c->fd(), EV_WRITE);
    add_read_event(c);
    add_write_event(c);
    return true;
}

bool Pressure::send_proto(Connection* c, MsgHead& head, MsgBody& body) {
    Codec::STATUS status = c->conn_write(head, body);
    if (status == Codec::STATUS::ERR) {
        LOG_ERROR("conn write data failed! fd: %d", c->fd());
        del_connect(c);
        return false;
    } else {
        if (status == Codec::STATUS::OK) {
            LOG_DEBUG("conn write data done! fd: %d", c->fd());
            del_write_event(c);
        } else {
            LOG_DEBUG("conn write data next time! fd: %d", c->fd());
            add_write_event(c);
        }
    }
    add_read_event(c);
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

void Pressure::libev_read_event(EV_P_ ev_io* watcher, int revents) {
    libev_events_t* e = (libev_events_t*)watcher->data;
    e->press->async_handle_read(e->conn);
}

void Pressure::libev_write_event(EV_P_ ev_io* watcher, int revents) {
    libev_events_t* e = (libev_events_t*)watcher->data;
    e->press->async_handle_write(e->conn);
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

void Pressure::async_handle_write(Connection* c) {
    if (!c->is_connected()) {
        if (!async_handle_connect(c)) {
            LOG_DEBUG("connect failed! fd: %d", c->fd());
            del_connect(c);
            return;
        }
        if (!c->is_connected()) {
            LOG_DEBUG("connect next time! fd: %d", c->fd());
            return;
        }
        LOG_DEBUG("connect ok! fd: %d", c->fd());

        for (int i = 0; i < m_packets; i++) {
            m_send_cnt++;
            if (!send_proto(c, 1)) {
                m_send_err_cnt++;
                LOG_ERROR("send to server failed!");
                continue;
            } else {
                m_send_ok_cnt++;
            }
        }
    }
}

void Pressure::async_handle_read(Connection* c) {
    LOG_DEBUG("");
    if (!c->is_connected()) {
        if (!async_handle_connect(c)) {
            LOG_DEBUG("connect failed! fd: %d", c->fd());
            del_connect(c);
            return;
        }
        if (!c->is_connected()) {
            return;
        }
        LOG_INFO("connect ok! fd: %d", c->fd());

        for (int i = 0; i < m_packets; i++) {
            m_send_cnt++;
            if (!send_proto(c, 1)) {
                m_send_err_cnt++;
                LOG_ERROR("send to server failed!");
                continue;
            } else {
                m_send_ok_cnt++;
            }
        }
    }

    if (!c->is_connected()) {
        return;
    }

    add_read_event(c);

    MsgHead head;
    MsgBody body;
    Codec::STATUS status;

    status = c->conn_read(head, body);
    LOG_DEBUG("status: %d.....", status);
    while (status == Codec::STATUS::OK) {
        m_cur_callback_cnt++;
        m_ok_callback_cnt++;
        LOG_DEBUG("cmd: %d, seq: %d, len: %d, body len: %zu, %s",
                  head.cmd(), head.seq(), head.len(),
                  body.data().length(), body.data().c_str());
        show_statics_result();

        head.Clear();
        body.Clear();
        status = c->fetch_data(head, body);
    }

    if (status == Codec::STATUS::PAUSE) {
        LOG_DEBUG("wait decoding......fd: %d", c->fd());
        show_statics_result();
        return;
    }

    if (status == Codec::STATUS::ERR) {
        m_cur_callback_cnt++;
        m_err_callback_cnt++;
        LOG_DEBUG("read data failed! fd: %d", c->fd());
        del_connect(c);
        show_statics_result();
    }
}

void Pressure::show_statics_result() {
    LOG_DEBUG("ok send cnt: %d, cur callback cnt: %d",
              m_send_ok_cnt, m_cur_callback_cnt);
    if (m_send_ok_cnt == m_cur_callback_cnt) {
        std::cout << "------" << std::endl
                  << "spend time: " << time_now() - m_begin_time << std::endl
                  << "write avg:  " << m_send_ok_cnt / (time_now() - m_begin_time) << std::endl;

        std::cout << "send cnt:         " << m_send_cnt << std::endl
                  << "send ok cnt:      " << m_send_ok_cnt << std::endl
                  << "send err cnt:     " << m_send_err_cnt << std::endl
                  << "callback cnt:     " << m_cur_callback_cnt << std::endl
                  << "ok callback cnt:  " << m_ok_callback_cnt << std::endl
                  << "err callback cnt: " << m_err_callback_cnt << std::endl;
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

void Pressure::cleanup_events(Connection* c) {
    del_read_event(c);
    del_write_event(c);
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

}  // namespace kim
