// http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#code_ev_timer_code_relative_and_opti
// g++ -std='c++11' -g -Wno-noexcept-type test_timer.cpp -o timer -lev && ./timer

#include <ev.h>
#include <stdio.h>

#include <iostream>
#include <memory>

class Connection {
   public:
    Connection() { std::cout << "Connection()" << std::endl; }
    virtual ~Connection() { std::cout << "~Connection()" << std::endl; }

   public:
    std::string m_data;
};

class ConnectionData {
   public:
    ConnectionData() { std::cout << "ConnectionData()" << std::endl; }
    virtual ~ConnectionData() { std::cout << "~ConnectionData()" << std::endl; }

   public:
    Connection* m_conn;
};

ev_timer timer;

// another callback, this time for a time-out
static void
timeout_cb(EV_P_ ev_timer* w, int revents) {
    puts("timeout");
    ConnectionData* conn_data = (ConnectionData*)w->data;
    if (conn_data != nullptr) {
        std::cout << conn_data->m_conn->m_data << std::endl;
    }

    // this causes the innermost ev_run to stop iterating
    ev_break(EV_A_ EVBREAK_ONE);
    delete conn_data;
}

int main(int argc, char** argv) {
    Connection* c = std::make_shared<Connection>();
    c->m_data = "hello world!";
    ConnectionData* conn_data = new ConnectionData;
    conn_data->m_conn = c;

    timer.data = conn_data;
    // use the default event loop unless you have special needs
    struct ev_loop* loop = EV_DEFAULT;

    // initialise a timer watcher, then start it
    // simple non-repeating 5.5 second timeout
    ev_timer_init(&timer, timeout_cb, 5.5, 1.0);
    ev_timer_start(loop, &timer);

    // now wait for events to arrive
    ev_run(loop, 0);

    // break was called, so exit
    return 0;
}
