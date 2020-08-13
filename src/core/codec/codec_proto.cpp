#include "codec_proto.h"

#define PROTO_MSG_HEAD_LEN 15

namespace kim {

CodecProto::CodecProto(Log* logger, Codec::TYPE codec)
    : Codec(logger, codec) {
}

Codec::STATUS
CodecProto::encode(const MsgHead& head, const MsgBody& body, SocketBuffer* sbuf) {
    size_t len = 0;
    size_t write_len = 0;

    write_len = sbuf->_write(head.SerializeAsString().c_str(), PROTO_MSG_HEAD_LEN);
    if (write_len != PROTO_MSG_HEAD_LEN) {
        LOG_ERROR("encode head failed!, cmd: %d, seq: %d", head.cmd(), head.seq());
        return CodecProto::STATUS::ERR;
    }

    len += write_len;

    if (head.len() <= 0) {
        // msg maybe has empty body, like heartbeat.
        return CodecProto::STATUS::OK;
    }

    write_len = sbuf->_write(body.SerializeAsString().c_str(), body.ByteSizeLong());
    if (write_len != body.ByteSizeLong()) {
        LOG_ERROR("encode failed! cmd: %d, seq: %d, write len: %d, body len: %d",
                  head.cmd(), head.seq(), write_len, body.ByteSizeLong());
        sbuf->set_write_index(sbuf->get_write_index() - len);
        return CodecProto::STATUS::ERR;
    }

    return CodecProto::STATUS::OK;
}

Codec::STATUS CodecProto::decode(SocketBuffer* sbuf, MsgHead& head, MsgBody& body) {
    LOG_DEBUG("decode data len: %d, cur read index: %d",
              sbuf->get_readable_len(), sbuf->get_read_index());

    if (sbuf->get_readable_len() < PROTO_MSG_HEAD_LEN) {
        LOG_DEBUG("wait for enough data to decode.");
        return CodecProto::STATUS::PAUSE;  // wait for more data to decode.
    }

    // parse msg head.
    bool ret = head.ParseFromArray(sbuf->get_raw_read_buffer(), PROTO_MSG_HEAD_LEN);
    if (!ret) {
        LOG_ERROR("decode head failed");
        return CodecProto::STATUS::ERR;
    }

    // msg maybe has empty body, like heartbeat.
    if (head.len() <= 0) {
        sbuf->skip_bytes(PROTO_MSG_HEAD_LEN);
        return CodecProto::STATUS::OK;
    }

    // parse msg body.
    if ((int)sbuf->get_readable_len() < PROTO_MSG_HEAD_LEN + head.len()) {
        LOG_DEBUG("wait for enough data to decode msg body.");
        return CodecProto::STATUS::PAUSE;  // wait for more data to decode.
    }

    ret = body.ParseFromArray(sbuf->get_raw_read_buffer(), PROTO_MSG_HEAD_LEN + head.len());
    if (!ret) {
        LOG_ERROR("cmd: %d, seq: %d, parse msg body failed!", head.cmd(), head.seq());
        return CodecProto::STATUS::ERR;
    }

    LOG_DEBUG("sbuf reable len: %d, body size: %d",
              sbuf->get_readable_len(), body.ByteSizeLong());
    sbuf->skip_bytes(PROTO_MSG_HEAD_LEN + head.len());
    return CodecProto::STATUS::OK;
}

}  // namespace kim