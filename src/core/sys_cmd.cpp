#include "sys_cmd.h"

#include "net/chanel.h"
#include "protocol.h"

namespace kim {

SysCmd::SysCmd(Log* logger, INet* net, WorkerDataMgr* d)
    : m_net(net), m_logger(logger), m_woker_data_mgr(d) {
}

bool SysCmd::send_req_connect_to_worker(Connection* c, int worker_index) {
    /* A1 contact with B1. */
    LOG_TRACE("send CMD_REQ_CONNECT_TO_WORKER, fd: %d", c->fd());

    // node_id = c->get_node_id();
    // worker_index = m_nodes->get_node_worker_index(node_id);
    // if (worker_index == -1) {
    //     m_events->del_write_event(c->get_ev_io());
    //     LOG_ERROR("no node info! node id: %s", node_id.c_str())
    //     return;
    // }

    if (!send_req(c, CMD_REQ_CONNECT_TO_WORKER, std::to_string(worker_index))) {
        LOG_ERROR("send CMD_REQ_CONNECT_TO_WORKER failed! fd: %d", c->fd());
        return false;
    }

    c->set_state(Connection::STATE::CONNECTING);
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
Cmd::STATUS SysCmd::process(Request& req) {
    LOG_TRACE("process sys message.");

    if (m_net->is_manager()) {
        switch (req.msg_head()->cmd()) {
            case CMD_REQ_CONNECT_TO_WORKER: {
                return on_req_connect_to_worker(req);
            }
            default: {
                return Cmd::STATUS::UNKOWN;
            }
        }
    } else {
        /* worker. */
        LOG_TRACE("process worker's msg, head cmd: %d, seq: %llu",
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
            default: {
                return Cmd::STATUS::UNKOWN;
            }
        }
    }
}

Cmd::STATUS SysCmd::on_req_connect_to_worker(Request& req) {
    /* B0. */
    LOG_TRACE("A1 connect to B0. handle CMD_REQ_CONNECT_TO_WORKER, fd: %d",
              req.conn()->fd());
    channel_t ch;
    int fd, err, chanel, worker_index, worker_cnt;

    fd = req.conn()->fd();
    worker_cnt = m_woker_data_mgr->get_infos().size();
    worker_index = str_to_int(req.msg_body()->data());

    if (worker_index == 0 ||
        m_woker_data_mgr == nullptr ||
        worker_index > worker_cnt) {
        send_ack(req, ERR_INVALID_WORKER_INDEX, "invalid worker index!");
        LOG_ERROR("invalid worker index, fd: %d, worker index: %d, worker cnt: %d",
                  fd, worker_index, worker_cnt);
        return Cmd::STATUS::ERROR;
    }

    if (!send_ack(req, ERR_OK, "ok")) {
        LOG_ERROR("send CMD_RSP_CONNECT_TO_WORKER failed! fd: %d", fd);
        return Cmd::STATUS::ERROR;
    }

    chanel = m_woker_data_mgr->get_worker_data_fd(worker_index);
    ch = {fd, AF_INET, static_cast<int>(Codec::TYPE::PROTOBUF)};
    err = write_channel(chanel, &ch, sizeof(channel_t), m_logger);
    if (err != 0) {
        LOG_ERROR("transfer fd failed! ch fd: %d, fd: %d, errno: %d",
                  chanel, fd, err);
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::COMPLETED;
}

Cmd::STATUS SysCmd::on_rsp_connect_to_worker(Request& req) {
    /* A1 receives rsp from B0. */
    LOG_TRACE("A1 receive B0's CMD_RSP_CONNECT_TO_WORKER. fd: %d",
              req.conn()->fd());

    if (!req.msg_body()->has_rsp_result()) {
        LOG_ERROR("no rsp result! fd: %d", req.conn()->fd());
        return Cmd::STATUS::ERROR;
    }

    if (req.msg_body()->rsp_result().code() != ERR_OK) {
        LOG_ERROR("parse CMD_RSP_CONNECT_TO_WORKER failed! fd: %d, error: %d, errstr: %s",
                  req.conn()->fd(),
                  req.msg_body()->rsp_result().code(),
                  req.msg_body()->rsp_result().msg().c_str());
        return Cmd::STATUS::ERROR;
    }

    /* A1 --> B1: CMD_REQ_TELL_WORKER */
    target_node tnode;
    tnode.set_node_type(m_net->node_type());
    tnode.set_ip(m_net->node_host());
    tnode.set_port(m_net->node_port());
    tnode.set_worker_index(m_net->worker_index());

    if (!send_req(req, tnode.SerializeAsString())) {
        LOG_ERROR("send data failed! fd: %d, ip: %s, port: %d, worker_index: %d",
                  req.conn()->fd(), tnode.ip().c_str(), tnode.port(), tnode.worker_index());
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_req_tell_worker(Request& req) {
    /* B1 */
    LOG_TRACE("B1 send CMD_REQ_TELL_WORKER. fd: %d", req.conn()->fd());

    target_node tnode;
    std::string node_id;

    if (!tnode.ParseFromString(req.msg_body()->data())) {
        LOG_ERROR("parse tell worker node info failed! fd: %d", req.conn()->fd());
        send_ack(req, ERR_INVALID_MSG_DATA, "parse request fail!");
        return Cmd::STATUS::ERROR;
    }

    /* B1 send ack to A1. */
    tnode.Clear();
    tnode.set_node_type(m_net->node_type());
    tnode.set_ip(m_net->node_host());
    tnode.set_port(m_net->node_port());
    tnode.set_worker_index(m_net->worker_index());

    if (!send_ack(req, ERR_OK, "ok", tnode.SerializeAsString())) {
        LOG_ERROR("send CMD_RSP_TELL_WORKER failed! fd: %d", req.conn()->fd());
        return Cmd::STATUS::ERROR;
    }

    /* B1 connect A1 ok. */
    node_id = format_nodes_id(tnode.ip(), tnode.port(), tnode.worker_index());
    m_net->update_conn_state(req.conn()->fd(), Connection::STATE::CONNECTED);
    m_net->add_client_conn(node_id, req.conn());
    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::on_rsp_tell_worker(Request& req) {
    /* A1 */
    LOG_TRACE("A1 receives CMD_RSP_TELL_WORKER. fd: %d", req.conn()->fd());

    target_node tnode;
    std::string node_id;

    if (!req.msg_body()->has_rsp_result()) {
        LOG_ERROR("CMD_RSP_TELL_WORKER, no rsp result! fd: %d", req.conn()->fd());
        return Cmd::STATUS::ERROR;
    }

    if (req.msg_body()->rsp_result().code() != ERR_OK) {
        LOG_ERROR("CMD_RSP_TELL_WORKER, error! fd: %d, error: %d, errstr: %s",
                  req.conn()->fd(),
                  req.msg_body()->rsp_result().code(),
                  req.msg_body()->rsp_result().msg().c_str());
        return Cmd::STATUS::ERROR;
    }

    /* A1 save B1 worker info. */
    if (!tnode.ParseFromString(req.msg_body()->data())) {
        LOG_ERROR("CMD_RSP_TELL_WORKER, parse B1' result failed! fd: %d",
                  req.conn()->fd());
        return Cmd::STATUS::ERROR;
    }

    node_id = format_nodes_id(tnode.ip(), tnode.port(), tnode.worker_index());
    m_net->update_conn_state(req.conn()->fd(), Connection::STATE::CONNECTED);
    m_net->add_client_conn(node_id, req.conn());

    /* A1 begin to send waiting buffer. */
    return Cmd::STATUS::RUNNING;
}

Cmd::STATUS SysCmd::on_req_add_zk_node(
    Connection* c, const MsgHead& head, const MsgBody& body) {
    LOG_TRACE("handle CMD_REQ_ADD_ZK_NODE. fd: %d", c->fd());

    MsgHead h(head);
    MsgBody b;
    zk_node node;

    h.set_cmd(CMD_RSP_ADD_ZK_NODE);

    if (!node.ParseFromString(body.data())) {
        b.mutable_rsp_result()->set_code(ERR_INVALID_MSG_DATA);
        b.mutable_rsp_result()->set_msg("parse msg body failed!");
        h.set_len(body.ByteSizeLong());
        if (!m_net->send_to(c, head, body)) {
            LOG_ERROR("send CMD_RSP_CONNECT_TO_WORKER failed! fd: %d", c->fd());
            return Cmd::STATUS::ERROR;
        }
        LOG_ERROR("parase msg body failed! fd: %d", c->fd());
        return Cmd::STATUS::ERROR;
    }
    return Cmd::STATUS::OK;
}

bool SysCmd::send_req(Request& req, const std::string& data) {
    MsgHead head;
    MsgBody body;

    body.set_data(data);
    head.set_seq(req.msg_head()->seq());
    head.set_cmd(req.msg_head()->cmd() + 1);
    head.set_len(body.ByteSizeLong());

    return m_net->send_to(req.conn(), head, body);
}

bool SysCmd::send_req(Connection* c, int cmd, const std::string& data) {
    MsgHead head;
    MsgBody body;

    body.set_data(data);
    head.set_cmd(cmd);
    head.set_seq(m_net->new_seq());
    head.set_len(body.ByteSizeLong());

    return m_net->send_to(c, head, body);
}

bool SysCmd::send_ack(Request& req, int err,
                      const std::string& errstr, const std::string& data) {
    MsgHead head;
    MsgBody body;

    body.set_data(data);
    body.mutable_rsp_result()->set_code(err);
    body.mutable_rsp_result()->set_msg(errstr);

    head.set_seq(req.msg_head()->seq());
    head.set_cmd(req.msg_head()->cmd() + 1);
    head.set_len(body.ByteSizeLong());

    return m_net->send_to(req.conn(), head, body);
}

}  // namespace kim