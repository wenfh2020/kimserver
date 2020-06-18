
#include "server.h"

#include "manager.h"
#include "util/set_proc_title.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "invalid param num!" << std::endl;
        exit(-1);
    }

    spt_init(argc, argv);

    kim::Manager mgr;
    if (!mgr.init(argv[1])) {
        exit(-1);
    }

    mgr.run();
    return 0;
}