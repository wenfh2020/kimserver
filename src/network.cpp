#include "network.h"

#include <unistd.h>

#include "context.h"
#include "net/anet.h"
#include "server.h"

namespace kim {

#define TCP_BACK_LOG 511
#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
#define MAX_ACCEPTS_PER_CALL 1000

Network::Network(Log* logger) : m_logger(logger),
                                m_seq(0),
                                m_bind_fd(0),
                                m_gate_bind_fd(0),
                                m_events(NULL),
                                m_is_created(false) {
}

Network::~Network() {
    destory();
}

void Network::destory() {
    close_conns();
    close_listen_sockets();
    SAFE_DELETE(m_events);
}

bool Network::create(const addr_info_t* addr_info, ISignalCallBack* s) {
    if (addr_info == NULL) return false;

    if (m_is_created) {
        LOG_ERROR("network has been created!");
        return false;
    }

    int fd = -1;

    if (!addr_info->bind.empty()) {
        fd = listen_to_port(addr_info->bind.c_str(), addr_info->port);
        if (fd == -1) {
            LOG_ERROR("listen to port fail! %s:%d",
                      addr_info->bind.c_str(), addr_info->port);
            return false;
        }
        m_bind_fd = fd;
    }

    if (!addr_info->gate_bind.empty()) {
        fd = listen_to_port(addr_info->gate_bind.c_str(), addr_info->gate_port);
        if (fd == -1) {
            LOG_ERROR("listen to gate fail! %s:%d",
                      addr_info->gate_bind.c_str(), addr_info->gate_port);
            return false;
        }
        m_gate_bind_fd = fd;
    }

    if (!init_events(s)) {
        LOG_ERROR("init events failed!");
        return false;
    }

    m_is_created = true;
    return true;
}

bool Network::init_events(ISignalCallBack* s) {
    m_events = new Events(m_logger);
    if (m_events == NULL) {
        LOG_ERROR("new events failed!");
        return false;
    }

    LOG_DEBUG("new events done!");

    if (!m_events->create(s, this)) {
        SAFE_DELETE(m_events);
        LOG_ERROR("create events failed!");
        return false;
    }

    LOG_DEBUG("create events done!");

    if (!add_conncted_read_event(m_bind_fd)) {
        SAFE_DELETE(m_events);
        LOG_ERROR("add bind read event failed, fd: %d", m_bind_fd);
        return false;
    }

    LOG_DEBUG("add bind read event failed, fd: %d", m_bind_fd);

    if (!add_conncted_read_event(m_gate_bind_fd)) {
        SAFE_DELETE(m_events);
        LOG_ERROR("add gate bind read event failed, fd: %d", m_gate_bind_fd);
        return false;
    }

    LOG_DEBUG("add gate bind read event failed, fd: %d", m_gate_bind_fd);
    return true;
}

bool Network::add_conncted_read_event(int fd) {
    LOG_DEBUG("add_conncted_read_event");

    Connection* c = create_conn(fd);
    if (c == NULL) {
        LOG_ERROR("create connection failed!");
        return false;
    }

    c->set_state(kim::Connection::CONN_STATE_CONNECTED);
    return m_events->add_read_event(c);
}

bool Network::add_chanel_event(int fd) {
    Connection* c = create_conn(fd);
    if (c != NULL) {
        c->set_state(kim::Connection::CONN_STATE_CONNECTED);
        m_events->add_read_event(c);
        return true;
    }

    return false;
}

Connection* Network::create_conn(int fd) {
    LOG_DEBUG("create_conn");

    std::map<int, Connection*>::iterator it = m_conns.find(fd);
    if (it != m_conns.end()) {
        return it->second;
    }

    uint64_t seq;
    Connection* c;

    seq = get_new_seq();
    c = new Connection(fd, seq);
    m_conns[fd] = c;

    LOG_DEBUG("create connection fd: %d, seq: %llu", fd, seq);
    return c;
}

void Network::run() {
    if (m_events == NULL) {
        LOG_ERROR("pls create events firstly!")
        return;
    }

    m_events->run();
}

int Network::listen_to_port(const char* bind, int port) {
    int fd = -1;
    char err[256] = {0};

    if (strchr(bind, ':')) {
        /* Bind IPv6 address. */
        fd = anet_tcp6_server(err, port, bind, TCP_BACK_LOG);
        if (fd == -1) {
            LOG_ERROR("bind tcp ipv6 fail! %s", err);
        }
    } else {
        /* Bind IPv4 address. */
        fd = anet_tcp_server(err, port, bind, TCP_BACK_LOG);
        if (fd == -1) {
            LOG_ERROR("bind tcp ipv4 fail! %s", err);
        }
    }

    if (fd != -1) {
        anet_no_block(NULL, fd);
    }

    return fd;
}

void Network::close_chanel(int* fds) {
    if (close(fds[0]) == -1) {
        LOG_WARNING("close channel failed, fd: %d.", fds[0]);
    }

    if (close(fds[1]) == -1) {
        LOG_WARNING("close channel failed, fd: %d.", fds[1]);
    }
}

void Network::close_listen_sockets() {
    if (m_bind_fd != -1) close(m_bind_fd);
    if (m_gate_bind_fd != -1) close(m_gate_bind_fd);
}

void Network::close_conns() {
    std::map<int, Connection*>::iterator it = m_conns.begin();
    for (; it != m_conns.begin(); it++) {
        close_conn(it->second);
    }
}

bool Network::io_read(Connection* c, struct ev_io* e) {
    LOG_DEBUG("io_read");

    if (c == NULL || e == NULL) return false;

    if (e->fd == m_bind_fd) {
        // accept.
        accept_server_conn(e->fd);
    } else if (e->fd == m_gate_bind_fd) {
        // transfer client connect from manger to child pro.
    } else {
        return true;
    }

    LOG_DEBUG("io read fd: %d, seq: %d, e->fd: %d, bind fd: %d, gate_bind_fd: %d",
              c->get_fd(), c->get_id(), e->fd, m_bind_fd, m_gate_bind_fd);
    return true;
}

bool Network::io_write(Connection* c, struct ev_io* e) {
    LOG_DEBUG("io write fd: %d, seq: %d", c->get_fd(), c->get_id());
    return true;
}

bool Network::io_error(Connection* c, struct ev_io* e) {
    LOG_DEBUG("io error fd: %d, seq: %d", c->get_fd(), c->get_id());
    return true;
}

bool Network::accept_server_conn(int fd) {
    LOG_DEBUG("accept_server_conn()");

    accept_tcp_handler(fd);
    return true;
}

void Network::accept_tcp_handler(int fd) {
    LOG_DEBUG("accept_tcp_handler()");

    if (fd == -1) {
        LOG_ERROR("invalid fd: %d", fd);
        return;
    }

    char cip[NET_IP_STR_LEN];
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        cfd = anet_tcp_accept(m_err, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                LOG_WARNING("accepting client connection: %s", m_err);
            return;
        }

        LOG_DEBUG("accepted %s:%d", cip, cport);

        Connection* c = create_conn(cfd);
        c->set_state(Connection::CONN_STATE_ACCEPTING);

        anet_no_block(NULL, cfd);
        anet_keep_alive(NULL, cfd, 100);
        anet_set_tcp_no_delay(NULL, cfd, 1);
        if (!m_events->add_read_event(c)) {
            LOG_ERROR("add read event failed! fd: %d", cfd);
            return;
        }
    }
}

bool Network::close_conn(Connection* c) {
    LOG_DEBUG("close_conn()");

    if (c == NULL) return false;

    int fd = c->get_fd();
    std::map<int, Connection*>::iterator it = m_conns.find(fd);
    if (it == m_conns.end()) {
        LOG_WARNING("close conn failed! fd: %d", fd);
        return false;
    }

    m_events->del_event(c);

    if (fd != -1) close(fd);
    SAFE_DELETE(c);
    m_conns.erase(it);
    return true;
}

}  // namespace kim