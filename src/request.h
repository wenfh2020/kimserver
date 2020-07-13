#ifndef __REQUEST_H__
#define __REQUEST_H__

#include "context.h"
#include "proto/http.pb.h"
#include "proto/msg.pb.h"

namespace kim {

class Request {
   public:
    Request(Connection* c, MsgHead* head, MsgBody* body);
    Request(Connection* c, HttpMsg* msg);
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    virtual ~Request() {}

    void set_msg_head(MsgHead* head) { m_msg_head = head; }
    MsgHead* get_msg_head() { return m_msg_head; }

    void set_msg_body(MsgBody* body) { m_msg_body = body; }
    MsgBody* get_msg_body() { return m_msg_body; }

    void set_http_msg(HttpMsg* msg) { m_http_msg = msg; }
    const HttpMsg* get_http_msg() const { return m_http_msg; }

    void set_conn(Connection* c) { m_conn = c; }
    Connection* get_conn() { return m_conn; }

    void set_errno(int n) { m_errno = n; }
    int get_errno() { return m_errno; }

    void set_error(const std::string& error) { m_error = error; }
    std::string get_error() { return m_error; }

    void set_privdate(void* data) { m_privdata = data; }
    void* get_privdata() { return m_privdata; }

   private:
    Connection* m_conn = nullptr;   // connection.
    MsgHead* m_msg_head = nullptr;  // protobuf msg head.
    MsgBody* m_msg_body = nullptr;  // protobuf msg body.
    HttpMsg* m_http_msg = nullptr;  // http msg.

    int m_errno = -1;            // error number.
    std::string m_error;         // error string.
    void* m_privdata = nullptr;  // private data.
};

};  // namespace kim

#endif  // __REQUEST_H__