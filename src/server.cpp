#include "server.h"

#include <fcntl.h>

#include "manager.h"
#include "util/set_proc_title.h"

// #define DAEMONSIZE 1

void daemonize(void) {
    int fd;

    if (fork() != 0) exit(0); /* parent exits */
    setsid();                 /* create a new session */

    /* Every output goes to /dev/null. If server is daemonized but
     * the 'logfile' is set to 'stdout' */
    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
}

void init_server(int argc, char** argv) {
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    spt_init(argc, argv);
#ifdef DAEMONSIZE
    daemonize();
#endif
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "invalid param num!" << std::endl;
        exit(-1);
    }

    init_server(argc, argv);

    kim::Manager mgr;
    kim::Log* logger = new kim::Log;
    if (!mgr.init(argv[1], logger)) {
        delete logger;
        exit(-1);
    }
    mgr.run();
    delete logger;
    return 0;
}