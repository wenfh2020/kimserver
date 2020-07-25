#ifndef __CMD_HELLO_H__
#define __CMD_HELLO_H__

#include "../cmd.h"

namespace kim {

class CmdHello : public Cmd {
   public:
    CmdHello(Log* logger, INet* net, uint64_t id);
    virtual ~CmdHello();
    virtual Cmd::STATUS execute(std::shared_ptr<Request> req);
};

}  // namespace kim

#endif  //__CMD_HELLO_H__