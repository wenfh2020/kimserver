#ifndef __KIM_ZOOKEEPER_TASK_H__
#define __KIM_ZOOKEEPER_TASK_H__

#include <iostream>
#include <vector>

namespace kim {

typedef struct zk_task_s zk_task_t;

/* zk task result. */
struct zk_result {
    int error = 0;
    std::string errstr;
    std::string value;
    std::vector<std::string> values;
};

typedef struct zk_task_s {
    enum class CMD {
        UNKNOWN = 0,
        REGISTER,
        GET,
        NOTIFY_SESSION_CONNECTD,
        NOTIFY_SESSION_EXPIRED,
        NOTIFY_NODE_CREATED,
        NOTIFY_NODE_DELETED,
        NOTIFY_NODE_DATA_CAHNGED,
        NOTIFY_NODE_CHILD_CAHNGED,
        END,
    };
    std::string path;
    std::string value;
    CMD cmd = CMD::UNKNOWN;
    double create_time = 0.0;
    zk_result res;
} zk_task_t;

}  // namespace kim

#endif  //__KIM_ZOOKEEPER_TASK_H__