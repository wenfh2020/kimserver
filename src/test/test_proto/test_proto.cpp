#include <google/protobuf/util/json_util.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "protobuf/proto/msg.pb.h"
#include "protobuf/sys/nodes.pb.h"
#include "util/log.h"
#include "util/util.h"
using google::protobuf::util::JsonStringToMessage;

#define PROTO_MSG_HEAD_LEN 15

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
}

void test_server(int argc, char** argv) {
    int ret = 0;
    const char* port = "3355";
    const char* ip = "127.0.0.1";

    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int fd = socket(PF_INET, SOCK_STREAM, 0);
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

    char recv_buf[256];

    while (1) {
        // send.
        MsgHead head;
        head.set_cmd(1);
        head.set_seq(123);
        MsgBody body;
        body.set_data("hello world!");
        head.set_len(body.ByteSizeLong());

        int buf_len = head.ByteSizeLong() + body.ByteSizeLong();
        char* buf = new char[buf_len];
        memcpy(buf, head.SerializeAsString().c_str(), head.ByteSizeLong());
        memcpy(buf + head.ByteSizeLong(), body.SerializeAsString().c_str(), body.ByteSizeLong());

        ret = send(fd, buf, buf_len, 0);
        if (ret < 0) {
            delete[] buf;
            printf("send err: %d, errstr: %s \n", ret, gai_strerror(ret));
            break;
        }

        // recvk
        ret = recv(fd, recv_buf, 256, 0);
        if (ret <= 0) {
            delete[] buf;
            printf("recv err: %d, errstr: %s \n", ret, gai_strerror(ret));
            break;
        }
        std::cout << recv_buf << std::endl;
        delete[] buf;

        head.ParseFromArray(recv_buf, PROTO_MSG_HEAD_LEN);
        body.ParseFromArray(recv_buf + PROTO_MSG_HEAD_LEN, head.len());
        printf("cmd: %d, seq: %d, len: %d, body len: %zu, %s\n",
               head.cmd(), head.seq(), head.len(),
               body.data().length(), body.data().c_str());
        break;
    }

    close(fd);
}

class addr_info_t {
   public:
    std::string bind;       // bind host for inner server.
    int port = 0;           // port for inner server.
    std::string gate_bind;  // bind host for user client.
    int gate_port = 0;      // port for user client.
};

////////////////////////////////////

class node_info_t {
   public:
    addr_info_t addr_info;     // network addr info.
    std::string node_type;     // node type in cluster.
    std::string conf_path;     // config path.
    std::string work_path;     // process work path.
    int worker_processes = 0;  // number of worker's processes.
};

void compare_struct() {
    double begin = mstime();

    for (int i = 0; i < 1000000; i++) {
        node_info_t node;
        node.addr_info.bind = "wruryeuwryeuwrw";
        node.addr_info.port = 342;
        node.addr_info.gate_bind = "fsduyruwerw";
        node.addr_info.gate_port = 4853;

        node.node_type = "34rw343";
        node.conf_path = "reuwyruiwe";
        node.work_path = "ewiruwe";
        node.worker_processes = 3;
    }
    printf("struct spend time: %f\n", mstime() - begin);

    begin = mstime();
    for (int i = 0; i < 1000000; i++) {
        kim::node_info node;
        node.mutable_addr_info()->set_bind("wruryeuwryeuwrw");
        node.mutable_addr_info()->set_port(342);
        node.mutable_addr_info()->set_gate_bind("fsduyruwerw");
        node.mutable_addr_info()->set_gate_port(4853);

        node.set_node_type("34rw343");
        node.set_conf_path("reuwyruiwe");
        node.set_work_path("ewiruwe");
        node.set_worker_cnt(3);
    }
    printf("proto spend time: %f\n", mstime() - begin);
}

void test_proto_json() {
    kim::node_info node;
    node.set_name("111111");
    node.mutable_addr_info()->set_bind("wruryeuwryeuwrw");
    node.mutable_addr_info()->set_port(342);
    node.mutable_addr_info()->set_gate_bind("fsduyruwerw");
    node.mutable_addr_info()->set_gate_port(4853);

    node.set_node_type("34rw343");
    node.set_conf_path("reuwyruiwe");
    node.set_work_path("ewiruwe");
    node.set_worker_cnt(3);

    std::string json_string;
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    options.always_print_primitive_fields = true;
    options.preserve_proto_field_names = true;
    MessageToJsonString(node, &json_string, options);

    // std::cout << node.SerializeAsString() << std::endl
    std::cout << json_string << std::endl;

    node.Clear();
    if (JsonStringToMessage(json_string, &node).ok()) {
        std::cout << "json to protobuf: "
                  << node.name()
                  << ", "
                  << node.mutable_addr_info()->bind()
                  << std::endl;
    }
}

int main(int argc, char** argv) {
    // check_protobuf();
    // test_server(argc, argv);
    // compare_struct();
    // test_proto_json();
    return 0;
}