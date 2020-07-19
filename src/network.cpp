#include "network.h"

#include <errno.h>
#include <unistd.h>

#include "context.h"
#include "module.h"
#include "module/module_core.h"
#include "net/anet.h"
#include "net/chanel.h"
#include "server.h"
#include "util/util.h"

namespace kim {

#define TCP_BACK_LOG 511
#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
#define MAX_ACCEPTS_PER_CALL 1000
#define CORE_MODULE "core-module"

Network::Network(Log* logger, TYPE type)
    : m_logger(logger), m_type(type) {
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

bool Network::create(const AddrInfo* addr_info,
                     Codec::TYPE code_type, ICallback* s, WorkerDataMgr* m) {
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

    if (!create_events(s, m_bind_fd, m_gate_bind_fd, code_type, false)) {
        LOG_ERROR("create events failed!");
        return false;
    }

    m_woker_data_mgr = m;
    return true;
}

bool Network::create(ICallback* s, int ctrl_fd, int data_fd) {
    if (!create_events(s, ctrl_fd, data_fd, Codec::TYPE::PROTOBUF, true)) {
        LOG_ERROR("create events failed!");
        return false;
    }
    m_manager_ctrl_fd = ctrl_fd;
    m_manager_data_fd = data_fd;
    LOG_INFO("create network done!");

    if (!load_modules()) {
        LOG_ERROR("load module failed!");
        return false;
    }

    return true;
}

bool Network::create_events(ICallback* s, int fd1, int fd2,
                            Codec::TYPE codec_type, bool is_worker) {
    m_events = new Events(m_logger);
    if (m_events == nullptr) {
        LOG_ERROR("new events failed!");
        return false;
    }

    if (!m_events->create()) {
        LOG_ERROR("create events failed!");
        goto error;
    }

    if (!add_read_event(fd1, codec_type, is_worker) ||
        !add_read_event(fd2, codec_type, is_worker)) {
        close_conn(fd1);
        LOG_ERROR("add read event failed, fd1: %d, fd2: %d", fd1, fd2);
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

std::shared_ptr<Connection> Network::add_read_event(int fd, Codec::TYPE codec_type, bool is_chanel) {
    if (anet_no_block(m_err, fd) != ANET_OK) {
        LOG_ERROR("set socket no block failed! fd: %d", fd);
        return nullptr;
    }

    if (!is_chanel) {
        if (anet_keep_alive(m_err, fd, 100) != ANET_OK) {
            LOG_ERROR("set socket keep alive failed! fd: %d, error: %s", fd, m_err);
            return nullptr;
        }
        if (anet_set_tcp_no_delay(m_err, fd, 1) != ANET_OK) {
            LOG_ERROR("set socket no delay failed! fd: %d, error: %s", fd, m_err);
            return nullptr;
        }
    }

    ev_io* w;
    std::shared_ptr<Connection> c;

    c = create_conn(fd);
    if (c == nullptr) {
        LOG_ERROR("add chanel event failed! fd: %d", fd);
        return nullptr;
    }
    c->init(codec_type);
    c->set_private_data(this);
    c->set_active_time(mstime());
    c->set_state(Connection::STATE::CONNECTED);

    w = c->get_ev_io();
    if (w == nullptr) {
        w = m_events->add_read_event(fd, this);
        if (w == nullptr) {
            LOG_ERROR("add read event failed! fd: %d", fd);
            return nullptr;
        }
    } else {
        if (!m_events->restart_read_event(w, fd, this)) {
            LOG_ERROR("reastart read event failed! fd: %d", fd);
            return nullptr;
        }
    }
    c->set_ev_io(w);

    LOG_DEBUG("add read event done! fd: %d", fd);
    return c;
}

std::shared_ptr<Connection> Network::create_conn(int fd) {
    auto it = m_conns.find(fd);
    if (it != m_conns.end()) {
        return it->second;
    }

    uint64_t seq;
    std::shared_ptr<Connection> c;

    seq = get_new_seq();
    c = std::make_shared<Connection>(m_logger, fd, seq);
    if (c == nullptr) {
        LOG_ERROR("new connection failed! fd: %d", fd);
        return nullptr;
    }
    c->set_keep_alive(m_keep_alive);
    m_conns[fd] = c;
    LOG_DEBUG("create connection fd: %d, seq: %llu", fd, seq);
    return c;
}

// delete event to stop callback, and then close fd.
bool Network::close_conn(std::shared_ptr<Connection> c) {
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
    if (it != m_conns.end()) {
        std::shared_ptr<Connection> c = it->second;
        if (c != nullptr) {
            c->set_state(Connection::STATE::CLOSED);
            m_events->del_event(c->get_ev_io());

            ev_timer* w = c->get_ev_timer();
            if (w != nullptr) {
                delete static_cast<ConnectionData*>(w->data);
                w->data = nullptr;
                m_events->del_event(w);
            }
        }
        m_conns.erase(it);
    }

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

    LOG_INFO("listen to port, %s:%d", bind, port);
    return fd;
}

void Network::close_chanel(int* fds) {
    LOG_DEBUG("close chanel, fd0: %d, fd1: %d", fds[0], fds[1]);

    if (close(fds[0]) == -1) {
        LOG_WARN("close channel failed, fd: %d.", fds[0]);
    }

    if (close(fds[1]) == -1) {
        LOG_WARN("close channel failed, fd: %d.", fds[1]);
    }
}

void Network::close_conns() {
    LOG_DEBUG("close_conns(), cnt: %d", m_conns.size());

    for (const auto& it : m_conns) {
        close_conn(it.second);
    }
}

void Network::close_fds() {
    for (const auto& it : m_conns) {
        int fd = it.second->get_fd();
        if (fd != -1) {
            close(fd);
            it.second->set_fd(-1);
        }
    }
}

void Network::on_io_write(int fd) {
    auto it = m_conns.find(fd);
    if (it == m_conns.end()) {
        return;
    }

    std::shared_ptr<Connection> c = it->second;
    if (c == nullptr || !c->is_active()) {
        return;
    }
}

void Network::on_io_error(int fd) {
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
            read_query_from_client(fd);
        }
    } else {
        read_query_from_client(fd);
    }
}

void Network::on_timer(void* privdata) {
    int secs;
    ConnectionData* conn_data;
    std::shared_ptr<Connection> c;

    conn_data = static_cast<ConnectionData*>(privdata);
    c = conn_data->m_conn;
    secs = c->get_keep_alive() - (mstime() - c->get_active_time()) / 1000;
    if (secs > 0) {
        LOG_DEBUG("timer restart, fd: %d, restart timer secs: %d", c->get_fd(), secs);
        m_events->restart_timer(secs, c->get_ev_timer(), conn_data);
        return;
    }

    close_conn(c);
    LOG_DEBUG("time up, close connection! secs: %d, fd: %d, seq: %llu",
              secs, c->get_fd(), c->get_id());
}

bool Network::read_query_from_client(int fd) {
    auto it = m_conns.find(fd);
    if (it == m_conns.end() || it->second == nullptr) {
        LOG_WARN("find connection failed, fd: %d", fd);
        return false;
    }

    std::shared_ptr<Connection> c = it->second;
    if (!c->is_active()) {
        return false;
    }

    if (c->is_http_codec()) {
        HttpMsg msg;
        Codec::STATUS status = c->conn_read(msg);
        if (status == Codec::STATUS::OK) {
            Request req(c, &msg);
            for (Module* m : m_core_modules) {
                LOG_DEBUG("module name: %s", m->get_name().c_str());
                if (m->process_message(&req) != Cmd::STATUS::UNKOWN) {
                    // remove cmd.
                    break;
                }
            }
        } else {
            if (status == Codec::STATUS::PAUSE) {
                LOG_DEBUG("decode next time. fd: %d", fd);
                return true;  // not enough data, and decode next time.
            } else {
                LOG_DEBUG("read failed. fd: %d", fd);
                close_conn(c);
                return false;
            }
        }
        LOG_DEBUG("connection is http codec type, status: %d", (int)status);
    } else {
        // LOG_DEBUG("connection is not http codec type, status: %d", (int)status);
    }

    return true;
}

void Network::accept_server_conn(int fd) {
    char cip[NET_IP_STR_LEN];
    int cport, cfd, family, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        cfd = anet_tcp_accept(m_err, fd, cip, sizeof(cip), &cport, &family);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("accepting client connection failed: fd: %d, error %s",
                          fd, m_err);
            }
            return;
        }

