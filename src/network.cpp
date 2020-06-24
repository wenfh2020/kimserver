#include "network.h"

#include <unistd.h>

#include "net/anet.h"
#include "server.h"

namespace kim {

#define TCP_BACK_LOG 511
#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
#define MAX_ACCEPTS_PER_CALL 1000

Network::Network(Log* logger)
    : m_logger(logger), m_bind_fd(0), m_gate_bind_fd(0) {
}

bool Network::create(const addr_info_t* addr_info, std::list<int>& fds) {
    if (addr_info == NULL) return false;
    int fd = -1;

    if (!addr_info->bind.empty()) {
        fd = listen_to_port(addr_info->bind.c_str(), addr_info->port);
        if (fd == -1) {
            LOG_ERROR("listen to port fail! %s:%d",
                      addr_info->bind.c_str(), addr_info->port);
            return false;
        }
        m_bind_fd = fd;
        fds.push_back(fd);
    }

    if (!addr_info->gate_bind.empty()) {
        fd = listen_to_port(addr_info->gate_bind.c_str(), addr_info->gate_port);
        if (fd == -1) {
            LOG_ERROR("listen to gate fail! %s:%d",
                      addr_info->gate_bind.c_str(), addr_info->gate_port);
            return false;
        }
        m_gate_bind_fd = fd;
        fds.push_back(fd);
    }

    return true;
}

int Network::listen_to_port(const char* bind, int port) {
    int fd = -1;
    char err[256] = {0};

    if (strchr(bind, ':')) {
        /* Bind IPv6 address. */
        fd = anet_tcp6_server(err, port, bind, TCP_BACK_LOG);
        if (fd == -1) {
            LOG_ERROR("bind tcp ipv6 fail! %s", err);
        }
    } else {
        /* Bind IPv4 address. */
        fd = anet_tcp_server(err, port, bind, TCP_BACK_LOG);
        if (fd == -1) {
            LOG_ERROR("bind tcp ipv4 fail! %s", err);
        }
    }

    if (fd != -1) {
        anet_no_block(NULL, fd);
    }

    return fd;
}

void Network::accept_tcp_handler(int fd, void* privdata) {
    char cip[NET_IP_STR_LEN];
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;

    while (max--) {
        cfd = anet_tcp_accept(m_err, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK) {
                LOG_WARNING("accepting client connection error: %s", m_err)
            }
            return;
        }
        LOG_DEBUG("accepted %s:%", cip, cport);
        // acceptCommonHandler(connCreateAcceptedSocket(cfd), 0, cip);
    }
}

}  // namespace kim