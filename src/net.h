#ifndef __NET_H__
#define __NET_H__

#include "context.h"
#include "proto/http.pb.h"
#include "proto/msg.pb.h"

namespace kim {

class INet {
   public:
    INet() {}
    virtual ~INet() {}

   public:
    uint64_t get_seq() { return ++m_seq; }

   public:
    virtual bool send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) { return true; }

   private:
    uint64_t m_seq = 0;
};

}  // namespace kim

#endif  //__NET_H__