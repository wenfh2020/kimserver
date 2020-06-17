#include <iostream>

#include "manager.h"
#include "util/set_proc_title.h"

void intt_server(int argc, char** argv) {
    spt_init(argc, argv);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "invalid param num!" << std::endl;
        exit(0);
    }

    intt_server(argc, argv);

    kim::Manager mgr;
    if (!mgr.init(argv[1])) {
        std::cerr << "init server fail!" << std::endl;
        exit(0);
    }
    mgr.run();

    return 0;
}