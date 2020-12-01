#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>

#include "error.h"
#include "protobuf/sys/nodes.pb.h"

namespace kim {

Network::Network(Log* logger, TYPE type)
    : EventsCallback(logger, this), m_type(type) {
}

Network::~Network() {
    destory();
}

void Network::destory() {
    end_ev_loop();
    close_conns();

    for (const auto& it : m_wait_send_fds) free(it);
    m_wait_send_fds.clear();

    for (const auto& it : m_cmds) delete it.second;
    m_cmds.clear();

    SAFE_DELETE(m_zk_client);
    SAFE_DELETE(m_db_pool);
    SAFE_DELETE(m_redis_pool);
    SAFE_DELETE(m_session_mgr);
    SAFE_DELETE(m_events);
    SAFE_DELETE(m_sys_cmd);
}

void Network::run() {
    if (m_events != nullptr) {
        m_events->run();
    }
}

double Network::now() {
    return (m_events != nullptr) ? m_events->now() : 0;
}

Events* Network::events() {
    return m_events;
}

bool Network::load_config(const CJsonObject& config) {
    double secs;
    m_conf = config;
    std::string codec;

    codec = m_conf("gate_codec");
    if (!codec.empty()) {
        if (!set_gate_codec(codec)) {
            LOG_ERROR("invalid codec: %s", codec.c_str());
            return false;
        }
        LOG_DEBUG("gate codec: %s", codec.c_str());
    }

    if (m_conf.Get("keep_alive", secs)) {
        set_keep_alive(secs);
    }

    m_node_type = m_conf("node_type");
    if (m_node_type.empty()) {
        LOG_ERROR("invalid inner node info!");
        return false;
    }

    return true;
}

bool Network::load_zk_mgr() {
    m_zk_client = new ZkClient(m_logger, this);
    if (m_zk_client == nullptr) {
        LOG_ERROR("new zk mgr failed!");
        return false;
    }

    if (m_zk_client->init(m_conf)) {
        LOG_INFO("load zk client done!");
    }
    return true;
}

/* parent. */
bool Network::create_m(const addr_info* ai, const CJsonObject& config) {
    int fd = -1;
    if (ai == nullptr) {
        return false;
    }

    if (!load_public(config)) {
        LOG_ERROR("load public failed!");
        return false;
    }

    if (!load_worker_data_mgr()) {
        LOG_ERROR("new worker data mgr failed!");
        return false;
    }

    if (!load_zk_mgr()) {
        LOG_ERROR("load zookeeper mgr failed!");
        return false;
    }

    if (!ai->node_host().empty()) {
        fd = listen_to_port(ai->node_host().c_str(), ai->node_port());
        if (fd == -1) {
            LOG_ERROR("listen to port failed! %s:%d",
                      ai->node_host().c_str(), ai->node_port());
            return false;
        }

        m_node_host_fd = fd;
        m_node_host = ai->node_host();
        m_node_port = ai->node_port();
        LOG_INFO("node fd: %d", m_node_host_fd);

        if (!add_read_event(m_node_host_fd, Codec::TYPE::PROTOBUF, false)) {
            close_fd(m_node_host_fd);
            LOG_ERROR("add read event failed, fd: %d", m_node_host_fd);
            return false;
        }
    }

    if (!ai->gate_host().empty()) {
        fd = listen_to_port(ai->gate_host().c_str(), ai->gate_port());
        if (fd == -1) {
            LOG_ERROR("listen to gate failed! %s:%d",
                      ai->gate_host().c_str(), ai->gate_port());
            return false;
        }

        m_gate_host_fd = fd;
        m_gate_host = ai->gate_host();
        m_gate_port = ai->gate_port();
        LOG_INFO("gate fd: %d", m_gate_host_fd);

        if (!add_read_event(m_gate_host_fd, m_gate_codec, false)) {
            close_fd(m_gate_host_fd);
            LOG_ERROR("add read event failed, fd: %d", m_gate_host_fd);
            return false;
        }
    }

    return true;
}

/* children. */
bool Network::create_w(const CJsonObject& config, int ctrl_fd, int data_fd, int index) {
    if (!load_public(config)) {
        LOG_ERROR("load public failed!");
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

    if (!add_read_event(ctrl_fd, Codec::TYPE::PROTOBUF, true)) {
        close_fd(ctrl_fd);
        LOG_ERROR("add read event failed, fd: %d", ctrl_fd);
        return false;
    }

    if (!add_read_event(data_fd, Codec::TYPE::PROTOBUF, true)) {
        close_fd(data_fd);
        LOG_ERROR("add read event failed, fd: %d", data_fd);
        return false;
    }

    m_conf = config;
    m_manager_ctrl_fd = ctrl_fd;
    m_manager_data_fd = data_fd;
    m_worker_index = index;
    LOG_INFO("create network done!");
    return true;
}

bool Network::load_public(const CJsonObject& config) {
    if (!load_config(config)) {
        LOG_ERROR("load config failed!");
        return false;
    }

    if (!create_events()) {
        LOG_ERROR("create events failed!");
        return false;
    }

    m_sys_cmd = new SysCmd(m_logger, this);
    if (m_sys_cmd == nullptr) {
        LOG_ERROR("alloc sys cmd failed!");
        return false;
    }

    m_session_mgr = new SessionMgr(m_logger, this);
    if (m_session_mgr == nullptr) {
        LOG_ERROR("alloc session mgr failed!");
        return false;
    }

    m_nodes = new Nodes(m_logger);
    if (m_nodes == nullptr) {
        LOG_ERROR("alloc nodes failed!");
        return false;
    }

    return true;
}

bool Network::create_events() {
    m_events = new Events(m_logger);
    if (m_events == nullptr) {
        LOG_ERROR("new events failed!");
        return false;
    }

    if (!m_events->create()) {
        LOG_ERROR("create events failed!");
        goto error;
    }

    if (!EventsCallback::init(m_logger, this)) {
        LOG_ERROR("init events callback failed!");
        goto error;
    }

    setup_io_callback();
    setup_io_timer_callback();
    setup_cmd_timer_callback();
    setup_session_timer_callback();
    return true;

error:
    SAFE_DELETE(m_events);
    return false;
}

Connection* Network::add_read_event(int fd, Codec::TYPE codec, bool is_chanel) {
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
    Connection* c;

    c = create_conn(fd);
    if (c == nullptr) {
        close_fd(fd);
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

    LOG_TRACE("add read event done! fd: %d", fd);
    return c;
}

Connection* Network::create_conn(int fd) {
    auto it = m_conns.find(fd);
    if (it != m_conns.end()) {
        LOG_WARN("find old connection, fd: %d", fd);
        close_conn(fd);
    }

    uint64_t seq;
    Connection* c;

    seq = new_seq();
    c = new Connection(m_logger, fd, seq);
    if (c == nullptr) {
        LOG_ERROR("new connection failed! fd: %d", fd);
        return nullptr;
    }

    c->set_events(m_events);
    c->set_keep_alive(m_keep_alive);
    m_conns[fd] = c;
    LOG_DEBUG("create connection fd: %d, seq: %llu", fd, seq);
    return c;
}

bool Network::close_conn(Connection* c) {
    if (c == nullptr) {
        return false;
    }
    return close_conn(c->fd());
}

bool Network::close_conn(int fd) {
    LOG_TRACE("close conn, fd: %d", fd);

    if (fd == -1) {
        LOG_ERROR("invalid fd: %d", fd);
        return false;
    }

    auto it = m_conns.find(fd);
    if (it == m_conns.end()) {
        return false;
    }

    Connection* c = it->second;
    c->set_state(Connection::STATE::CLOSED);
    m_events->del_io_event(c->get_ev_io());
    c->set_ev_io(nullptr);

    ev_timer* w = c->timer();
    if (w != nullptr) {
        w->data = nullptr;
        m_events->del_timer_event(w);
        c->set_timer(nullptr);
    }

    if (!c->get_node_id().empty()) {
        m_node_conns.erase(c->get_node_id());
    }

    close_fd(fd);
    SAFE_DELETE(c);
    m_conns.erase(it);
    return true;
}

int Network::listen_to_port(const char* ip, int port) {
    int fd = -1;
    char errstr[256];

    if (strchr(ip, ':')) {
        /* Bind IPv6 address. */
        fd = anet_tcp6_server(errstr, port, ip, TCP_BACK_LOG);
        if (fd == -1) {
            LOG_ERROR("bind tcp ipv6 failed! %s", errstr);
        }
    } else {
        /* Bind IPv4 address. */
        fd = anet_tcp_server(errstr, port, ip, TCP_BACK_LOG);
        if (fd == -1) {
            LOG_ERROR("bind tcp ipv4 failed! %s", errstr);
        }
    }

    if (fd != -1) {
        anet_no_block(NULL, fd);
    }

    LOG_INFO("listen to port, %s:%d", ip, port);
    return fd;
}

void Network::close_chanel(int* fds) {
    LOG_DEBUG("close chanel, fd0: %d, fd1: %d", fds[0], fds[1]);
    close_fd(fds[0]);
    close_fd(fds[1]);
}

void Network::close_conns() {
    LOG_TRACE("close_conns(), cnt: %d", m_conns.size());
    for (const auto& it : m_conns) {
        close_conn(it.second);
    }
}

void Network::close_fds() {
    Connection* c;
    for (const auto& it : m_conns) {
        c = it.second;
        if (!c->is_invalid()) {
            close_conn(c);
        }
    }
}

void Network::on_io_read(int fd) {
    if (is_manager()) {
        if (fd == m_node_host_fd) {
            accept_server_conn(fd);
        } else if (fd == m_gate_host_fd) {
            accept_and_transfer_fd(fd);
        } else {
            read_query_from_client(fd);
        }
    } else {
        if (fd == m_manager_data_fd) {
            LOG_TRACE("on io read manager data fd: %d", fd);
            read_transfer_fd(fd);
        } else {
            read_query_from_client(fd);
        }
    }
}

void Network::on_io_write(int fd) {
    auto it = m_conns.find(fd);
    if (it == m_conns.end() || it->second == nullptr) {
        LOG_ERROR("find connection failed! fd: %d", fd);
        return;
    }

    Connection* c = it->second;
    if (c->is_invalid()) {
        LOG_WARN("invalid conn in write event! fd: %d", c->fd());
        close_conn(c);
        return;
    }

    if (c->is_connected() || c->is_connecting()) {
        if (!handle_write_events(c)) {
            close_conn(c);
            LOG_WARN("handle write event failed! fd: %d", c->fd());
        }
    } else if (c->state() == Connection::STATE::TRY_CONNECT) {
        /* A1 contact with B1. */
        if (!m_sys_cmd->send_req_connect_to_worker(c)) {
            LOG_ERROR("send CMD_REQ_CONNECT_TO_WORKER failed! fd: %d", c->fd());
            close_conn(c);
        }
    } else {
        /* ... */
    }
}

void Network::on_repeat_timer(void* privdata) {
    if (is_manager()) {
        check_wait_send_fds();
        if (m_zk_client != nullptr) {
            m_zk_client->on_repeat_timer();
        }
        report_payload_to_zookeeper();
    } else {
        /* send payload info to parent. */
        report_payload_to_parent();
    }

    if (m_sys_cmd != nullptr) {
        m_sys_cmd->on_repeat_timer();
    }
}

bool Network::report_payload_to_parent() {
    if (!is_worker()) {
        LOG_ERROR("report payload to manager, only for worker!");
        return false;
    }

    m_payload.set_worker_index(worker_index());
    m_payload.set_cmd_cnt(m_cmds.size());
    m_payload.set_conn_cnt(m_conns.size() + m_node_conns.size());
    m_payload.set_create_time(now());

    if (!m_sys_cmd->send_parent_payload(m_payload)) {
        m_payload.Clear();
        return false;
    } else {
        m_payload.Clear();
        return true;
    }
}

bool Network::report_payload_to_zookeeper() {
    if (!is_manager()) {
        LOG_ERROR("report payload to zk, only for manager!");
        return false;
    }

    NodeData* node;
    PayloadStats pls;
    Payload *manager_pls, *worker_pls;
    std::string json_data;
    int cmd_cnt = 0, conn_cnt = 0, read_cnt = 0, write_cnt = 0,
        read_bytes = 0, write_bytes = 0;
    const std::unordered_map<int, worker_info_t*>& infos =
        m_worker_data_mgr->get_infos();

    /* node info. */
    node = pls.mutable_node();
    node->set_zk_path(m_nodes->get_my_zk_node_path());
    node->set_node_type(node_type());
    node->set_node_host(node_host());
    node->set_node_port(node_port());
    node->set_gate_host(m_gate_host);
    node->set_gate_port(m_gate_port);
    node->set_worker_cnt(infos.size());

    /* worker payload infos. */
    for (const auto& it : infos) {
        cmd_cnt += it.second->payload.cmd_cnt();
        conn_cnt += it.second->payload.conn_cnt();
        read_cnt += it.second->payload.read_cnt();
        read_bytes += it.second->payload.read_bytes();
        write_cnt += it.second->payload.write_cnt();
        write_bytes += it.second->payload.write_bytes();
        worker_pls = pls.add_workers();
        *worker_pls = it.second->payload;
        if (it.second->payload.worker_index() == 0) {
            worker_pls->set_worker_index(it.second->index);
        }
    }

    /* manager statistics data results。 */
    manager_pls = pls.mutable_manager();
    manager_pls->set_worker_index(worker_index());
    manager_pls->set_cmd_cnt(cmd_cnt);
    manager_pls->set_conn_cnt(conn_cnt + m_conns.size());
    manager_pls->set_read_cnt(read_cnt + m_payload.read_cnt());
    manager_pls->set_read_bytes(read_bytes + m_payload.read_bytes());
    manager_pls->set_write_cnt(write_cnt + m_payload.write_cnt());
    manager_pls->set_write_bytes(write_bytes + m_payload.write_bytes());
    manager_pls->set_create_time(now());

    m_payload.Clear();

    if (!proto_to_json(pls, json_data)) {
        LOG_TRACE("proto to json failed!");
        return false;
    }

    // LOG_TRACE("data: %s", json_data.c_str());
    if (m_zk_client != nullptr) {
        m_zk_client->set_payload_data(json_data);
    }
    return true;
}

void Network::check_wait_send_fds() {
    int err;
    int chanel_fd;
    chanel_resend_data_t* data;

    for (auto it = m_wait_send_fds.begin(); it != m_wait_send_fds.end();) {
        data = *it;
        chanel_fd = m_worker_data_mgr->get_next_worker_data_fd();
        if (chanel_fd <= 0) {
            LOG_ERROR("can not find next worker chanel!");
            return;
        }

        err = write_channel(chanel_fd, &data->ch, sizeof(channel_t), m_logger);
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

void Network::on_cmd_timer(void* privdata) {
    int old;
    Cmd* cmd;
    double secs;
    Connection* c;
    Cmd::STATUS status;

    cmd = static_cast<Cmd*>(privdata);
    secs = cmd->keep_alive() - (now() - cmd->active_time());
    if (secs > 0) {
        LOG_TRACE("cmd timer restart, cmd id: %llu, restart timer secs: %f",
                  cmd->id(), secs);
        m_events->restart_timer(secs, cmd->timer(), privdata);
        return;
    }

    /* timeup, cmd run again? */
    old = cmd->cur_timeout_cnt();
    status = cmd->on_timeout();
    if (status != Cmd::STATUS::RUNNING) {
        del_cmd(cmd);
        return;
    }

    /* status == Cmd::STATUS::RUNNING */
    c = get_conn(cmd->req()->fd_data());
    if (c == nullptr || c->is_invalid()) {
        LOG_DEBUG("connection is closed, stop timeout!");
        del_cmd(cmd);
        return;
    }

    if (old == cmd->cur_timeout_cnt()) {
        cmd->refresh_cur_timeout_cnt();
    }

    if (cmd->cur_timeout_cnt() >= cmd->max_timeout_cnt()) {
        LOG_WARN("pls check timeout logic! %s", cmd->name());
        del_cmd(cmd);
        return;
    }

    LOG_TRACE("reset cmd timer, cmd id: %llu, restart timer secs: %f", cmd->id(), secs);
    m_events->restart_timer(CMD_TIMEOUT_VAL, cmd->timer(), privdata);
}

void Network::on_io_timer(void* privdata) {
    if (is_worker()) {
        double secs;
        Connection* c;

        c = static_cast<Connection*>(privdata);
        secs = c->keep_alive() - (now() - c->active_time());
        if (secs > 0) {
            LOG_TRACE("io timer restart, fd: %d, restart timer secs: %f",
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

    Connection* c = it->second;
    if (c->is_invalid()) {
        close_conn(fd);
        return false;
    }

    return process_msg(c);
}

ev_io* Network::add_write_event(Connection* c) {
    ev_io* w = m_events->add_write_event(c->fd(), c->get_ev_io(), this);
    if (w != nullptr) {
        c->set_ev_io(w);
    }
    return w;
}

bool Network::process_msg(Connection* c) {
    return (c->is_http()) ? process_http_msg(c) : process_tcp_msg(c);
}

bool Network::process_tcp_msg(Connection* c) {
    int fd;
    uint32_t old_cnt, old_bytes;
    Cmd::STATUS cmd_ret;
    Codec::STATUS codec_ret;
    Request req(c->fd_data(), false);

    fd = c->fd();
    old_cnt = c->read_cnt();
    old_bytes = c->read_bytes();

    codec_ret = c->conn_read(*req.msg_head(), *req.msg_body());
    if (codec_ret != Codec::STATUS::ERR) {
        m_payload.set_read_cnt(m_payload.read_cnt() + (c->read_cnt() - old_cnt));
        m_payload.set_read_bytes(m_payload.read_bytes() + (c->read_bytes() - old_bytes));
    }

    LOG_TRACE("conn read result, fd: %d, ret: %d", fd, (int)codec_ret);

    while (codec_ret == Codec::STATUS::OK) {
        cmd_ret = m_sys_cmd->process(req);
        if (cmd_ret == Cmd::STATUS::RUNNING) {
            /* send waiting buffer. */
            if (add_write_event(c) == nullptr) {
                LOG_ERROR("add write event failed! fd: %d", fd);
                goto error;
            }
        } else if (cmd_ret == Cmd::STATUS::COMPLETED) {
            close_conn(c);
            return true;
        } else if (cmd_ret == Cmd::STATUS::UNKOWN) {
            if (is_request(req.msg_head()->cmd())) {
                cmd_ret = m_module_mgr->process_req(req);
            } else {
                cmd_ret = m_module_mgr->process_ack(req);
            }
        }

        if (cmd_ret == Cmd::STATUS::UNKOWN) {
            LOG_WARN("can not find cmd handler. fd: %d", fd)
            goto error;
        } else if (cmd_ret == Cmd::STATUS::ERROR) {
            LOG_TRACE("process tcp msg failed! fd: %d", fd);
            goto error;
        } else {
            req.msg_head()->Clear();
            req.msg_body()->Clear();
            codec_ret = c->fetch_data(*req.msg_head(), *req.msg_body());
            if (codec_ret == Codec::STATUS::ERR) {
                LOG_TRACE("conn read failed. fd: %d", fd);
                goto error;
            }
        }
    }

    if (codec_ret == Codec::STATUS::ERR) {
        LOG_TRACE("conn read failed. fd: %d", fd);
        goto error;
    }

    return true;

error:
    close_conn(c);
    return false;
}

bool Network::process_http_msg(Connection* c) {
    HttpMsg msg;
    int old_cnt, old_bytes;
    Cmd::STATUS cmd_ret;
    Codec::STATUS codec_ret;
    Request req(c->fd_data(), true);

    old_cnt = c->read_cnt();
    old_bytes = c->read_bytes();

    codec_ret = c->conn_read(*req.http_msg());
    if (codec_ret != Codec::STATUS::ERR) {
        m_payload.set_read_cnt(m_payload.read_cnt() + (c->read_cnt() - old_cnt));
        m_payload.set_read_bytes(m_payload.read_bytes() + (c->read_bytes() - old_bytes));
    }

    LOG_TRACE("connection is http, read ret: %d", (int)codec_ret);

    while (codec_ret == Codec::STATUS::OK) {
        cmd_ret = m_module_mgr->process_req(req);
        msg.Clear();
        codec_ret = c->fetch_data(*req.http_msg());
        LOG_TRACE("cmd status: %d", cmd_ret);
    }

    if (codec_ret == Codec::STATUS::ERR) {
        LOG_TRACE("conn read failed. fd: %d", c->fd());
        close_conn(c);
        return false;
    }

    return true;
}

void Network::accept_server_conn(int listen_fd) {
    char ip[NET_IP_STR_LEN];
    int fd, port, family, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        fd = anet_tcp_accept(m_errstr, listen_fd, ip, sizeof(ip), &port, &family);
        if (fd == ANET_ERR) {
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("accepting client connection failed: fd: %d, errstr %s",
                          listen_fd, m_errstr);
            }
            return;
        }

        LOG_INFO("accepted server %s:%d", ip, port);

        if (!add_read_event(fd, Codec::TYPE::PROTOBUF)) {
            close_conn(fd);
            LOG_ERROR("add read event failed, client fd: %d", fd);
            return;
        }
    }
}

/* manager accept new fd and transfer which to worker through chanel.*/
void Network::accept_and_transfer_fd(int listen_fd) {
    channel_t ch;
    chanel_resend_data_t* ch_data;
    char ip[NET_IP_STR_LEN] = {0};
    int fd, port, family, chanel_fd, err;

    fd = anet_tcp_accept(m_errstr, listen_fd, ip, sizeof(ip), &port, &family);
    if (fd == ANET_ERR) {
        if (errno != EWOULDBLOCK) {
            LOG_WARN("accepting client connection: %s", m_errstr);
        }
        return;
    }

    LOG_INFO("accepted client: %s:%d, fd: %d", ip, port, fd);

    chanel_fd = m_worker_data_mgr->get_next_worker_data_fd();
    if (chanel_fd <= 0) {
        LOG_ERROR("find next worker chanel failed!");
        goto end;
    }

    LOG_TRACE("send client fd: %d to worker through chanel fd %d", fd, chanel_fd);

    ch = {fd, family, static_cast<int>(m_gate_codec)};
    err = write_channel(chanel_fd, &ch, sizeof(channel_t), m_logger);
    if (err != 0) {
        if (err == EAGAIN) {
            ch_data = (chanel_resend_data_t*)malloc(sizeof(chanel_resend_data_t));
            memset(ch_data, 0, sizeof(chanel_resend_data_t));
            ch_data->ch = ch;
            m_wait_send_fds.push_back(ch_data);
            LOG_TRACE("wait to write channel, errno: %d", err);
            return;
        }
        LOG_ERROR("write channel failed! errno: %d", err);
    }

end:
    close_fd(fd);
}

// worker send fd which transfered from manager.
void Network::read_transfer_fd(int fd) {
    ev_timer* w;
    channel_t ch;
    Connection* c;
    Codec::TYPE codec;
    int err, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        // read fd from manager.
        err = read_channel(fd, &ch, sizeof(channel_t), m_logger);
        if (err != 0) {
            if (err == EAGAIN) {
                LOG_TRACE("read channel again next time! channel fd: %d", fd);
                return;
            } else {
                destory();
                LOG_ERROR("read channel failed, exit! channel fd: %d", fd);
                exit(EXIT_FD_TRANSFER);
            }
        }

        LOG_TRACE("read from channel, get data: fd: %d, family: %d, codec: %d",
                  ch.fd, ch.family, ch.codec);

        codec = static_cast<Codec::TYPE>(ch.codec);
        c = add_read_event(ch.fd, codec);
        if (c == nullptr) {
            LOG_ERROR("add data fd read event failed, fd: %d", ch.fd);
            goto error;
        }

        // add timer.
        LOG_TRACE("add io timer, fd: %d, time val: %f", ch.fd, m_keep_alive);
        w = m_events->add_io_timer(m_keep_alive, c->timer(), c);
        if (w == nullptr) {
            LOG_ERROR("add timer failed! fd: %d", ch.fd);
            goto error;
        }

        c->set_timer(w);
    }

    return;

error:
    close_conn(ch.fd);
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

bool Network::send_to_node(const std::string& node_type, const std::string& obj,
                           const MsgHead& head, const MsgBody& body) {
    if (is_manager()) {
        LOG_ERROR("send_to_node only for worker!");
        return false;
    }

    node_t* node;
    Connection* c;
    std::string node_id;

    node = m_nodes->get_node_in_hash(node_type, obj);
    if (node == nullptr) {
        LOG_ERROR("cant not find node type: %s", node_type.c_str());
        return false;
    }

    node_id = format_nodes_id(node->host, node->port, node->worker_index);
    auto it = m_node_conns.find(node_id);
    if (it == m_node_conns.end()) {
        return auto_send(node->host, node->port, node->worker_index, head, body);
    }

    c = it->second;
    if (c->is_connected()) {
        return send_to(c, head, body);
    } else if (c->is_connecting() || c->is_try_connect()) {
        /* if TRY_CONNECT / CONNECTING, write in waitting buffer. */
        return (c->conn_write_waiting(head, body) != Codec::STATUS::ERR);
    } else {
        LOG_ERROR("send to node failed! conn status: %d, fd: %d",
                  (int)c->state(), c->fd());
        return false;
    }
}

bool Network::send_to_children(int cmd, uint64_t seq, const std::string& data) {
    LOG_TRACE("send to children, cmd: %d, seq: %llu", cmd, seq);

    if (!is_manager()) {
        LOG_ERROR("send_to_children only for manager!");
        return false;
    }

    /* Parent and child processes communicate through socketpair. */
    const std::unordered_map<int, worker_info_t*>& infos =
        m_worker_data_mgr->get_infos();

    for (auto& v : infos) {
        auto it = m_conns.find(v.second->ctrl_fd);
        if (it == m_conns.end() || it->second->is_invalid()) {
            LOG_ALERT("ctrl fd is invalid! fd: %d", v.second->ctrl_fd);
            continue;
        }

        if (!send_req(it->second, cmd, seq, data)) {
            LOG_ALERT("send to child failed! fd: %d", v.second->ctrl_fd);
            continue;
        }
    }

    return true;
}

bool Network::send_to_parent(int cmd, uint64_t seq, const std::string& data) {
    if (!is_worker()) {
        LOG_ERROR("send_to_parent only for worker!");
        return false;
    }

    auto it = m_conns.find(m_manager_ctrl_fd);
    if (it == m_conns.end()) {
        LOG_ERROR("can not find manager ctrl fd, fd: %d", m_manager_ctrl_fd);
        return false;
    }

    if (!send_req(it->second, cmd, seq, data)) {
        LOG_ALERT("send to parent failed! fd: %d", m_manager_ctrl_fd);
        return false;
    }
    return true;
}

/* auto_send(...)
 * A1 contact with B1. (auto_send func)
 * 
 * A1: node A's worker.
 * B0: node B's manager.
 * B1: node B's worker.
 * 
 * process_sys_message(.)
 * 1. A1 connect to B0. (inner host : inner port)
 * 2. A1 send CMD_REQ_CONNECT_TO_WORKER to B0.
 * 3. B0 send CMD_RSP_CONNECT_TO_WORKER to A1.
 * 4. B0 transfer A1's fd to B1.
 * 5. A1 send CMD_REQ_TELL_WORKER to B1.
 * 6. B1 send CMD_RSP_TELL_WORKER A1.
 * 7. A1 send waiting buffer to B1.
 * 8. B1 send ack to A1.
 */
bool Network::auto_send(const std::string& host, int port, int worker_index,
                        const MsgHead& head, const MsgBody& body) {
    LOG_TRACE("auto send to node, ip: %s, port: %d, worker: %d, msg cmd: %d, msg seq: %d",
              host.c_str(), port, worker_index, head.cmd(), head.seq());

    int fd;
    ev_io* w;
    // size_t saddr_len;
    // struct sockaddr saddr;
    struct sockaddr_in saddr;
    std::string node_id;
    Connection* c;

    /* create socket. */
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(host.c_str());
    bzero(&(saddr.sin_zero), 8);
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    /* connect. */
    // fd = anet_tcp_connect(m_errstr, host.c_str(), port, false, &saddr, &saddr_len);
    if (fd == -1) {
        LOG_ERROR("client connect server failed! errstr: %s", m_errstr);
        return false;
    }

    /* connection. */
    c = create_conn(fd);
    if (c == nullptr) {
        close_fd(fd);
        LOG_ERROR("create conn failed! fd: %d", fd);
        return false;
    }

    if (anet_no_block(m_errstr, fd) != ANET_OK) {
        LOG_ERROR("set noblock failed! fd: %d, errstr: %s", fd, m_errstr);
        goto error;
    }

    if (anet_keep_alive(m_errstr, fd, 100) != ANET_OK) {
        LOG_ERROR("set socket keep alive failed! fd: %d, errstr: %s", fd, m_errstr);
        goto error;
    }

    if (anet_set_tcp_no_delay(m_errstr, fd, 1) != ANET_OK) {
        LOG_ERROR("set socket no delay failed! fd: %d, errstr: %s", fd, m_errstr);
        goto error;
    }

    c->init(Codec::TYPE::PROTOBUF);
    c->set_privdata(this);
    c->set_active_time(now());
    // c->set_addr_info(&saddr, saddr_len);

    /* read event. */
    w = m_events->add_read_event(fd, c->get_ev_io(), this);
    if (w == nullptr) {
        LOG_ERROR("add read event failed! fd: %d", fd);
        goto error;
    }

    /* write event. */
    w = m_events->add_write_event(fd, w, this);
    if (w == nullptr) {
        LOG_ERROR("add write event failed! fd: %d", fd);
        goto error;
    }

    c->set_ev_io(w);
    c->set_state(Connection::STATE::TRY_CONNECT);

    /* add waiting data wait to write. */
    if (c->conn_write_waiting(head, body) == Codec::STATUS::ERR) {
        LOG_ERROR("write waiting data failed! fd: %d", fd);
        goto error;
    }

    /* set timeout for connection. */
    if (!add_io_timer(c, 1.5)) {
        LOG_ERROR("add io timer failed! fd: %d", fd);
        goto error;
    }

    /* A1 connect to B1, and save B1's connection. */
    node_id = format_nodes_id(host, port, worker_index);
    m_node_conns[node_id] = c;
    c->set_node_id(node_id);
    connect(fd, (struct sockaddr*)&saddr, sizeof(struct sockaddr));
    return true;

error:
    close_conn(fd);
    return false;
}

bool Network::send_to(Connection* c, const HttpMsg& msg) {
    if (c == nullptr) {
        return false;
    }

    if (!handle_write_events(c, msg)) {
        close_conn(c);
        LOG_WARN("handle write event failed! fd: %d", c->fd());
        return false;
    }

    return true;
}

bool Network::send_to(Connection* c, const MsgHead& head, const MsgBody& body) {
    if (c == nullptr) {
        return false;
    }

    if (!handle_write_events(c, head, body)) {
        close_conn(c);
        LOG_WARN("handle write event failed! fd: %d", c->fd());
        return false;
    }

    return true;
}

Connection* Network::get_conn(const fd_t& f) {
    auto it = m_conns.find(f.fd);
    return (it == m_conns.end() || it->second->id() != f.id) ? nullptr : it->second;
}

bool Network::send_to(const fd_t& f, const HttpMsg& msg) {
    return send_to(get_conn(f), msg);
}

bool Network::send_to(const fd_t& f, const MsgHead& head, const MsgBody& body) {
    return send_to(get_conn(f), head, body);
}

bool Network::send_req(Connection* c, uint32_t cmd, uint32_t seq,
                       const std::string& data) {
    if (c == nullptr) {
        LOG_DEBUG("invalid connection!");
        return false;
    }

    MsgHead head;
    MsgBody body;
    body.set_data(data);
    head.set_cmd(cmd);
    head.set_seq(seq);
    head.set_len(body.ByteSizeLong());
    return send_to(c, head, body);
}

bool Network::send_req(const fd_t& f, uint32_t cmd,
                       uint32_t seq, const std::string& data) {
    MsgHead head;
    MsgBody body;
    body.set_data(data);
    head.set_seq(seq);
    head.set_cmd(cmd);
    head.set_len(body.ByteSizeLong());
    return send_to(f, head, body);
}

bool Network::send_ack(const Request& req, int err,
                       const std::string& errstr, const std::string& data) {
    MsgHead head;
    MsgBody body;

    body.set_data(data);
    body.mutable_rsp_result()->set_code(err);
    body.mutable_rsp_result()->set_msg(errstr);

    head.set_seq(req.msg_head()->seq());
    head.set_cmd(req.msg_head()->cmd() + 1);
    head.set_len(body.ByteSizeLong());

    return send_to(req.fd_data(), head, body);
}

bool Network::handle_write_events(Connection* c) {
    Codec::STATUS ret;
    int old_cnt, old_bytes;

    old_cnt = c->write_cnt();
    old_bytes = c->write_bytes();
    ret = c->conn_write();
    if (ret != Codec::STATUS::ERR) {
        m_payload.set_write_cnt(m_payload.write_cnt() + (c->write_cnt() - old_cnt));
        m_payload.set_write_bytes(m_payload.write_bytes() + (c->write_bytes() - old_bytes));
    }
    return handle_write_events(c, ret);
}

bool Network::handle_write_events(Connection* c, const HttpMsg& msg) {
    int old_cnt, old_bytes;
    Codec::STATUS ret;

    old_cnt = c->write_cnt();
    old_bytes = c->write_bytes();
    ret = c->conn_write(msg);
    if (ret != Codec::STATUS::ERR) {
        m_payload.set_write_cnt(m_payload.write_cnt() + (c->write_cnt() - old_cnt));
        m_payload.set_write_bytes(m_payload.write_bytes() + (c->write_bytes() - old_bytes));
    }
    return handle_write_events(c, ret);
}

bool Network::handle_write_events(Connection* c, const MsgHead& head, const MsgBody& body) {
    Codec::STATUS ret;
    int old_cnt, old_bytes;

    old_cnt = c->write_cnt();
    old_bytes = c->write_bytes();
    ret = c->conn_write(head, body);
    if (ret != Codec::STATUS::ERR) {
        m_payload.set_write_cnt(m_payload.write_cnt() + (c->write_cnt() - old_cnt));
        m_payload.set_write_bytes(m_payload.write_bytes() + (c->write_bytes() - old_bytes));
    }
    return handle_write_events(c, ret);
}

bool Network::handle_write_events(Connection* c, Codec::STATUS codec_ret) {
    int fd = c->fd();
    ev_io* w = c->get_ev_io();

    if (codec_ret == Codec::STATUS::PAUSE) {
        w = m_events->add_write_event(fd, w, this);
        if (w == nullptr) {
            LOG_ERROR("add write event failed! fd: %d", fd);
            return false;
        } else {
            c->set_ev_io(w);
        }
    } else {
        m_events->del_write_event(w);
        if (codec_ret == Codec::STATUS::ERR) {
            LOG_DEBUG("conn write faild! fd: %d", fd);
            return false;
        }
    }

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

    LOG_TRACE("redis send to node: %s", node);

    uint64_t cmd_id;
    wait_cmd_info_t* index;

    // delete index when callback.
    cmd_id = cmd->id();
    index = new wait_cmd_info_t{this, cmd_id, cmd->get_exec_step()};
    if (index == nullptr) {
        LOG_ERROR("add wait cmd index failed! cmd id: %llu", cmd_id);
        return false;
    }

    if (!m_redis_pool->send_to(node, argv, on_redis_lib_callback, index)) {
        LOG_ERROR("redis send data failed! node: %s.", node);
        SAFE_DELETE(index);
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

    uint64_t cmd_id;
    wait_cmd_info_t* index;

    cmd_id = cmd->id();
    index = new wait_cmd_info_t{this, cmd_id, cmd->get_exec_step()};
    if (index == nullptr) {
        LOG_ERROR("add wait cmd index failed! cmd id: %llu", cmd_id);
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

    uint64_t cmd_id;
    wait_cmd_info_t* index;

    cmd_id = cmd->id();
    index = new wait_cmd_info_t{this, cmd_id, cmd->get_exec_step()};
    if (index == nullptr) {
        LOG_ERROR("add wait cmd index failed! cmd id: %llu", cmd_id);
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
    if (task->error != ERR_OK) {
        LOG_ERROR("database query failed, error: %d, errstr: %s",
                  task->error, task->errstr.c_str());
    }

    wait_cmd_info_t* index = static_cast<wait_cmd_info_t*>(task->privdata);
    handle_cmd_callback(index, task->error, res);
    SAFE_DELETE(index);
}

void Network::on_mysql_lib_exec_callback(const MysqlAsyncConn* c, sql_task_t* task) {
    wait_cmd_info_t* index = static_cast<wait_cmd_info_t*>(task->privdata);
    index->net->on_mysql_exec_callback(c, task);
}

void Network::on_mysql_exec_callback(const MysqlAsyncConn* c, sql_task_t* task) {
    if (task->error != ERR_OK) {
        LOG_ERROR("database query failed, error: %d, errstr: %s",
                  task->error, task->errstr.c_str());
    }
    wait_cmd_info_t* index = static_cast<wait_cmd_info_t*>(task->privdata);
    handle_cmd_callback(index, task->error, nullptr);
    SAFE_DELETE(index);
}

void Network::on_redis_lib_callback(redisAsyncContext* ac, void* reply, void* privdata) {
    wait_cmd_info_t* index = static_cast<wait_cmd_info_t*>(privdata);
    index->net->on_redis_callback(ac, reply, privdata);
}

bool Network::handle_cmd_callback(wait_cmd_info_t* index, int err, void* data) {
    if (index == nullptr) {
        return false;
    }

    Cmd* cmd;
    Cmd::STATUS ret;

    cmd = get_cmd(index->cmd_id);
    if (cmd == nullptr) {
        LOG_WARN("find cmd failed! seq: %llu", index->cmd_id);
        return false;
    }

    cmd->set_active_time(now());
    ret = cmd->on_callback(err, data);
    if (ret != Cmd::STATUS::RUNNING) {
        del_cmd(cmd);
    }

    return true;
}

void Network::on_redis_callback(redisAsyncContext* c, void* reply, void* privdata) {
    LOG_TRACE("redis callback. host: %s, port: %d.", c->c.tcp.host, c->c.tcp.port);

    if (reply != nullptr) {
        redisReply* r = (redisReply*)reply;
        if (c->err != 0) {
            LOG_ERROR("redis callback data: %s, err: %d, errstr: %s",
                      r->str, c->err, c->errstr);
        } else {
            LOG_TRACE("redis callback data: %s, err: %d", r->str, c->err);
        }
    }

    wait_cmd_info_t* index = static_cast<wait_cmd_info_t*>(privdata);
    handle_cmd_callback(index, c->err, reply);
    SAFE_DELETE(index);
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

    LOG_TRACE("delete cmd id: %llu", cmd->id());

    auto it = m_cmds.find(cmd->id());
    if (it == m_cmds.end()) {
        return false;
    }

    m_cmds.erase(it);
    if (cmd->timer() != nullptr) {
        LOG_TRACE("del timer: %p!", cmd->timer());
        del_cmd_timer(cmd->timer());
        cmd->set_timer(nullptr);
    }

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
    m_redis_pool = new kim::RedisMgr(m_logger, m_events->ev_loop());
    if (m_redis_pool == nullptr || !m_redis_pool->init(m_conf["redis"])) {
        SAFE_DELETE(m_redis_pool);
        LOG_ERROR("init redis mgr failed!");
        return false;
    }
    return true;
}

bool Network::load_worker_data_mgr() {
    m_worker_data_mgr = new WorkerDataMgr(m_logger);
    return (m_worker_data_mgr != nullptr);
}

bool Network::update_conn_state(int fd, Connection::STATE state) {
    auto it = m_conns.find(fd);
    if (it == m_conns.end()) {
        return false;
    }
    it->second->set_state(state);
    return true;
}

bool Network::Network::add_client_conn(const std::string& node_id, const fd_t& f) {
    Connection* c = get_conn(f);
    if (c == nullptr) {
        return false;
    }
    m_node_conns[node_id] = c;
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

bool Network::add_io_timer(Connection* c, double secs) {
    /* create timer. */
    LOG_TRACE("add io timer, fd: %d, time val: %f", c->fd(), secs);
    ev_timer* w = m_events->add_io_timer(secs, c->timer(), c);
    if (w == nullptr) {
        LOG_ERROR("add timer failed! fd: %d", c->fd());
        close_conn(c);
        return false;
    }
    c->set_timer(w);
    return true;
}

}  // namespace kim