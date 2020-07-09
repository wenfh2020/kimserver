#include "codec.h"

#include <cryptopp/gzip.h>

namespace kim {

bool Codec::set_codec_type(Codec::TYPE type) {
    if (type < Codec::TYPE::UNKNOWN || type >= Codec::TYPE::COUNT) {
        return false;
    }
    m_codec_type = type;
    return true;
}

Codec::STATUS Codec::encode(const MsgHead& head, const MsgBody& body, SocketBuffer* sbuf) {
    LOG_DEBUG("encode");
    return Codec::STATUS::ERR;
}

Codec::STATUS Codec::decode(SocketBuffer* sbuf, const MsgHead& head, const MsgBody& body) {
    LOG_DEBUG("decode");
    return Codec::STATUS::ERR;
}

bool Codec::gzip(const std::string& src, std::string& dst) {
    try {
        CryptoPP::Gzip zip;
        zip.Put((CryptoPP::byte*)src.c_str(), src.size());
        zip.MessageEnd();

        CryptoPP::word64 avail = zip.MaxRetrievable();
        if (avail) {
            dst.resize(avail);
            zip.Get((CryptoPP::byte*)&dst[0], dst.size());
        }
    } catch (CryptoPP::InvalidDataFormat& e) {
        LOG_ERROR("%s", e.GetWhat().c_str());
        return false;
    }
    return true;
}

bool Codec::ungzip(const std::string& src, std::string& dst) {
    try {
        CryptoPP::Gunzip zip;
        zip.Put((CryptoPP::byte*)src.c_str(), src.size());
        zip.MessageEnd();
        CryptoPP::word64 avail = zip.MaxRetrievable();
        if (avail) {
            dst.resize(avail);
            zip.Get((CryptoPP::byte*)&dst[0], dst.size());
        }
    } catch (CryptoPP::InvalidDataFormat& e) {
        LOG_ERROR("%s", e.GetWhat().c_str());
        return false;
    }
    return (true);
}

};  // namespace kim
