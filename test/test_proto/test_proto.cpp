// g++ -g -std='c++11' test_proto.cpp ../../src/proto/msg.pb.cc -o proto -lprotobuf && ./proto
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "../../src/core/proto/msg.pb.h"
#include "../../src/core/util/log.h"

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

int main(int argc, char** argv) {
    // check_protobuf();
    test_server(argc, argv);
    return 0;
}
