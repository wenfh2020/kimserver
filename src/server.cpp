#include <iostream>

#include "manager.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "invalid param num!" << std::endl;
        exit(0);
    }

    kim::Manager mgr;
    if (!mgr.init(argv[1])) {
        std::cerr << "init server fail!" << std::endl;
        exit(0);
    }

    mgr.run();
    return 0;
}