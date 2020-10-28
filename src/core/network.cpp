#include "network.h"

#include <errno.h>
#include <unistd.h>

#include "error.h"

namespace kim {

Network::Network(Log* logger, TYPE type)
    : m_logger(logger), m_type(type) {
}

Network::~Network() {
    destory();
}

void Network::destory() {
    if (m_events != nullptr) {
        m_events->del_timer_event(m_timer);
        m_timer = nullptr;
    }

    SAFE_DELETE(m_db_pool);
    SAFE_DELETE(m_redis_pool);
    SAFE_DELETE(m_session_mgr);

    close_conns();
    for (const auto& it : m_wait_send_fds) {
        free(it);
    }
    m_wait_send_fds.clear();

    for (const auto& it : m_cmds) {
        delete it.second;
    }
    m_cmds.clear();

    end_ev_loop();
    SAFE_DELETE(m_events);
}

void Network::run() {
    if (m_events != nullptr) {
        m_events->run();
    }
}

double Network::now() {
    if (m_events == nullptr) {
        return 0;
    }
    return m_events->now();
}

Events* Network::events() {
    return m_events;
}

bool Network::load_config(const CJsonObject& config) {
    double secs;
    m_conf = config;
    std::string codec = m_conf("gate_codec");

    if (!set_gate_codec(codec)) {
        LOG_ERROR("invalid codec: %s", codec.c_str());
        return false;
    }
    LOG_DEBUG("gate codec: %s", codec.c_str());

    if (m_conf.Get("keep_alive", secs)) {
        set_keep_alive(secs);
    }

    return true;
}

/* parent. */
bool Network::create(const addr_info* ainfo,
                     INet* net, const CJsonObject& config, WorkerDataMgr* m) {
    int fd = -1;
    if (ainfo == nullptr || net == nullptr || m == nullptr) {
        return false;
    }

    if (!load_config(config)) {
        LOG_ERROR("load config failed!");
        return false;
    }

    if (!ainfo->bind().empty()) {
        fd = listen_to_port(ainfo->bind().c_str(), ainfo->port());
        if (fd == -1) {
            LOG_ERROR("listen to port fail! %s:%d", ainfo->bind().c_str(), ainfo->port());
            return false;
        }
        m_bind_fd = fd;
    }

    if (!ainfo->gate_bind().empty()) {
        fd = listen_to_port(ainfo->gate_bind().c_str(), ainfo->gate_port());
        if (fd == -1) {
            LOG_ERROR("listen to gate fail! %s:%d",
                      ainfo->gate_bind().c_str(), ainfo->gate_port());
            return false;
        }
        m_gate_bind_fd = fd;
    }

    LOG_INFO("bind fd: %d, gate bind fd: %d", m_bind_fd, m_gate_bind_fd);

    if (!create_events(net, m_bind_fd, m_gate_bind_fd, m_gate_codec, false)) {
        LOG_ERROR("create events failed!");
        return false;
    }
    m_woker_data_mgr = m;

    if (!load_timer(net)) {
        LOG_ERROR("load timer failed!");
        return false;
    }

    m_session_mgr = new SessionMgr(m_logger, this);
    if (m_session_mgr == nullptr) {
        LOG_ERROR("alloc session mgr failed!");
        return false;
    }

    return true;
}

/* children. */
bool Network::create(INet* net, const CJsonObject& config, int ctrl_fd, int data_fd) {
    if (!create_events(net, ctrl_fd, data_fd, Codec::TYPE::PROTOBUF, true)) {
        LOG_ERROR("create events failed!");
        return false;
    }
    m_conf = config;
    m_manager_ctrl_fd = ctrl_fd;
    m_manager_data_fd = data_fd;
    LOG_INFO("create network done!");

    if (!load_config(config)) {
        LOG_ERROR("load config failed!");
        return false;
    }

    if (!load_db()) {
        LOG_ERROR("load db failed!");
        return false;
    }

    if (!load_redis_mgr()) {
        LOG_ERROR("load redis mgr failed!");
        return false;
    }

    if (!load_modules()) {
        LOG_ERROR("load module failed!");
        return false;
    }
    LOG_INFO("load modules ok!");

    // if (!load_timer(net)) {
    //     LOG_ERROR("load timer failed!");
    //     return false;
    // }

    m_session_mgr = new SessionMgr(m_logger, this);
    if (m_session_mgr == nullptr) {
        LOG_ERROR("alloc session mgr failed!");
        return false;
    }
    return true;
}

bool Network::create_events(INet* s, int fd1, int fd2,
                            Codec::TYPE codec, bool is_worker) {
    m_events = new Events(m_logger);
    if (m_events == nullptr) {
        LOG_ERROR("new events failed!");
        return false;
    }

    if (!m_events->create()) {
        LOG_ERROR("create events failed!");
        goto error;
    }

    if (!add_read_event(fd1, codec, is_worker) ||
        !add_read_event(fd2, codec, is_worker)) {
        close_conn(fd1);
        close_conn(fd2);
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

std::shared_ptr<Connection>
Network::add_read_event(int fd, Codec::TYPE codec, bool is_chanel) {
    if (anet_no_block(m_errstr, fd) != ANET_OK) {
        LOG_ERROR("set socket no block failed! fd: %d, errstr: %s", fd, m_errstr);
        return nullptr;
    }

    if (!is_chanel) {
        if (anet_keep_alive(m_errstr, fd, 100) != ANET_OK) {
            LOG_ERROR("set socket keep alive failed! fd: %d, errstr: %s", fd, m_errstr);
            return nullptr;
        }
        if (anet_set_tcp_no_delay(m_errstr, fd, 1) != ANET_OK) {
            LOG_ERROR("set socket no delay failed! fd: %d, errstr: %s", fd, m_errstr);
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
    c->init(codec);
    c->set_privdata(this);
    c->set_active_time(now());
    c->set_state(Connection::STATE::CONNECTED);

    w = m_events->add_read_event(fd, c->get_ev_io(), this);
    if (w == nullptr) {
        LOG_ERROR("add read event failed! fd: %d", fd);
        return nullptr;
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

    seq = new_seq();
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
    return close_conn(c->fd());
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
            m_events->del_io_event(c->get_ev_io());
            c->set_ev_io(nullptr);

            ev_timer* w = c->timer();
            if (w != nullptr) {
                delete static_cast<ConnectionData*>(w->data);
                w->data = nullptr;
                m_events->del_timer_event(w);
                c->set_timer(nullptr);
            }
        }
        m_conns.erase(it);
    }

    close_fd(fd);
    LOG_DEBUG("close fd: %d", fd);
    return true;
}

int Network::listen_to_port(const char* bind, int port) {
    int fd = -1;
    char errstr[256];

    if (strchr(bind, ':')) {
        /* Bind IPv6 address. */
        fd = anet_tcp6_server(errstr, port, bind, TCP_BACK_LOG);
        if (fd == -1) {
            LOG_ERROR("bind tcp ipv6 fail! %s", errstr);
        }
    } else {
        /* Bind IPv4 address. */
        fd = anet_tcp_server(errstr, port, bind, TCP_BACK_LOG);
        if (fd == -1) {
            LOG_ERROR("bind tcp ipv4 fail! %s", errstr);
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

    close_fd(fds[0]);
    close_fd(fds[1]);
}

void Network::close_conns() {
    LOG_DEBUG("close_conns(), cnt: %d", m_conns.size());
    for (const auto& it : m_conns) {
        close_conn(it.second);
    }
}

void Network::close_fds() {
    for (const auto& it : m_conns) {
        int fd = it.second->fd();
        if (fd != -1) {
            close_fd(fd);
            it.second->set_fd(-1);
        }
    }
}

void Network::on_io_write(int fd) {
    auto it = m_conns.find(fd);
    if (it == m_conns.end() || it->second == nullptr) {
        LOG_ERROR("find connection failed! fd: %d", fd);
        return;
    }

    std::shared_ptr<Connection> c = it->second;
    if (!c->is_connected() && !c->is_connecting()) {
        LOG_ERROR("invalid connection, fd: %d, id: %llu", fd, c->id());
        close_conn(fd);
        return;
    }

    ev_io* w;
    Codec::STATUS status;

    w = c->get_ev_io();
    status = c->conn_write();
    if (status == Codec::STATUS::OK) {
        m_events->del_write_event(w);
        return;
    }

    if (status == Codec::STATUS::PAUSE) {
        w = m_events->add_write_event(fd, w, this);
        if (w == nullptr) {
            close_conn(c);
            LOG_ERROR("add write event failed! fd: %d", fd);
            return;
        }
        c->set_ev_io(w);
        return;
    }

    // ok remove write event.
    m_events->del_write_event(w);
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
    } else {
        if (fd == m_manager_data_fd) {
            read_transfer_fd(fd);
        } else {
            read_query_from_client(fd);
        }
    }
}

void Network::check_wait_send_fds() {
    auto it = m_wait_send_fds.begin();
    for (; it != m_wait_send_fds.end();) {
        chanel_resend_data_t* data = *it;
        int chanel_fd = m_woker_data_mgr->get_next_worker_data_fd();
        int err = write_channel(chanel_fd, &data->ch, sizeof(channel_t), m_logger);
        if (err == 0 || (err != 0 && err != EAGAIN)) {
            if (err != 0) {
                LOG_ERROR("resend chanel failed! fd: %d, errno: %d", data->ch.fd, err);
            }
            close_fd(data->ch.fd);
            free(data);
            m_wait_send_fds.erase(it++);
            continue;
        }

        //  err == EAGAIN)
        if (++data->count >= 3) {
            LOG_INFO("resend chanel too much! fd: %d, errno: %d", err, data->ch.fd);
            close_fd(data->ch.fd);
            free(data);
            m_wait_send_fds.erase(it++);
            continue;
        }

        it++;
        LOG_DEBUG("wait to write channel, errno: %d", err);
    }
    // LOG_DEBUG("wait send list cnt: %d", m_wait_send_fds.size());
}

void Network::on_repeat_timer(void* privdata) {
    check_wait_send_fds();
    // if (m_module_mgr != nullptr) {
    //     m_module_mgr->reload_so("module_test.so");
    // }
}

void Network::on_cmd_timer(void* privdata) {
    Cmd* cmd;
    double secs;
    Module* module;

    cmd = static_cast<Cmd*>(privdata);
    secs = cmd->keep_alive() - (now() - cmd->active_time());
    if (secs > 0) {
        LOG_DEBUG("cmd timer restart, cmd id: %llu, restart timer secs: %f",
                  cmd->id(), secs);
        m_events->restart_timer(secs, cmd->timer(), privdata);
        return;
    } else {
        module = m_module_mgr->get_module(cmd->module_id());
        if (module == nullptr) {
            LOG_ERROR("find module failed!, module id: %llu", cmd->module_id());
            return;
        }
        /* note: cmd will be deleted, when status != Cmd::STATUS::RUNNING.
         * and the same to cmd's timer.*/
        if (module->on_timeout(cmd) == Cmd::STATUS::RUNNING) {
            LOG_DEBUG("cmd timer reset, cmd id: %llu, restart timer secs: %f",
                      cmd->id(), secs);
            m_events->restart_timer(CMD_TIMEOUT_VAL, cmd->timer(), privdata);
        }
    }
}

void Network::on_io_timer(void* privdata) {
    if (is_worker()) {
        double secs;
        ConnectionData* conn_data;
        std::shared_ptr<Connection> c;

        conn_data = static_cast<ConnectionData*>(privdata);
        c = conn_data->m_conn;
        secs = c->keep_alive() - (now() - c->active_time());
        if (secs > 0) {
            LOG_DEBUG("io timer restart, fd: %d, restart timer secs: %f",
                      c->fd(), secs);
            m_events->restart_timer(secs, c->timer(), privdata);
            return;
        }

        LOG_INFO("time up, close connection! secs: %f, fd: %d, seq: %llu",
                 secs, c->fd(), c->id());
        close_conn(c);
    }
}

bool Network::read_query_from_client(int fd) {
    auto it = m_conns.find(fd);
    if (it == m_conns.end() || it->second == nullptr) {
        LOG_WARN("find connection failed, fd: %d", fd);
        close_conn(fd);
        return false;
    }

    std::shared_ptr<Connection> c = it->second;
    if (c->is_closed()) {
        close_conn(fd);
        return false;
    }

    return process_message(c);
}

bool Network::process_message(std::shared_ptr<Connection>& c) {
    Cmd::STATUS cmd_ret;
    Codec::STATUS status;

    if (c->is_http_codec()) {
        HttpMsg msg;
        status = c->conn_read(msg);
        LOG_DEBUG("connection is http codec type, status: %d", (int)status);
        while (status == Codec::STATUS::OK) {
            cmd_ret = m_module_mgr->process_msg(c, msg);
            LOG_DEBUG("cmd status: %d", cmd_ret);
            msg.Clear();
            status = c->fetch_data(msg);
        }
    } else {
        MsgHead head;
        MsgBody body;
        status = c->conn_read(head, body);
        LOG_DEBUG("connection is protobuf codec type, status: %d", (int)status);
        while (status == Codec::STATUS::OK) {
            cmd_ret = m_module_mgr->process_msg(c, head, body);
            LOG_DEBUG("cmd status: %d", cmd_ret);
            head.Clear();
            body.Clear();
            status = c->fetch_data(head, body);
        }
    }

    if (status == Codec::STATUS::ERR) {
        LOG_DEBUG("read failed. fd: %d", c->fd());
        close_conn(c);
        return false;
    }

    // if (c->keep_alive() == 0.0) {
    //     LOG_DEBUG("short connection! fd: %d", c->fd());
    //     close_conn(c);
    //     return false;
    // }

    return true;
}

void Network::accept_server_conn(int fd) {
    char cip[NET_IP_STR_LEN];
    int cport, cfd, family, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        cfd = anet_tcp_accept(m_errstr, fd, cip, sizeof(cip), &cport, &family);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("accepting client connection failed: fd: %d, errstr %s",
                          fd, m_errstr);
            }
            return;
        }

        LOG_INFO("accepted server %s:%d", cip, cport);

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

    cfd = anet_tcp_accept(m_errstr, fd, cip, sizeof(cip), &cport, &family);
    if (cfd == ANET_ERR) {
        if (errno != EWOULDBLOCK) {
            LOG_WARN("accepting client connection: %s", m_errstr);
        }
        return;
    }

    LOG_INFO("accepted client: %s:%d", cip, cport);

    int chanel_fd = m_woker_data_mgr->get_next_worker_data_fd();
    if (chanel_fd > 0) {
        LOG_DEBUG("send client fd: %d to worker through chanel fd %d", cfd, chanel_fd);
        channel_t ch = {cfd, family, static_cast<int>(m_gate_codec)};
        int err = write_channel(chanel_fd, &ch, sizeof(channel_t), m_logger);
        if (err != 0) {
            if (err == EAGAIN) {
                chanel_resend_data_t* ch_data =
                    (chanel_resend_data_t*)malloc(sizeof(chanel_resend_data_t));
                memset(ch_data, 0, sizeof(chanel_resend_data_t));
                ch_data->ch = ch;
                m_wait_send_fds.push_back(ch_data);
                LOG_DEBUG("wait to write channel, errno: %d", err);
                return;
            }
            LOG_ERROR("write channel failed! errno: %d", err);
        }
    } else {
        LOG_ERROR("find next worker chanel failed!");
    }

    close_fd(cfd);
}

// worker send fd which transfered from manager.
void Network::read_transfer_fd(int fd) {
    channel_t ch;
    Codec::TYPE codec;
    ev_timer* w;
    ConnectionData* conn_data = nullptr;
    std::shared_ptr<Connection> c = nullptr;
    int err, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        // read fd from manager.
        err = read_channel(fd, &ch, sizeof(channel_t), m_logger);
        if (err != 0) {
            if (err == EAGAIN) {
                return;
            } else {
                destory();
                LOG_ERROR("read channel failed!, exit!");
                exit(EXIT_FD_TRANSFER);
            }
        }

        codec = static_cast<Codec::TYPE>(ch.codec);
        c = add_read_event(ch.fd, codec);
        if (c == nullptr) {
            LOG_ERROR("add data fd read event failed, fd: %d", ch.fd);
            goto error;
        }

        conn_data = new ConnectionData;
        if (conn_data == nullptr) {
            LOG_ERROR("alloc connection data failed! fd: %d", ch.fd);
            goto error;
        }
        conn_data->m_conn = c;
        // add timer.
        LOG_DEBUG("add io timer, fd: %d, time val: %f", ch.fd, m_keep_alive);
        w = m_events->add_io_timer(m_keep_alive, c->timer(), conn_data);
        if (w == nullptr) {
            LOG_ERROR("add timer failed! fd: %d", ch.fd);
            goto error;
        }
        c->set_timer(w);

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

bool Network::set_gate_codec(const std::string& codec_type) {
    Codec codec;
    Codec::TYPE type = codec.get_codec_type(codec_type);
    if (type == Codec::TYPE::UNKNOWN) {
        return false;
    }
    m_gate_codec = type;
    return true;
}

bool Network::load_modules() {
    m_module_mgr = new ModuleMgr(new_seq(), m_logger, this);
    if (m_module_mgr == nullptr) {
        LOG_ERROR("alloc module mgr failed!");
        return false;
    }
    return m_module_mgr->init(m_conf);
}

bool Network::load_timer(INet* net) {
    if (m_events == nullptr || net == nullptr) {
        return false;
    }
    m_timer = m_events->add_repeat_timer(REPEAT_TIMEOUT_VAL, m_timer, net);
    return (m_timer != nullptr);
}

bool Network::send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) {
    int fd;
    ev_io* w;
    Codec::STATUS status;

    fd = c->fd();
    if (c->is_closed()) {
        LOG_WARN("connection is closed! fd: %d", fd);
        return false;
    }

    w = c->get_ev_io();
    status = c->conn_write(msg);
    if (status == Codec::STATUS::OK) {
        m_events->del_write_event(w);
        return true;
    }

    if (status == Codec::STATUS::PAUSE) {
        w = m_events->add_write_event(fd, w, this);
        if (w == nullptr) {
            close_conn(c);
            LOG_ERROR("add write event failed! fd: %d", fd);
            return false;
        }
        c->set_ev_io(w);
        return true;
    }

    // ok remove write event.
    m_events->del_write_event(w);
    return true;
}

bool Network::send_to(std::shared_ptr<Connection> c, const MsgHead& head, const MsgBody& body) {
    int fd;
    ev_io* w;
    Codec::STATUS status;

    fd = c->fd();
    if (c->is_closed()) {
        LOG_WARN("connection is closed! fd: %d", fd);
        return false;
    }

    w = c->get_ev_io();
    status = c->conn_write(head, body);
    if (status == Codec::STATUS::OK) {
        m_events->del_write_event(w);
        return true;
    }

    if (status == Codec::STATUS::PAUSE) {
        w = m_events->add_write_event(fd, w, this);
        if (w == nullptr) {
            close_conn(c);
            LOG_ERROR("add write event failed! fd: %d", fd);
            return false;
        }
        c->set_ev_io(w);
        return true;
    }

    // ok remove write event.
    m_events->del_write_event(w);
    return true;
}

ev_timer* Network::add_cmd_timer(double secs, ev_timer* w, void* privdata) {
    return m_events->add_cmd_timer(secs, w, privdata);
}

bool Network::del_cmd_timer(ev_timer* w) {
    return m_events->del_timer_event(w);
}

bool Network::redis_send_to(const char* node, Cmd* cmd, const std::vector<std::string>& argv) {
    if (node == nullptr || cmd == nullptr || argv.size() == 0) {
        LOG_ERROR("invalid params!");
        return false;
    }

    LOG_DEBUG("redis send to node: %s", node);

    wait_cmd_info_t* info;
    uint64_t module_id, cmd_id;

    cmd_id = cmd->id();
    module_id = cmd->module_id();

    // delete info when callback.
    info = new wait_cmd_info_t(this, module_id, cmd_id, cmd->get_exec_step());
    if (info == nullptr) {
        LOG_ERROR("add wait cmd info failed! cmd id: %llu, module id: %llu",
                  cmd_id, module_id);
        return false;
    }

    if (!m_redis_pool->send_to(node, argv, on_redis_lib_callback, info)) {
        LOG_ERROR("redis send data failed! node: %s.", node);
        SAFE_DELETE(info);
        return false;
    }

    return true;
}

bool Network::db_exec(const char* node, const char* sql, Cmd* cmd) {
    if (node == nullptr || sql == nullptr || cmd == nullptr) {
        LOG_ERROR("invalid param!");
        return false;
    }

    LOG_DEBUG("database exec, node: %s, sql: %s", node, sql);

    wait_cmd_info_t* index;
    uint64_t cmd_id, module_id;

    cmd_id = cmd->id();
    module_id = cmd->module_id();
    index = new wait_cmd_info_t(this, module_id, cmd_id, cmd->get_exec_step());
    if (index == nullptr) {
        LOG_ERROR("add wait cmd info failed! cmd id: %llu, module id: %llu",
                  cmd_id, module_id);
        return false;
    }

    if (!m_db_pool->async_exec(node, &on_mysql_lib_exec_callback, sql, index)) {
        SAFE_DELETE(index);
        LOG_ERROR("database query failed! node: %s, sql: %s", node, sql);
        return false;
    }

    return true;
}

bool Network::db_query(const char* node, const char* sql, Cmd* cmd) {
    if (node == nullptr || sql == nullptr || cmd == nullptr) {
        LOG_ERROR("invalid param!");
        return false;
    }

    LOG_DEBUG("database query, node: %s, sql: %s", node, sql);

    wait_cmd_info_t* index;
    uint64_t cmd_id, module_id;

    cmd_id = cmd->id();
    module_id = cmd->module_id();
    index = new wait_cmd_info_t(this, module_id, cmd_id, cmd->get_exec_step());
    if (index == nullptr) {
        LOG_ERROR("add wait cmd info failed! cmd id: %llu, module id: %llu",
                  cmd_id, module_id);
        return false;
    }

    if (!m_db_pool->async_query(node, &on_mysql_lib_query_callback, sql, index)) {
        SAFE_DELETE(index);
        LOG_ERROR("database query failed! node: %s, sql: %s", node, sql);
        return false;
    }

    return true;
}

void Network::on_mysql_lib_query_callback(const MysqlAsyncConn* c, sql_task_t* task, MysqlResult* res) {
    wait_cmd_info_t* index = static_cast<wait_cmd_info_t*>(task->privdata);
    index->net->on_mysql_query_callback(c, task, res);
}

void Network::on_mysql_query_callback(const MysqlAsyncConn* c, sql_task_t* task, MysqlResult* res) {
    Module* module;
    wait_cmd_info_t* index;

    index = static_cast<wait_cmd_info_t*>(task->privdata);
    module = m_module_mgr->get_module(index->module_id);
    if (module == nullptr) {
        LOG_ERROR("find module failed! module id: %llu.", index->module_id);
        SAFE_DELETE(index);
        return;
    }

    if (task->error != ERR_OK) {
        LOG_ERROR("database query failed, error: %d, errstr: %s",
                  task->error, task->errstr.c_str());
    }
    module->on_callback(index, task->error, res);
}

void Network::on_mysql_lib_exec_callback(const MysqlAsyncConn* c, sql_task_t* task) {
    wait_cmd_info_t* index = static_cast<wait_cmd_info_t*>(task->privdata);
    index->net->on_mysql_exec_callback(c, task);
}

void Network::on_mysql_exec_callback(const MysqlAsyncConn* c, sql_task_t* task) {
    Module* module;
    wait_cmd_info_t* index;

    index = static_cast<wait_cmd_info_t*>(task->privdata);
    module = m_module_mgr->get_module(index->module_id);
    if (module == nullptr) {
        LOG_ERROR("find module failed! module id: %llu.", index->module_id);
        SAFE_DELETE(index);
        return;
    }

    if (task->error != ERR_OK) {
        LOG_ERROR("database query failed, error: %d, errstr: %s",
                  task->error, task->errstr.c_str());
    }
    module->on_callback(index, task->error, nullptr);
}

void Network::on_redis_lib_callback(redisAsyncContext* ac, void* reply, void* privdata) {
    wait_cmd_info_t* index = static_cast<wait_cmd_info_t*>(privdata);
    index->net->on_redis_callback(ac, reply, privdata);
}

void Network::on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) {
    LOG_DEBUG("redis callback. host: %s, port: %d.", c->c.tcp.host, c->c.tcp.port);

    Module* module;
    wait_cmd_info_t* info;
    info = static_cast<wait_cmd_info_t*>(privdata);

    module = m_module_mgr->get_module(info->module_id);
    if (module == nullptr) {
        LOG_ERROR("find module failed! module id: %llu.", info->module_id);
        SAFE_DELETE(info);
        return;
    }

    if (reply != nullptr) {
        redisReply* r = (redisReply*)reply;
        if (c->err != 0) {
            LOG_ERROR("redis callback data: %s, err: %d, errstr: %s",
                      r->str, c->err, c->errstr);
        } else {
            LOG_DEBUG("redis callback data: %s, err: %d", r->str, c->err);
        }
    }

    module->on_callback(info, c->err, reply);
    SAFE_DELETE(info);
}

bool Network::add_cmd(Cmd* cmd) {
    if (cmd == nullptr) {
        return false;
    }
    auto it = m_cmds.insert({cmd->id(), cmd});
    if (it.second == false) {
        LOG_ERROR("cmd: %s duplicate!", cmd->name());
        return false;
    }
    return true;
}

Cmd* Network::get_cmd(uint64_t id) {
    auto it = m_cmds.find(id);
    if (it == m_cmds.end() || it->second == nullptr) {
        LOG_WARN("find cmd failed! seq: %llu", id);
        return nullptr;
    }
    return it->second;
}

bool Network::del_cmd(Cmd* cmd) {
    if (cmd == nullptr) {
        return false;
    }

    auto it = m_cmds.find(cmd->id());
    if (it == m_cmds.end()) {
        return false;
    }

    m_cmds.erase(it);
    if (cmd->timer() != nullptr) {
        LOG_DEBUG("del timer: %p!", cmd->timer());
        del_cmd_timer(cmd->timer());
        cmd->set_timer(nullptr);
    }

    LOG_DEBUG("delete cmd: %p!", cmd);
    SAFE_DELETE(cmd);
    return true;
}

void Network::close_fd(int fd) {
    if (close(fd) == -1) {
        LOG_WARN("close channel failed, fd: %d. errno: %d, errstr: %s",
                 fd, errno, strerror(errno));
    }
    LOG_DEBUG("close fd: %d.", fd);
}

bool Network::load_db() {
    SAFE_DELETE(m_db_pool);
    m_db_pool = new DBMgr(m_logger, m_events->ev_loop());
    if (m_db_pool == nullptr) {
        LOG_ERROR("load db pool failed!");
        return false;
    }
    if (!m_db_pool->init(m_conf["database"])) {
        LOG_ERROR("init db pool failed!");
        SAFE_DELETE(m_db_pool);
        return false;
    }
    return true;
}

bool Network::load_redis_mgr() {
    SAFE_DELETE(m_redis_pool);
    m_redis_pool = new kim::RedisMgr(m_logger, m_events->ev_loop());
    if (m_redis_pool == nullptr || !m_redis_pool->init(m_conf["redis"])) {
        LOG_ERROR("init redis mgr failed!");
        return false;
    }
    return true;
}

bool Network::add_session(Session* s) {
    return m_session_mgr->add_session(s);
}

Session* Network::get_session(const std::string& sessid, bool re_active) {
    return m_session_mgr->get_session(sessid, re_active);
}

bool Network::del_session(const std::string& sessid) {
    return m_session_mgr->del_session(sessid);
}

void Network::on_session_timer(void* privdata) {
    m_session_mgr->on_session_timer(privdata);
}

}  // namespace kim