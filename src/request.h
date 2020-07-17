#ifndef __REQUEST_H__
#define __REQUEST_H__

#include "context.h"
#include "proto/http.pb.h"
#include "proto/msg.pb.h"

namespace kim {

class Request {
   public:
    Request() {}
    Request(std::shared_ptr<Connection> c, MsgHead* head, MsgBody* body);
    Request(std::shared_ptr<Connection> c, HttpMsg* msg);
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    virtual ~Request() {}

    void set_msg_head(MsgHead* head) { m_msg_head = head; }
    MsgHead* get_msg_head() { return m_msg_head; }

    void set_msg_body(MsgBody* body) { m_msg_body = body; }
    MsgBody* get_msg_body() { return m_msg_body; }

    void set_http_msg(HttpMsg* msg) { m_http_msg = msg; }
    const HttpMsg* get_http_msg() const { return m_http_msg; }

    void set_conn(std::shared_ptr<Connection> c) { m_conn = c; }
    std::shared_ptr<Connection> get_conn() { return m_conn; }

    void set_errno(int n) { m_errno = n; }
    int get_errno() { return m_errno; }

    void set_privdate(void* data) { m_privdata = data; }
    void* get_privdata() { return m_privdata; }

   private:
    std::shared_ptr<Connection> m_conn = nullptr;  // connection.
    MsgHead* m_msg_head = nullptr;                 // protobuf msg head.
    MsgBody* m_msg_body = nullptr;                 // protobuf msg body.
    HttpMsg* m_http_msg = nullptr;                 // http msg.

    int m_errno = -1;            // error number.
    void* m_privdata = nullptr;  // private data.
};

};  // namespace kim

#endif  // __REQUEST_H__