#ifndef __KIM_REQUEST_H__
#define __KIM_REQUEST_H__

#include "connection.h"
#include "protobuf/proto/http.pb.h"
#include "protobuf/proto/msg.pb.h"

namespace kim {

class Request {
   public:
    Request(const Request& req);
    Request(Connection* c, bool is_http);
    Request(Connection* c, const HttpMsg& msg);
    Request(Connection* c, const MsgHead& head, const MsgBody& body);
    virtual ~Request();

    Request() = delete;
    Request& operator=(const Request&) = delete;

   public:
    void set_msg_head(const MsgHead& head) { CHECK_SET(m_msg_head, MsgHead, head); }
    void set_msg_body(const MsgBody& body) { CHECK_SET(m_msg_body, MsgBody, body); }
    void set_http_msg(const HttpMsg& msg) { CHECK_SET(m_http_msg, HttpMsg, msg); }

    MsgHead* msg_head() { return m_msg_head; }
    MsgBody* msg_body() { return m_msg_body; }
    HttpMsg* http_msg() { return m_http_msg; }
    const MsgHead* msg_head() const { return m_msg_head; }
    const MsgBody* msg_body() const { return m_msg_body; }
    const HttpMsg* http_msg() const { return m_http_msg; }

    void set_conn(Connection* c) { m_conn = c; }
    Connection* conn() { return m_conn; }
    const Connection* conn() const { return m_conn; }

    bool is_http() { return m_is_http; }

   private:
    Connection* m_conn = nullptr;   // connection.
    MsgHead* m_msg_head = nullptr;  // protobuf msg head.
    MsgBody* m_msg_body = nullptr;  // protobuf msg body.
    HttpMsg* m_http_msg = nullptr;  // http msg.
    bool m_is_http = false;
};

};  // namespace kim

#endif  // __KIM_REQUEST_H__