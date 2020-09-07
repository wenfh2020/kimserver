#ifndef __REQUEST_H__
#define __REQUEST_H__

#include "connection.h"
#include "proto/http.pb.h"
#include "proto/msg.pb.h"

namespace kim {

class Request {
   public:
    Request() {}
    Request(std::shared_ptr<Connection> c, const HttpMsg& msg);
    Request(std::shared_ptr<Connection> c, const MsgHead& head, const MsgBody& body);
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    virtual ~Request();

    MsgHead* msg_head();
    MsgBody* msg_body();
    HttpMsg* http_msg();

    void set_http_msg_info(const HttpMsg& msg);
    void set_msg_info(const MsgHead& head, const MsgBody& body);

    void set_conn(std::shared_ptr<Connection> c) { m_conn = c; }
    std::shared_ptr<Connection> conn() { return m_conn; }

    void set_errno(int n) { m_errno = n; }
    int get_errno() { return m_errno; }

    void set_privdate(void* data) { m_privdata = data; }
    void* privdata() { return m_privdata; }

    bool is_http_req() { return m_is_http; }

   private:
    std::shared_ptr<Connection> m_conn = nullptr;  // connection.
    MsgHead* m_msg_head = nullptr;                 // protobuf msg head.
    MsgBody* m_msg_body = nullptr;                 // protobuf msg body.
    HttpMsg* m_http_msg = nullptr;                 // http msg.

    int m_errno = -1;            // error number.
    void* m_privdata = nullptr;  // private data.
    bool m_is_http = false;
};

};  // namespace kim

#endif  // __REQUEST_H__