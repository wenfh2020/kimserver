#include <stdio.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "../../../src/core/server.h"
#include "../../../src/core/util/util.h"
#include "../../../src/core/zookeeper/zk.h"
#include "../../../src/core/zookeeper/zk_mgr.h"

/* 
void print_zk_cpp_usage() {
    fprintf(stderr, "usage\n");
    fprintf(stderr,
            "    create <path> <value> <flag>\n"
            "                          0 - persistence\n"
            "                          1 - ephemeral \n"
            "                          2 - sequence \n"
            "                          3 - sequence and ephemeral\n");
    fprintf(stderr, "    delete <path>\n");
    fprintf(stderr, "    set <path> <data>\n");
    fprintf(stderr, "    get <path>\n");
    fprintf(stderr, "    ls <path>\n");
    fprintf(stderr, "    exists <path>\n");
    fprintf(stderr, "    setacl <path> scheme:id:perm\n");
    fprintf(stderr, "    getacl <path>\n");
    fprintf(stderr, "    addauth username passwd\n");
    fprintf(stderr, "    watch_data <path> \n");
    fprintf(stderr, "    watch_child <path> \n");
}
 */

const char* task_oper_to_string(kim::zk_task_t::OPERATE oper) {
    switch (oper) {
        case kim::zk_task_t::OPERATE::EXISTS:
            return "exists";
        case kim::zk_task_t::OPERATE::SET:
            return "set";
        case kim::zk_task_t::OPERATE::GET:
            return "get";
        case kim::zk_task_t::OPERATE::WATCH_DATA:
            return "watch_data";
        case kim::zk_task_t::OPERATE::WATCH_CHILD:
            return "watch_child";
        case kim::zk_task_t::OPERATE::CREATE:
            return "create";
        case kim::zk_task_t::OPERATE::DELETE:
            return "delete";
        case kim::zk_task_t::OPERATE::LIST:
            return "list";
        default:
            return "invalid";
    }
}

void data_change_event(const std::string& path, const std::string& new_value, void* privdata) {
    printf("--------\n");
    printf("data_change_event, path[%s] new_data[%s], privdata[%p]\n",
           path.c_str(), new_value.c_str(), privdata);
}

void child_change_event(const std::string& path, const std::vector<std::string>& children, void* privdata) {
    printf("--------\n");
    printf("child_change_event, path[%s] new_child_count[%d], privdata[%p]\n",
           path.c_str(), (int32_t)children.size(), privdata);

    for (size_t i = 0; i < children.size(); i++) {
        printf("%zu, %s\n", i, children[i].c_str());
    }
}

void cmd_callback_fn(const kim::zk_task_t* task) {
    printf("----------\n");
    printf("cmd cb:\noper: %s\npath: %s\nvalue: %s\nflag:%d\nerror:%d\nerrstr: %s\nprivdata: %p\n",
           task_oper_to_string(task->oper), task->path.c_str(),
           task->value.c_str(), task->flag, task->res.error, task->res.errstr.c_str(), task->privdata);

    switch (task->oper) {
        case kim::zk_task_t::OPERATE::EXISTS:
        case kim::zk_task_t::OPERATE::SET:
        case kim::zk_task_t::OPERATE::WATCH_DATA:
        case kim::zk_task_t::OPERATE::WATCH_CHILD:
        case kim::zk_task_t::OPERATE::DELETE:
            break;
        case kim::zk_task_t::OPERATE::GET:
        case kim::zk_task_t::OPERATE::CREATE:
            printf("res value: %s\n", task->res.value.c_str());
            break;
        case kim::zk_task_t::OPERATE::LIST: {
            std::string list;
            list.append("[");
            for (size_t i = 0; i < task->res.values.size(); i++) {
                //printf("%s\n", children[i].c_str());
                list.append(task->res.values[i]).append(", ");
            }
            list.append("]");
            printf("%s\n", list.c_str());
            break;
        }
        default:
            printf("invalid task oper: %d\n", task->oper);
            break;
    }
}

int main() {
    kim::Log* m_logger = new kim::Log;
    if (!m_logger->set_log_path("./kimserver.log")) {
        LOG_ERROR("set log path failed!");
        SAFE_DELETE(m_logger);
        return 1;
    }
    m_logger->set_level(kim::Log::LL_DEBUG);

    std::string path("/kimserver");
    std::string create_path = path + "/test";
    std::string servers("127.0.0.1:2181");
    std::cout << "servers: " << servers << std::endl;

    kim::ZookeeperMgr* mgr = new kim::ZookeeperMgr(m_logger);
    mgr->set_zk_log("./zk.log", utility::zoo_log_lvl::zoo_log_lvl_info);
    if (!mgr->connect(servers)) {
        LOG_ERROR("init servers failed! servers: %s", servers.c_str());
        SAFE_DELETE(m_logger);
        return 1;
    }

    mgr->attach_zk_cmd_event(&cmd_callback_fn);
    mgr->attach_zk_watch_events(&data_change_event, &child_change_event, (void*)mgr);

    mgr->zk_list(path, (void*)mgr);
    sleep(5);

    mgr->zk_create(create_path, "1", 0, (void*)mgr);
    mgr->zk_create(create_path, "2", 1, (void*)mgr);
    mgr->zk_create(create_path, "3", 2, (void*)mgr);
    mgr->zk_create(create_path, "4", 3, (void*)mgr);

    mgr->zk_get(create_path, (void*)mgr);
    mgr->zk_exists(create_path, (void*)mgr);
    mgr->zk_delelte(create_path, (void*)mgr);
    mgr->zk_exists(create_path, (void*)mgr);

    mgr->zk_watch_data(path, (void*)mgr);
    mgr->zk_watch_children(path, (void*)mgr);

    sleep(5);

    int i = 0;
    while (1) {
        sleep(5);
        mgr->zk_set(path, format_str("%d", i++), (void*)mgr);
        mgr->zk_create(create_path, "4", 3, (void*)mgr);
    }

    sleep(100);

    SAFE_DELETE(m_logger);
    SAFE_DELETE(mgr);
    return 0;
}
