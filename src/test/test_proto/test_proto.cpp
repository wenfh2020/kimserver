#include <google/protobuf/util/json_util.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "config.pb.h"
#include "protobuf/proto/msg.pb.h"
#include "protobuf/sys/nodes.pb.h"
#include "util/log.h"
#include "util/util.h"

#define PROTO_MSG_HEAD_LEN 15

enum {
    KP_REQ_TEST_PROTO = 1001,
    KP_RSP_TEST_PROTO = 1002,
    KP_REQ_TEST_AUTO_SEND = 1003,
    KP_RSP_TEST_AUTO_SEND = 1004,
};

void check_protobuf() {
    MsgHead head;
    head.set_cmd(123);
    head.set_seq(456);
    head.set_len(789);
    std::cout << head.ByteSizeLong() << std::endl;

    MsgBody body;
    body.set_data("hello");
    MsgBody_Request* req = body.mutable_req_target();
    MsgBody_Response* rsp = body.mutable_rsp_result();
    req->set_route_id(1000);
    req->set_route("abc");
    std::cout << body.SerializePartialAsString() << std::endl;
    std::cout << body.ByteSizeLong() << std::endl;

    rsp->set_code(3423423);
    rsp->set_msg("abcdefg");
    std::cout << body.SerializePartialAsString() << std::endl;
    std::cout << body.ByteSizeLong() << std::endl;

    std::cout << "----------" << std::endl;

    std::cout << body.has_rsp_result() << std::endl;
    std::cout << body.rsp_result().code() << std::endl;
}

void test_server(int argc, char** argv) {
    int fd, ret;
    MsgHead head;
    MsgBody body;
    char buf[256];
    const char* port = "3355";
    const char* ip = "127.0.0.1";
    struct addrinfo hints, *servinfo;
    int i = 100;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    fd = socket(PF_INET, SOCK_STREAM, 0);
    ret = getaddrinfo(ip, port, &hints, &servinfo);
    if (ret != 0) {
        printf("getaddrinfo err: %d, errstr: %s \n", ret, gai_strerror(ret));
        return;
    }

    ret = connect(fd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (ret == -1) {
        printf("connect err: %d, errstr: %s \n", ret, gai_strerror(ret));
        close(fd);
    }
    freeaddrinfo(servinfo);

    while (i-- > 0) {
        /* send. */
        body.set_data("hello world!");
        head.set_seq(123);
        head.set_cmd(KP_REQ_TEST_AUTO_SEND);
        head.set_len(body.ByteSizeLong());

        memcpy(buf, head.SerializeAsString().c_str(), head.ByteSizeLong());
        memcpy(buf + head.ByteSizeLong(), body.SerializeAsString().c_str(), body.ByteSizeLong());

        ret = send(fd, buf, head.ByteSizeLong() + body.ByteSizeLong(), 0);
        if (ret < 0) {
            printf("send err: %d, errstr: %s \n", ret, gai_strerror(ret));
            break;
        }

        /* recv. */
        ret = recv(fd, buf, sizeof(buf), 0);
        if (ret <= 0) {
            printf("recv err: %d, errstr: %s \n", ret, gai_strerror(ret));
            break;
        }

        head.ParseFromArray(buf, PROTO_MSG_HEAD_LEN);
        body.ParseFromArray(buf + PROTO_MSG_HEAD_LEN, head.len());
        printf("%d, cmd: %d, seq: %d, len: %d, body len: %zu, %s\n",
               i, head.cmd(), head.seq(), head.len(),
               body.data().length(), body.data().c_str());
        // break;
    }

    close(fd);
}

class addr_info_t {
   public:
    std::string node_host;  // host for inner server.
    int node_port = 0;      // port for inner server.
    std::string gate_host;  // host host for user client.
    int gate_port = 0;      // port for user client.
};

////////////////////////////////////

class node_info_t {
   public:
    addr_info_t addr_info;  // network addr info.
    std::string node_type;  // node type in cluster.
    std::string conf_path;  // config path.
    std::string work_path;  // process work path.
    int worker_cnt = 0;     // number of worker's processes.
};

void compare_struct() {
    double begin = mstime();

    for (int i = 0; i < 1000000; i++) {
        node_info_t node;
        node.addr_info.node_host = "wruryeuwryeuwrw";
        node.addr_info.node_port = 342;
        node.addr_info.gate_host = "fsduyruwerw";
        node.addr_info.gate_port = 4853;

        node.node_type = "34rw343";
        node.conf_path = "reuwyruiwe";
        node.work_path = "ewiruwe";
        node.worker_cnt = 3;
    }
    printf("struct spend time: %f\n", mstime() - begin);

    begin = mstime();
    for (int i = 0; i < 1000000; i++) {
        kim::node_info node;
        node.mutable_addr_info()->set_node_host("wruryeuwryeuwrw");
        node.mutable_addr_info()->set_node_port(342);
        node.mutable_addr_info()->set_gate_host("fsduyruwerw");
        node.mutable_addr_info()->set_gate_port(4853);

        node.set_node_type("34rw343");
        node.set_conf_path("reuwyruiwe");
        node.set_work_path("ewiruwe");
        node.set_worker_cnt(3);
    }
    printf("proto spend time: %f\n", mstime() - begin);
}

void convert() {
    kim::node_info node;
    std::string json_string;

    node.set_name("111111");
    node.mutable_addr_info()->set_node_host("wruryeuwryeuwrw");
    node.mutable_addr_info()->set_node_port(342);
    node.mutable_addr_info()->set_gate_host("fsduyruwerw");
    node.mutable_addr_info()->set_gate_port(4853);

    node.set_node_type("34rw343");
    node.set_conf_path("reuwyruiwe");
    node.set_work_path("ewiruwe");
    node.set_worker_cnt(3);

    if (!proto_to_json(node, json_string)) {
        std::cout << "proto to json failed!" << std::endl;
    } else {
        std::cout << "protobuf to json: " << std::endl
                  << json_string << std::endl;
    }

    node.Clear();

    if (json_to_proto(json_string, node)) {
        std::cout << "json to protobuf: "
                  << node.name()
                  << ", "
                  << node.mutable_addr_info()->node_host()
                  << std::endl;
    }
}

void test_proto_convert_json() {
    kim::config config;
    if (json_file_to_proto("config.json", config)) {
        printf("json convert to proto done!\n");
        auto it = config.database().find("test");
        if (it != config.database().end()) {
            printf("host: %s\n", it->second.host().c_str());
        }
    }
}

void test_list() {
    kim::zk_node* an;
    kim::zk_node node;
    kim::register_node rn;

    /* node info. */
    node.set_path("skdfjdskf");
    node.set_type("fsdfsfs");
    node.set_host("127.0.0.1");
    node.set_port(123);
    node.set_worker_cnt(3);
    node.set_active_time(123455);

    /* list. */
    rn.set_my_zk_path("fdsjfkdsfjlsf");
    an = rn.add_nodes();
    *an = node;

    for (int i = 0; i < rn.nodes_size(); i++) {
        an = rn.mutable_nodes(i);
        printf("**node info, ip: %s\n", node.host().c_str());
    }
}

int main(int argc, char** argv) {
    // check_protobuf();
    test_server(argc, argv);
    // compare_struct();
    // convert();
    // test_proto_convert_json();
    // test_list();
    return 0;
}