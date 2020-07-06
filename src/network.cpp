#include "network.h"

#include <errno.h>
#include <unistd.h>

#include "context.h"
#include "net/anet.h"
#include "net/chanel.h"
#include "server.h"

namespace kim {

#define TCP_BACK_LOG 511
#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
#define MAX_ACCEPTS_PER_CALL 1000

Network::Network(std::shared_ptr<Log> logger, TYPE type) {
    set_type(type);
}

Network::~Network() {
    destory();
}

void Network::destory() {
    end_ev_loop();
    close_conns();
    SAFE_DELETE(m_events);
}

void Network::run() {
    if (m_events != nullptr) {
        m_events->run();
    }
}

bool Network::create(const addr_info_t* addr_info, ISignalCallBack* s, WorkerDataMgr* m) {
    int fd = -1;
    if (addr_info == nullptr || s == nullptr || m == nullptr) {
        return false;
    }

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

    LOG_INFO("listen fds, bind fd: %d, gate bind fd: %d",
             m_bind_fd, m_gate_bind_fd);

    if (!create_events(s, m_bind_fd, m_gate_bind_fd, false)) {
        LOG_ERROR("create events failed!");
        return false;
    }

    m_woker_data_mgr = m;
    return true;
}

bool Network::create(ISignalCallBack* s, int ctrl_fd, int data_fd) {
    if (!create_events(s, ctrl_fd, data_fd, true)) {
        LOG_ERROR("create events failed!");
        return false;
    }

    m_manager_ctrl_fd = ctrl_fd;
    m_manager_data_fd = data_fd;
    LOG_INFO("create network done!");
    return true;
}

bool Network::create_events(ISignalCallBack* s, int fd1, int fd2, bool is_worker) {
    m_events = new Events(m_logger);
    if (m_events == nullptr) {
        LOG_ERROR("new events failed!");
        return false;
    }

    if (!m_events->create()) {
        LOG_ERROR("create events failed!");
        goto error;
    }

    if (!add_conncted_read_event(fd1, is_worker)) {
        LOG_ERROR("add read event failed, fd: %d", fd1);
        goto error;
    }

    if (!add_conncted_read_event(fd2, is_worker)) {
        LOG_ERROR("add read event failed, fd: %d", fd2);
        goto error;
    }

    if (is_worker) {
        m_events->create_signal_event(SIGINT, s);
    } else {
        m_events->setup_signal_events(s);
    }

    return true;

error:
    SAFE_DELETE(m_events);
    return false;
}

bool Network::add_conncted_read_event(int fd, bool is_chanel) {
    if (anet_no_block(m_err, fd) != ANET_OK) {
        close(fd);
        LOG_ERROR("set socket no block failed! fd: %d", fd);
        return false;
    }

    if (!is_chanel) {
        if (anet_keep_alive(m_err, fd, 100) != ANET_OK) {
            close(fd);
            LOG_ERROR("set socket keep alive failed! fd: %d, error: %s", fd, m_err);
            return false;
        }

        if (anet_set_tcp_no_delay(m_err, fd, 1) != ANET_OK) {
            close(fd);
            LOG_ERROR("set socket no delay failed! fd: %d, error: %s", fd, m_err);
            return false;
        }
    }

    Connection* c = create_conn(fd);
    if (c == nullptr) {
        LOG_ERROR("add chanel event failed! fd: %d", fd);
        return false;
    }
    c->set_active_time(m_events->get_now_time());
    c->set_state(Connection::CONN_STATE::CONNECTED);

    ev_io* w = c->get_ev_io();
    if (!m_events->add_read_event(fd, &w, this)) {
        close_conn(c);
        LOG_ERROR("add read event failed! fd: %d", fd);
        return false;
    }
    c->set_ev_io(w);

    LOG_DEBUG("add read event done! fd: %d", fd);
    return true;
}

Connection* Network::create_conn(int fd) {
    auto it = m_conns.find(fd);
    if (it != m_conns.end()) {
        return it->second;
    }

    uint64_t seq = get_new_seq();
    Connection* c = new Connection(fd, seq);
    if (c == nullptr) {
        LOG_ERROR("new connection failed! fd: %d", fd);
        return nullptr;
    }
    m_conns[fd] = c;
    LOG_DEBUG("create connection fd: %d, seq: %llu", fd, seq);
    return c;
}

// delete event to stop callback, and then close fd.
bool Network::close_conn(Connection* c) {
    if (c == nullptr) {
        return false;
    }
    return close_conn(c->get_fd());
}

bool Network::close_conn(int fd) {
    if (fd == -1) {
        LOG_ERROR("invalid fd: %d", fd);
        return false;
    }

    auto it = m_conns.find(fd);
    if (it == m_conns.end()) {
        LOG_WARNING("delele conn failed! fd: %d", fd);
        return false;
    }

    Connection* c = it->second;
    if (c != nullptr) {
        m_events->del_event(c->get_ev_io());
        SAFE_DELETE(c);
    }
    m_conns.erase(it);
    close(fd);

    LOG_DEBUG("close fd: %d", fd);
    return true;
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

    LOG_INFO("listen_to_port, %s:%d", bind, port);
    return fd;
}

void Network::close_chanel(int* fds) {
    LOG_DEBUG("close chanel, fd0: %d, fd1: %d", fds[0], fds[1]);

    if (close(fds[0]) == -1) {
        LOG_WARNING("close channel failed, fd: %d.", fds[0]);
    }

    if (close(fds[1]) == -1) {
        LOG_WARNING("close channel failed, fd: %d.", fds[1]);
    }
}

void Network::close_conns() {
    LOG_DEBUG("close_conns(), cnt: %d", m_conns.size());

    auto it = m_conns.begin();
    for (; it != m_conns.end(); it++) {
        close_conn(it->second);
    }
}

void Network::close_fds() {
    auto it = m_conns.begin();
    for (; it != m_conns.end(); it++) {
        Connection* c = static_cast<Connection*>(it->second);
        int fd = c->get_fd();
        if (fd != -1) {
            close(fd);
            c->set_fd(-1);
        }
    }
}

void Network::on_io_read(int fd) {
    if (is_manager()) {
        if (fd == m_bind_fd) {
            accept_server_conn(fd);
        } else if (fd == m_gate_bind_fd) {
            accept_and_transfer_fd(fd);
        } else {
            read_query_from_client(fd);
        }
    } else if (is_worker()) {
        if (fd == m_manager_data_fd) {
            read_transfer_fd(fd);
        } else {
            LOG_DEBUG("worker io read!, fd: %d", fd);
            read_query_from_client(fd);
        }
    } else {
        LOG_CRIT("unknown work type io read! exit!");
        exit(EXIT_FAILURE);
    }
}

void Network::on_io_write(int fd) {
    auto it = m_conns.find(fd);
    if (it == m_conns.end()) {
        return;
    }

    Connection* c = it->second;
    if (c == nullptr || !c->is_active()) {
        return;
    }
}

void Network::on_io_error(int fd) {
}

void Network::read_query_from_client(int fd) {
    auto it = m_conns.find(fd);
    if (it == m_conns.end()) {
        LOG_WARNING("find connection failed, fd: %d", fd);
        return;
    }

    Connection* c = it->second;
    if (c == nullptr) {
        LOG_ERROR("connection is null! fd: %d", fd);
        return;
    }

    // connection read data.
    int read_len = c->read_data();
    if (read_len == 0) {
        close_conn(c);
        LOG_DEBUG("connection closed, fd: %d", fd);
        return;
    }

    if (read_len < 0) {
        if ((read_len == -1) && (c->is_active())) {  // EAGAIN
            return;
        }
        close_conn(c);
        LOG_DEBUG("client closed connection! fd: %d", fd);
        return;
    }

    c->set_active_time(m_events->get_now_time());

    // analysis data.
    LOG_DEBUG("recv data: %s", c->get_query_data());
}

void Network::accept_server_conn(int fd) {
    char cip[NET_IP_STR_LEN];
    int cport, cfd, family, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        cfd = anet_tcp_accept(m_err, fd, cip, sizeof(cip), &cport, &family);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                LOG_ERROR("accepting client connection failed: fd: %d, error %s",
                          fd, m_err);
            return;
        }