        LOG_DEBUG("accepted %s:%d", cip, cport);

        if (!add_read_event(cfd, Codec::TYPE::PROTOBUF)) {
            close_conn(cfd);
            LOG_ERROR("add data fd read event failed, fd: %d", cfd);
            return;
        }
    }
}

// manager accept new fd and transfer which to worker through chanel.
void Network::accept_and_transfer_fd(int fd) {
    int cport, cfd, family;
    char cip[NET_IP_STR_LEN] = {0};

    cfd = anet_tcp_accept(m_err, fd, cip, sizeof(cip), &cport, &family);
    if (cfd == ANET_ERR) {
        if (errno != EWOULDBLOCK) {
            LOG_WARN("accepting client connection: %s", m_err);
        }
        return;
    }

    LOG_DEBUG("accepted: %s:%d", cip, cport);

    int chanel_fd = m_woker_data_mgr->get_next_worker_data_fd();
    if (chanel_fd > 0) {
        LOG_DEBUG("send client fd: %d to worker through chanel fd %d", cfd, chanel_fd);
        channel_t ch = {cfd, family, static_cast<int>(m_gate_codec_type)};
        int err = write_channel(chanel_fd, &ch, sizeof(channel_t), m_logger);
        if (err != 0) {
            LOG_ERROR("write channel failed! errno: %d", err);
        }
    } else {
        LOG_ERROR("find next worker chanel failed!");
    }

    close(cfd);
}

