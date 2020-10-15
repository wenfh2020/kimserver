/* ./test_pressure 127.0.0.1 3355 100 100  */

#include "pressure.h"

int g_test_users = 0;
int g_test_packets = 0;

int g_server_port = 3355;
std::string g_server_host = "127.0.0.1";

ev_timer m_timer;
kim::Log* m_logger = nullptr;
kim::Pressure* g_pressure = nullptr;

void libev_timer_cb(struct ev_loop* loop, ev_timer* w, int events) {
    if (g_pressure != nullptr) {
        g_pressure->show_statics_result();
    }
}

bool check_args(int args, char** argv) {
    if (args < 4 ||
        argv[2] == nullptr || !isdigit(argv[2][0]) || atoi(argv[2]) == 0 ||
        argv[3] == nullptr || !isdigit(argv[3][0]) || atoi(argv[3]) == 0 ||
        argv[4] == nullptr || !isdigit(argv[4][0]) || atoi(argv[4]) == 0) {
        std::cerr << "invalid args!" << std::endl;
        return false;
    }

    g_server_host = argv[1];
    g_server_port = atoi(argv[2]);
    g_test_users = atoi(argv[3]);
    g_test_packets = atoi(argv[4]);
    return true;
}

// users cnt, benchmark.
int main(int args, char** argv) {
    if (!check_args(args, argv)) {
        return 1;
    }

    struct ev_loop* loop;
    kim::Log* m_logger = nullptr;

    m_logger = new kim::Log;
    if (!m_logger->set_log_path("./kimserver.log")) {
        std::cerr << "set log path failed!" << std::endl;
        return 1;
    }

    m_logger->set_level(kim::Log::LL_INFO);
    // m_logger->set_level(kim::Log::LL_DEBUG);

    loop = EV_DEFAULT;
    // ev_timer_init(&m_timer, libev_timer_cb, 1.0, 1.0);
    // ev_timer_start(loop, &m_timer);

    g_pressure = new kim::Pressure(m_logger, loop);
    if (!g_pressure->start(g_server_host.c_str(), g_server_port, g_test_users, g_test_packets)) {
        LOG_ERROR("start g_pressure failed!");
        goto error;
    }

    ev_run(loop, 0);
    SAFE_DELETE(g_pressure);
    SAFE_DELETE(m_logger);
    return 0;

error:
    SAFE_DELETE(g_pressure);
    SAFE_DELETE(m_logger);
    return 1;
}