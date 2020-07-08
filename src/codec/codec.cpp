#include "codec.h"

namespace kim {

bool Codec::set_codec_type(CODEC_TYPE type) {
    if (type < CODEC_TYPE::UNKNOWN || type >= CODEC_TYPE::COUNT) {
        return false;
    }
    m_codec_type = type;
    return true;
}

};  // namespace kim