// worker send fd which transfered from manager.
void Network::read_transfer_fd(int fd) {
    channel_t ch;
    Codec::TYPE codec;
    ConnectionData* conn_data = nullptr;
    std::shared_ptr<Connection> c = nullptr;
    int err, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        // read fd from manager.
        err = read_channel(fd, &ch, sizeof(channel_t), m_logger);
        if (err != 0) {
            if (err != EAGAIN) {
                destory();
                LOG_ERROR("read channel failed!, exit!");
                exit(EXIT_FD_TRANSFER);
            }
            return;
        }

        codec = static_cast<Codec::TYPE>(ch.codec);
        c = add_read_event(ch.fd, codec);
        if (c == nullptr) {
            LOG_ERROR("add data fd read event failed, fd: %d", ch.fd);
            goto error;
        }

        // add timer.
        if (codec == Codec::TYPE::HTTP) {
            conn_data = new ConnectionData;
            if (conn_data == nullptr) {
                LOG_ERROR("alloc connection data failed! fd: %d", fd);
                goto error;
            }
            conn_data->m_conn = c;

            ev_timer* w = c->get_ev_timer();
            if (w == nullptr) {
                w = m_events->add_timer_event(1, conn_data);
                if (w == nullptr) {
                    LOG_ERROR("add timer failed! fd: %d", fd);
                    goto error;
                }
            } else {
                if (!m_events->restart_timer(1, w, conn_data)) {
                    LOG_ERROR("restart timer failed! fd: %d", fd);
                    goto error;
                }
            }
            c->set_ev_timer(w);
        }

        LOG_DEBUG("read channel, channel data: fd: %d, family: %d, codec: %d",
                  ch.fd, ch.family, ch.codec);
    }

error:
    close_conn(ch.fd);
    SAFE_FREE(conn_data);
    return;
}

void Network::end_ev_loop() {
    if (m_events != nullptr) {
        m_events->end_ev_loop();
    }
}

bool Network::set_gate_codec_type(Codec::TYPE type) {
    if (type < Codec::TYPE::UNKNOWN ||
        type >= Codec::TYPE::COUNT) {
        return false;
    }
    m_gate_codec_type = type;
    return true;
}

bool Network::load_modules() {
    // core module.
    Module* p = new MoudleCore;
    if (p == nullptr) {
        return false;
    }
    p->init(m_logger, this);
    p->set_name(CORE_MODULE);
    m_core_modules.push_back(p);
    return true;
}

bool Network::send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) {
    if (c->is_closed()) {
        LOG_WARN("connection is closed! fd: %d", c->get_fd());
        return false;
    }

    Codec::STATUS status = c->conn_write(msg);
    if (status == Codec::STATUS::ERR) {
        close_conn(c);
        return false;
    }

    if (status == Codec::STATUS::PAUSE) {
        // add write events.
        return true;
    }

    return true;
}

}  // namespace kim