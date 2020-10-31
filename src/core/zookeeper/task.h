#ifndef __KIM_ZOOKEEPER_TASK_H__
#define __KIM_ZOOKEEPER_TASK_H__

#include <vector>

namespace kim {

typedef struct zk_task_s zk_task_t;

/* zk task result. */
struct zk_result {
    int error = 0;
    std::string errstr;
    std::string value; /* json data. */
};

typedef struct zk_task_s {
    enum class CMD {
        UNKNOWN = 0,
        REGISTER,
        END,
    };
    std::string path;
    std::string value; /* json data. */
    CMD cmd = CMD::UNKNOWN;
    double create_time = 0.0;
    zk_result res; /* task result. */
} zk_task_t;

}  // namespace kim

#endif  //__KIM_ZOOKEEPER_TASK_H__