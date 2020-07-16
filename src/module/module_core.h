#ifndef __MODULE_HELLO_H__
#define __MODULE_HELLO_H__

#include "../module.h"
#include "cmd_hello.h"

namespace kim {

class MoudleCore : public Module {
    BEGIN_HTTP_MAP()
    HTTP_HANDLER("/kim/im/user/", CmdHello, "hello");
    END_HTTP_MAP()
};

}  // namespace kim

#endif  //__MODULE_HELLO_H__