        LOG_DEBUG("accepted %s:%d", cip, cport);

        if (!add_conncted_read_event(cfd)) {
            close_conn(cfd);
            LOG_ERROR("add data fd read event failed, fd: %d", cfd);
            return;
        }
    }
}

// manager accept new fd and then transfer it to worker.
void Network::accept_and_transfer_fd(int fd) {
    int cport, cfd, family;
    char cip[NET_IP_STR_LEN] = {0};

    cfd = anet_tcp_accept(m_err, fd, cip, sizeof(cip), &cport, &family);
    if (cfd == ANET_ERR) {
        if (errno != EWOULDBLOCK) {
            LOG_WARNING("accepting client connection: %s", m_err);
        }
        return;
    }

    LOG_INFO("accepted: %s:%d", cip, cport);

    int chanel_fd = m_woker_data_mgr->get_next_worker_data_fd();
    if (chanel_fd > 0) {
        LOG_DEBUG("send new fd: %d to worker communication fd %d", cfd, chanel_fd);
        channel_t ch = {cfd, family, 1};
        int err = write_channel(chanel_fd, &ch, sizeof(channel_t), m_logger);
        if (err != 0) {
            LOG_ERROR("write channel failed! errno: %d", err);
            goto error;
        }
        close(cfd);
        return;
    }

error:
    LOG_ERROR("write channel failed!");
    close(cfd);
}

// worker send fd which transfered from manager.
void Network::read_transfer_fd(int fd) {
    channel_t ch;
    int err, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        // recv fd from manager.
        err = read_channel(fd, &ch, sizeof(channel_t), m_logger);
        if (err != 0) {
            if (err != EAGAIN) {
                destory();
                LOG_ERROR("read channel failed!, exit!");
                exit(EXIT_FD_TRANSFER);
            }
            return;
        }

        if (!add_conncted_read_event(ch.fd)) {
            LOG_ERROR("add data fd read event failed, fd: %d", ch.fd);
            close_conn(fd);
            return;
        }

        LOG_DEBUG("read channel, channel data: fd: %d, family: %d, codec: %d",
                  ch.fd, ch.family, ch.codec);
    }
}

void Network::end_ev_loop() {
    if (m_events != nullptr) {
        m_events->end_ev_loop();
    }
}

}  // namespace kim