#ifndef __KIM_ERROR_H__
#define __KIM_ERROR_H__

namespace kim {

enum E_ERROR {
    // common.
    ERR_OK = 0,
    ERR_FAILED = 1,

    // redis.
    ERR_REDIS_CONNECT_FAILED = 11001,
    ERR_REDIS_DISCONNECT = 11002,
    ERR_REDIS_CALLBACK = 11003,
};

}

#endif  //__KIM_ERROR_H__