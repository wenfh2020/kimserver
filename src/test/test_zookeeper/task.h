#ifndef __KIM_ZOOKEEPER_TASK_H__
#define __KIM_ZOOKEEPER_TASK_H__

#include <iostream>
#include <vector>

namespace kim {

typedef struct zk_task_s zk_task_t;

/* zk callback fn. */
typedef void(ZkCallbackFn)(const zk_task_t* task);

struct zk_result {
    int error = 0;
    std::string errstr;
    std::string value;
    std::vector<std::string> values;
};

typedef struct zk_task_s {
    enum class OPERATE {
        EXISTS = 0,
        CREATE,
        DELETE,
        GET,
        SET,
        LIST,
        WATCH_DATA,
        WATCH_CHILD,
    };
    std::string path;
    std::string value;
    OPERATE oper = OPERATE::EXISTS;
    bool watch = false;
    int flag = 0;
    void* privdata = nullptr;
    double create_time = 0.0;
    zk_result res;
} zk_task_t;

}  // namespace kim

#endif  //__KIM_ZOOKEEPER_TASK_H__