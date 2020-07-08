#ifndef __CODEC_HTTP_H__
#define __CODEC_HTTP_H__

#include <iostream>
#include <unordered_map>

#include "codec.h"
#include "util/http/http_parser.h"

namespace kim {

class CodecHttp : public Codec {
   public:
    CodecHttp();
    virtual ~CodecHttp();

   private:
    http_parser m_parser;
    std::string m_http_string;
    http_parser_settings m_parser_setting;
    std::unordered_map<std::string, std::string> m_http_headers;
};

};  // namespace kim

#endif  //__CODEC_HTTP_H__