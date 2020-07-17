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
    virtual bool send_to(std::shared_ptr<Connection> c, const HttpMsg& msg) { return true; }
};

}  // namespace kim

#endif  //__NET_H__