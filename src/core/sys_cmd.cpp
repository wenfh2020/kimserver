#include "sys_cmd.h"

#include "net/chanel.h"
#include "protocol.h"
#include "worker_data_mgr.h"

namespace kim {

SysCmd::SysCmd(Log* logger, INet* net)
    : m_net(net), m_logger(logger) {
}

bool SysCmd::check_rsp(const Request& req) {
    if (!req.msg_body()->has_rsp_result()) {
        LOG_ERROR("no rsp result! fd: %d, cmd: %d", req.fd(), req.msg_head()->cmd());
        return false;
    }

    if (req.msg_body()->rsp_result().code() != ERR_OK) {
        LOG_ERROR("rsp code is not ok, error! fd: %d, error: %d, errstr: %s",
                  req.fd(),
                  req.msg_body()->rsp_result().code(),
                  req.msg_body()->rsp_result().msg().c_str());
        return false;
    }

    return true;
}

bool SysCmd::send_req_connect_to_worker(Connection* c) {
    /* A1 contact with B1. */
    LOG_TRACE("send CMD_REQ_CONNECT_TO_WORKER, fd: %d", c->fd());

    int worker_index;
    std::string node_id;

    node_id = c->get_node_id();
    worker_index = m_net->nodes()->get_node_worker_index(node_id);
    if (worker_index == -1) {
        LOG_ERROR("no node info! node id: %s", node_id.c_str())
        return false;
    }

    if (!m_net->send_req(c, CMD_REQ_CONNECT_TO_WORKER,
                         m_net->new_seq(), std::to_string(worker_index))) {
        LOG_ERROR("send CMD_REQ_CONNECT_TO_WORKER failed! fd: %d", c->fd());
        return false;
    }

    c->set_state(Connection::STATE::CONNECTING);
    return true;
}

bool SysCmd::send_parent_sync_zk_nodes(int version) {
    LOG_TRACE("send CMD_REQ_SYNC_ZK_NODES");
    if (!m_net->send_to_parent(
            CMD_REQ_SYNC_ZK_NODES, m_net->new_seq(), std::to_string(version))) {
        LOG_ERROR("send CMD_REQ_SYNC_ZK_NODES failed!");
        return false;
    }
    return true;
}

bool SysCmd::send_parent_payload(const Payload& pl) {
    LOG_TRACE("send CMD_REQ_UPDATE_PAYLOAD");
    // std::string data;
    // if (!proto_to_json(pl, data)) {
    //     LOG_ERROR("CMD_REQ_UPDATE_PAYLOAD proto json failed!");
    //     return false;
    // }
    if (!m_net->send_to_parent(
            CMD_REQ_UPDATE_PAYLOAD, m_net->new_seq(), pl.SerializeAsString())) {
        LOG_ERROR("send CMD_REQ_UPDATE_PAYLOAD failed!");
        return false;
    }
    return true;
}

bool SysCmd::send_children_add_zk_node(const zk_node& node) {
    LOG_TRACE("send CMD_REQ_ADD_ZK_NODE");
    return m_net->send_to_children(
        CMD_REQ_ADD_ZK_NODE, m_net->new_seq(), node.SerializeAsString());
}

bool SysCmd::send_children_del_zk_node(const std::string& zk_path) {
    LOG_TRACE("send CMD_REQ_DEL_ZK_NODE, path: %d", zk_path.c_str());
    return m_net->send_to_children(CMD_REQ_DEL_ZK_NODE, m_net->new_seq(), zk_path);
}

bool SysCmd::send_children_reg_zk_node(const register_node& rn) {
    LOG_TRACE("send CMD_REQ_REGISTER_NODE, path: %s", rn.my_zk_path().c_str());
    return m_net->send_to_children(
        CMD_REQ_REGISTER_NODE, m_net->new_seq(), rn.SerializeAsString());
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

Cmd::STATUS SysCmd::process(const Request& req) {
    return (m_net->is_manager()) ? process_manager_msg(req) : process_worker_msg(req);
}

Cmd::STATUS SysCmd::process_manager_msg(const Request& req) {
    LOG_TRACE("process manager message.");
    switch (req.msg_head()->cmd()) {
        case CMD_REQ_CONNECT_TO_WORKER: {
            return on_req_connect_to_worker(req);
        }
        case CMD_RSP_ADD_ZK_NODE: {
            return on_rsp_add_zk_node(req);
        }
        case CMD_RSP_DEL_ZK_NODE: {
            return on_rsp_del_zk_node(req);
        }
        case CMD_RSP_REGISTER_NODE: {
            return on_rsp_reg_zk_node(req);
        }
        case CMD_REQ_SYNC_ZK_NODES: {
            return on_req_sync_zk_nodes(req);
        }
        case CMD_REQ_UPDATE_PAYLOAD: {
            return on_req_update_payload(req);
        }
        default: {
            return Cmd::STATUS::UNKOWN;
        }
    }
}

Cmd::STATUS SysCmd::process_worker_msg(const Request& req) {
    /* worker. */
    LOG_TRACE("process worker's msg, head cmd: %d, seq: %u",
              req.msg_head()->cmd(), req.msg_head()->seq());

    switch (req.msg_head()->cmd()) {
        case CMD_RSP_CONNECT_TO_WORKER: {
            return on_rsp_connect_to_worker(req);
        }
        case CMD_REQ_TELL_WORKER: {
            return on_req_tell_worker(req);
        }
        case CMD_RSP_TELL_WORKER: {
            return on_rsp_tell_worker(req);
        }
        case CMD_REQ_ADD_ZK_NODE: {
            return on_req_add_zk_node(req);
        }
        case CMD_REQ_DEL_ZK_NODE: {
            return on_req_del_zk_node(req);
        }
        case CMD_REQ_REGISTER_NODE: {
            return on_req_reg_zk_node(req);
        }
        case CMD_RSP_SYNC_ZK_NODES: {
            return on_rsp_sync_zk_nodes(req);
        }
        case CMD_RSP_UPDATE_PAYLOAD: {
            return on_rsp_update_payload(req);
        }
        default: {
            return Cmd::STATUS::UNKOWN;
        }
    }
}

void SysCmd::on_repeat_timer() {
    if (m_net->is_worker()) {
        if (++m_timer_index % (60 * 5) == 2) {
            send_parent_sync_zk_nodes(m_net->nodes()->version());
        }
    }
}

Cmd::STATUS SysCmd::on_req_connect_to_worker(const Request& req) {
    /* B0. */
    LOG_TRACE("A1 connect to B0. handle CMD_REQ_CONNECT_TO_WORKER, fd: %d", req.fd());
    channel_t ch;
    int fd, err, chanel, worker_index, worker_cnt;

    fd = req.fd();
    worker_cnt = m_net->worker_data_mgr()->get_infos().size();
    worker_index = str_to_int(req.msg_body()->data());

    if (worker_index == 0 || worker_index > worker_cnt) {
        m_net->send_ack(req, ERR_INVALID_WORKER_INDEX, "invalid worker index!");
        LOG_ERROR("invalid worker index, fd: %d, worker index: %d, worker cnt: %d",
                  fd, worker_index, worker_cnt);
        return Cmd::STATUS::ERROR;
    }

    if (!m_net->send_ack(req, ERR_OK, "ok")) {
        LOG_ERROR("send CMD_RSP_CONNECT_TO_WORKER failed! fd: %d", fd);
        return Cmd::STATUS::ERROR;
    }

    chanel = m_net->worker_data_mgr()->get_worker_data_fd(worker_index);
    ch = {fd, AF_INET, static_cast<int>(Codec::TYPE::PROTOBUF)};
    err = write_channel(chanel, &ch, sizeof(channel_t), m_logger);
    if (err != 0) {
        LOG_ERROR("transfer fd failed! ch fd: %d, fd: %d, errno: %d",
                  chanel, fd, err);
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::COMPLETED;
}

Cmd::STATUS SysCmd::on_rsp_connect_to_worker(const Request& req) {
    /* A1 receives rsp from B0. */
    LOG_TRACE("A1 receive B0's CMD_RSP_CONNECT_TO_WORKER. fd: %d", req.fd());

    if (!check_rsp(req)) {
        LOG_ERROR("CMD_RSP_CONNECT_TO_WORKER is not ok! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    /* A1 --> B1: CMD_REQ_TELL_WORKER */
    target_node tn;
    tn.set_node_type(m_net->node_type());
    tn.set_ip(m_net->node_host());
    tn.set_port(m_net->node_port());
    tn.set_worker_index(m_net->worker_index());

    if (!m_net->send_req(req.fd_data(), CMD_REQ_TELL_WORKER, m_net->new_seq(), tn.SerializeAsString())) {
        LOG_ERROR("send data failed! fd: %d, ip: %s, port: %d, worker_index: %d",
                  req.fd(), tn.ip().c_str(), tn.port(), tn.worker_index());
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_req_tell_worker(const Request& req) {
    /* B1 */
    LOG_TRACE("B1 send CMD_REQ_TELL_WORKER. fd: %d", req.fd());

    target_node tn;
    std::string node_id;

    if (!tn.ParseFromString(req.msg_body()->data())) {
        LOG_ERROR("parse tell worker node info failed! fd: %d", req.fd());
        m_net->send_ack(req, ERR_INVALID_MSG_DATA, "parse request failed!");
        return Cmd::STATUS::ERROR;
    }

    /* B1 send ack to A1. */
    tn.Clear();
    tn.set_node_type(m_net->node_type());
    tn.set_ip(m_net->node_host());
    tn.set_port(m_net->node_port());
    tn.set_worker_index(m_net->worker_index());

    if (!m_net->send_ack(req, ERR_OK, "ok", tn.SerializeAsString())) {
        LOG_ERROR("send CMD_RSP_TELL_WORKER failed! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    /* B1 connect A1 ok. */
    node_id = format_nodes_id(tn.ip(), tn.port(), tn.worker_index());
    m_net->update_conn_state(req.fd(), Connection::STATE::CONNECTED);
    m_net->add_client_conn(node_id, req.fd_data());
    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_rsp_tell_worker(const Request& req) {
    /* A1 */
    LOG_TRACE("A1 receives CMD_RSP_TELL_WORKER. fd: %d", req.fd());

    target_node tn;
    std::string node_id;

    if (!check_rsp(req)) {
        LOG_ERROR("CMD_RSP_TELL_WORKER is not ok! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    /* A1 save B1 worker info. */
    if (!tn.ParseFromString(req.msg_body()->data())) {
        LOG_ERROR("CMD_RSP_TELL_WORKER, parse B1' result failed! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    node_id = format_nodes_id(tn.ip(), tn.port(), tn.worker_index());
    m_net->update_conn_state(req.fd(), Connection::STATE::CONNECTED);
    m_net->add_client_conn(node_id, req.fd_data());

    /* A1 begin to send waiting buffer. */
    return Cmd::STATUS::RUNNING;
}

Cmd::STATUS SysCmd::on_req_add_zk_node(const Request& req) {
    LOG_TRACE("handle CMD_REQ_ADD_ZK_NODE. fd: % d", req.fd());

    zk_node zn;
    if (!zn.ParseFromString(req.msg_body()->data())) {
        LOG_ERROR("parse tell worker node info failed! fd: %d", req.fd());
        m_net->send_ack(req, ERR_INVALID_MSG_DATA, "parse request failed!");
        return Cmd::STATUS::ERROR;
    }

    if (!m_net->nodes()->add_zk_node(zn)) {
        LOG_ERROR("add zk node failed! fd: %d, path: %s", zn.path().c_str(), req.fd());
        if (!m_net->send_ack(req, ERR_FAILED, "add zk node failed!")) {
            LOG_ERROR("send CMD_RSP_ADD_ZK_NODE failed! fd: %d", req.fd());
        }
        return Cmd::STATUS::ERROR;
    }

    m_net->nodes()->print_debug_nodes_info();

    if (!m_net->send_ack(req, ERR_OK, "ok")) {
        LOG_ERROR("send CMD_RSP_ADD_ZK_NODE failed! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_rsp_add_zk_node(const Request& req) {
    if (!check_rsp(req)) {
        LOG_ERROR("CMD_RSP_ADD_ZK_NODE is not ok! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_req_del_zk_node(const Request& req) {
    LOG_TRACE("handle CMD_REQ_DEL_ZK_NODE. fd: % d", req.fd());

    std::string zk_path = req.msg_body()->data();
    if (zk_path.empty()) {
        LOG_ERROR("parse tell worker node info failed! fd: %d", req.fd());
        m_net->send_ack(req, ERR_INVALID_MSG_DATA, "parse request failed!");
        return Cmd::STATUS::ERROR;
    }

    m_net->nodes()->del_zk_node(zk_path);
    m_net->nodes()->print_debug_nodes_info();

    if (!m_net->send_ack(req, ERR_OK, "ok")) {
        LOG_ERROR("send CMD_RSP_DEL_ZK_NODE failed! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_rsp_del_zk_node(const Request& req) {
    if (!check_rsp(req)) {
        LOG_ERROR("CMD_RSP_DEL_ZK_NODE is not ok! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_req_reg_zk_node(const Request& req) {
    LOG_TRACE("handle CMD_REQ_REGISTER_NODE. fd: % d", req.fd());

    kim::zk_node* zn;
    kim::register_node rn;

    if (!rn.ParseFromString(req.msg_body()->data())) {
        LOG_ERROR("parse CMD_REQ_REGISTER_NODE data failed! fd: %d", req.fd());
        m_net->send_ack(req, ERR_INVALID_MSG_DATA, "parse request data failed!");
        return Cmd::STATUS::ERROR;
    }

    m_net->nodes()->clear();
    m_net->nodes()->set_my_zk_node_path(rn.my_zk_path());
    for (int i = 0; i < rn.nodes_size(); i++) {
        zn = rn.mutable_nodes(i);
        m_net->nodes()->add_zk_node(*zn);
    }
    m_net->nodes()->print_debug_nodes_info();

    if (!m_net->send_ack(req, ERR_OK, "ok")) {
        LOG_ERROR("send CMD_RSP_REGISTER_NODE failed! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_rsp_reg_zk_node(const Request& req) {
    LOG_TRACE("handle CMD_RSP_REGISTER_NODE. fd: % d", req.fd());
    if (!check_rsp(req)) {
        LOG_ERROR("CMD_RSP_REGISTER_NODE is not ok! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

/* manager. */
Cmd::STATUS SysCmd::on_req_sync_zk_nodes(const Request& req) {
    LOG_TRACE("handle CMD_REQ_SYNC_ZK_NODES. fd: % d", req.fd());

    /* check node version. sync reg nodes.*/
    uint32_t version;
    register_node rn;

    version = str_to_int(req.msg_body()->data());
    rn.set_version(version);

    if (m_net->nodes()->version() != version) {
        rn.set_my_zk_path(m_net->nodes()->get_my_zk_node_path());
        const std::unordered_map<std::string, zk_node>& nodes =
            m_net->nodes()->get_zk_nodes();
        for (const auto& it : nodes) {
            *rn.add_nodes() = it.second;
        }
    }

    LOG_TRACE("version, old: %d, new: %d", version, m_net->nodes()->version());

    if (!m_net->send_ack(req, ERR_OK, "ok", rn.SerializeAsString())) {
        LOG_ERROR("send CMD_REQ_SYNC_ZK_NODES failed! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_rsp_sync_zk_nodes(const Request& req) {
    LOG_TRACE("handle CMD_RSP_SYNC_ZK_NODES. fd: % d", req.fd());
    if (!check_rsp(req)) {
        LOG_ERROR("CMD_RSP_SYNC_ZK_NODES is not ok! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    kim::zk_node* zn;
    kim::register_node rn;
    if (!rn.ParseFromString(req.msg_body()->data())) {
        LOG_ERROR("parse CMD_RSP_SYNC_ZK_NODES data failed! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    if (rn.version() != m_net->nodes()->version()) {
        m_net->nodes()->clear();
        m_net->nodes()->set_my_zk_node_path(rn.my_zk_path());
        for (int i = 0; i < rn.nodes_size(); i++) {
            zn = rn.mutable_nodes(i);
            m_net->nodes()->add_zk_node(*zn);
        }
        m_net->nodes()->print_debug_nodes_info();
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_req_update_payload(const Request& req) {
    LOG_TRACE("handle CMD_REQ_UPDATE_PAYLOAD. fd: % d", req.fd());

    kim::Payload pl;
    worker_info_t* info;

    if (!pl.ParseFromString(req.msg_body()->data())) {
        LOG_ERROR("parse CMD_REQ_UPDATE_PAYLOAD data failed! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    info = m_net->worker_data_mgr()->get_worker_info(pl.worker_index());
    if (info == nullptr) {
        m_net->send_ack(req, ERR_INVALID_WORKER_INDEX, "can not find worker index!");
        LOG_ERROR("can not find worker index: %d", pl.worker_index());
        return Cmd::STATUS::ERROR;
    }

    info->payload = pl;

    if (!m_net->send_ack(req, ERR_OK, "ok")) {
        LOG_ERROR("send CMD_RSP_UPDATE_PAYLOAD failed! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_rsp_update_payload(const Request& req) {
    if (!check_rsp(req)) {
        LOG_ERROR("CMD_RSP_UPDATE_PAYLOAD is not ok! fd: %d", req.fd());
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

}  // namespace kim