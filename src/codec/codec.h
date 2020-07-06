#ifndef __CODEC_H__
#define __CODEC_H__

namespace kim {

class Codec {
   public:
    enum class CODEC_TYPE {
        UNKNOWN = 0,
        PROTOBUF,
        HTTP,
        PRIVATE,
    };

    enum class CODEC_STATUS {
        OK = 0,
        ERR = 1,
        PAUSE = 2,
    };

    Codec() {}
    virtual ~Codec() {}

    void set_codec_type(CODEC_TYPE type) { m_codec_type = type; }
    CODEC_TYPE get_codec_type() { return m_codec_type; }

   private:
    CODEC_TYPE m_codec_type = CODEC_TYPE::PROTOBUF;
};

};  // namespace kim

#endif  //__CODEC_H__