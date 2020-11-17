#include "sys_cmd.h"

#include "net/chanel.h"

namespace kim {

SysCmd::SysCmd(Log* logger, INet* net, WorkerDataMgr* d)
    : m_net(net), m_logger(logger), m_woker_data_mgr(d) {
}

bool SysCmd::send_req_connect_to_worker(
    std::shared_ptr<Connection>& c, int worker_index) {
    /* A1 contact with B1. */
    LOG_TRACE("send CMD_REQ_CONNECT_TO_WORKER, fd: %d", c->fd());

    MsgHead head;
    MsgBody body;
    std::string node_id;

    // node_id = c->get_node_id();
    // worker_index = m_nodes->get_node_worker_index(node_id);
    // if (worker_index == -1) {
    //     m_events->del_write_event(c->get_ev_io());
    //     LOG_ERROR("no node info! node id: %s", node_id.c_str())
    //     return;
    // }

    head.set_seq(m_net->new_seq());
    head.set_cmd(CMD_REQ_CONNECT_TO_WORKER);
    body.set_data(std::to_string(worker_index));
    head.set_len(body.ByteSizeLong());
    if (!m_net->send_to(c, head, body)) {
        LOG_ERROR("try connect to node failed! node id: %s",
                  node_id.c_str());
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
 * process_sys_message(...)
 * 1. A1 connect to B0. (inner host : inner port)
 * 2. B1 send CMD_REQ_CONNECT_TO_WORKER to B0.
 * 3. B0 send CMD_RSP_CONNECT_TO_WORKER to A1.
 * 4. B0 transfer A1's fd to B1.
 * 5. A1 send CMD_REQ_TELL_WORKER to B1.
 * 6. B1 send CMD_RSP_TELL_WORKER A1.
 * 7. A1 send waiting buffer to B1.
 * 8. B1 send ack to A1.
 */
Cmd::STATUS SysCmd::process(
    std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body) {
    LOG_TRACE("process sys message.");

    if (m_net->is_manager()) {
        switch (head.cmd()) {
            case CMD_REQ_CONNECT_TO_WORKER: {
                return req_connect_to_worker(c, head, body);
            }
            default: {
                return Cmd::STATUS::UNKOWN;
            }
        }
    } else {
        /* worker. */
        LOG_TRACE("process worker's msg, head cmd: %d, seq: %llu",
                  head.cmd(), head.seq());

        switch (head.cmd()) {
            case CMD_RSP_CONNECT_TO_WORKER: {
                return rsp_connect_to_worker(c, head, body);
            }
            case CMD_REQ_TELL_WORKER: {
                return req_tell_worker(c, head, body);
            }
            case CMD_RSP_TELL_WORKER: {
                return rsp_tell_worker(c, head, body);
            }
            default: {
                return Cmd::STATUS::UNKOWN;
            }
        }
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::req_connect_to_worker(
    std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body) {
    LOG_TRACE("A1 connect to B0. fd: %d", c->fd());

    int err;
    channel_t ch;
    int chanel_fd;
    int worker_index = str_to_int(body.data());
    int fd = c->fd();

    head.set_cmd(head.cmd() + 1);
    body.mutable_rsp_result()->set_code(ERR_OK);
    body.mutable_rsp_result()->set_msg("OK");
    head.set_len(body.ByteSizeLong());

    if (!m_net->send_to(c, head, body)) {
        LOG_ERROR("send CMD_RSP_CONNECT_TO_WORKER failed! fd: %d", fd);
        return Cmd::STATUS::ERROR;
    }

    chanel_fd = m_woker_data_mgr->get_worker_data_fd(worker_index);
    ch = {fd, AF_INET, static_cast<int>(Codec::TYPE::PROTOBUF)};
    err = write_channel(chanel_fd, &ch, sizeof(channel_t), m_logger);
    if (err == 0 || (err != 0 && err != EAGAIN)) {
        if (err != 0) {
            LOG_ERROR("transfer fd failed! ch fd: %d, fd: %d, errno: %d",
                      chanel_fd, fd, err);
            return Cmd::STATUS::ERROR;
        }
    }
    return Cmd::STATUS::COMPLETED;
}

Cmd::STATUS SysCmd::rsp_connect_to_worker(
    std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body) {
    /* A1 receives rsp from B0. */
    LOG_TRACE("parse CMD_RSP_CONNECT_TO_WORKER. fd: %d", c->fd());

    if (body.mutable_rsp_result()->code() != ERR_OK) {
        LOG_ERROR("parse CMD_RSP_CONNECT_TO_WORKER failed!, error: %d, errstr: %s",
                  body.mutable_rsp_result()->code(),
                  body.mutable_rsp_result()->msg().c_str());
        return Cmd::STATUS::ERROR;
    }

    /* A1 --> B1: CMD_REQ_TELL_WORKER */
    target_node tnode;
    tnode.set_node_type(m_net->node_type());
    tnode.set_ip(m_net->node_inner_host());
    tnode.set_port(m_net->node_inner_port());
    tnode.set_worker_index(m_net->worker_index());

    head.set_seq(head.seq());
    head.set_cmd(CMD_REQ_TELL_WORKER);
    body.set_data(tnode.SerializeAsString());
    head.set_len(body.ByteSizeLong());
    if (!m_net->send_to(c, head, body)) {
        LOG_ERROR("send data failed! ip: %s, port: %d, worker_index: %d",
                  tnode.ip().c_str(), tnode.port(), tnode.worker_index());
        return Cmd::STATUS::ERROR;
    }

    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::req_tell_worker(
    std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body) {
    LOG_TRACE("parse CMD_REQ_TELL_WORKER. fd: %d", c->fd());

    target_node tnode;
    std::string node_id;
    int fd = c->fd();

    /* B1 */
    LOG_TRACE("parse CMD_REQ_TELL_WORKER. fd: %d", fd);

    if (!tnode.ParseFromString(body.data())) {
        LOG_ERROR("parse tell worker node info failed! fd: %d", fd);
        return Cmd::STATUS::ERROR;
    }

    /* B1 send ack to A1. */
    tnode.Clear();
    tnode.set_node_type(m_net->node_type());
    tnode.set_ip(m_net->node_inner_host());
    tnode.set_port(m_net->node_inner_port());
    tnode.set_worker_index(m_net->worker_index());

    head.set_seq(head.seq());
    head.set_cmd(head.cmd() + 1);
    body.set_data(tnode.SerializeAsString());
    head.set_len(body.ByteSizeLong());
    if (!m_net->send_to(c, head, body)) {
        LOG_ERROR("send CMD_RSP_TELL_WORKER failed! fd: %d", fd);
        return Cmd::STATUS::ERROR;
    }

    /* B1 connect A1 ok. */
    node_id = format_nodes_id(tnode.ip(), tnode.port(), tnode.worker_index());
    c->set_state(Connection::STATE::CONNECTED);
    m_net->add_client_conn(node_id, c);
    return Cmd::STATUS::OK;
}

Cmd::STATUS SysCmd::rsp_tell_worker(
    std::shared_ptr<Connection>& c, MsgHead& head, MsgBody& body) {
    /* A1 */
    LOG_TRACE("parse B1 CMD_RSP_TELL_WORKER. fd: %d", c->fd());

    target_node tnode;
    std::string node_id;
    int fd = c->fd();

    if (body.mutable_rsp_result()->code() != ERR_OK) {
        LOG_DEBUG("connect worker failed!........");
        return Cmd::STATUS::ERROR;
    }

    /* A1 save B1 worker info. */
    if (!tnode.ParseFromString(body.data())) {
        LOG_ERROR("parse B1 CMD_RSP_TELL_WORKER failed! fd: %d", fd);
        return Cmd::STATUS::ERROR;
    }

    node_id = format_nodes_id(tnode.ip(), tnode.port(), tnode.worker_index());
    c->set_state(Connection::STATE::CONNECTED);
    m_net->add_client_conn(node_id, c);

    /* A1 begin to send waiting buffer. */
    return Cmd::STATUS::RUNNING;
}

}  // namespace kim