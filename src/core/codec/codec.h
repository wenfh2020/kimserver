#ifndef __CODEC_H__
#define __CODEC_H__

#include "../proto/http.pb.h"
#include "../proto/msg.pb.h"
#include "../server.h"
#include "../util/log.h"
#include "../util/socket_buffer.h"

namespace kim {

class Codec {
   public:
    enum class TYPE {
        UNKNOWN = 0,
        PROTOBUF = 1,
        HTTP = 2,
        PRIVATE = 3,
        COUNT = 4,
    };

    enum class STATUS {
        OK = 0,
        ERR = 1,
        PAUSE = 2,
    };

    Codec(Log* logger, Codec::TYPE codec) : m_logger(logger), m_codec(codec) {}
    virtual ~Codec() {}

    virtual Codec::STATUS encode(const MsgHead& head, const MsgBody& body, SocketBuffer* sbuf);
    virtual Codec::STATUS decode(SocketBuffer* sbuf, MsgHead& head, MsgBody& body);

    bool set_codec(Codec::TYPE codec);
    Codec::TYPE get_codec() { return m_codec; }

    bool gzip(const std::string& src, std::string& dst);
    bool ungzip(const std::string& src, std::string& dst);

   protected:
    Log* m_logger = nullptr;
    Codec::TYPE m_codec = Codec::TYPE::PROTOBUF;
};

};  // namespace kim

#endif  //__CODEC_H__