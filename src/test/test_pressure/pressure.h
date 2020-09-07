#ifndef __KIM_PRESSURE_H__
#define __KIM_PRESSURE_H__

#include <ev.h>

#include <iostream>

#include "connection.h"
#include "events.h"
#include "proto/msg.pb.h"
#include "server.h"
#include "util/json/CJsonObject.hpp"
#include "util/log.h"

namespace kim {

class Pressure {
   public:
    typedef struct libev_events_s {
        Pressure* press = nullptr;
        Connection* conn = nullptr;
        struct ev_loop* loop = nullptr;
        bool reading = false;
        bool writing = false;
        ev_io rev, wev;
    } libev_events_t;

    double m_begin_time = 0.0;
    int m_packets = 0;
    int m_test_cnt = 0;
    int m_send_cnt = 0;
    int m_send_ok_cnt = 0;
    int m_send_err_cnt = 0;
    int m_cur_callback_cnt = 0;
    int m_err_callback_cnt = 0;
    int m_ok_callback_cnt = 0;

   public:
    Pressure(Log* logger, struct ev_loop* loop);
    virtual ~Pressure();

    bool start(const char* host, int port, int users, int packets);
    int new_seq() { return ++m_seq; }
    void show_statics_result();

    // connection.
    Connection* get_connect(const char* host, int port);
    bool del_connect(Connection* c);

    bool send_proto(Connection* c, int cmd);
    bool send_proto(Connection* c, MsgHead& head, MsgBody& body);

    // events.
    bool attach_libev(Connection* c);
    void cleanup_events(Connection* c);

    void add_write_event(Connection* c);
    void del_write_event(Connection* c);
    void async_handle_write(Connection* c);

    void add_read_event(Connection* c);
    void del_read_event(Connection* c);
    void async_handle_read(Connection* c);
    bool async_handle_connect(Connection* c);

    static void libev_read_event(EV_P_ ev_io* watcher, int revents);
    static void libev_write_event(EV_P_ ev_io* watcher, int revents);

   private:
    int m_seq = 0;
    char m_errstr[256];
    Log* m_logger = nullptr;
    struct ev_loop* m_loop = nullptr;
    std::unordered_map<int, kim::Connection*> m_conns;
    size_t m_saddr_len;
    struct sockaddr m_saddr;
};

}  // namespace kim

#endif  //__KIM_PRESSURE_H__