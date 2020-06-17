#include "server.h"

#include <iostream>

void init_server() {
    if (g_log == NULL) {
        g_log = new kim::Log("./server.log");
    }

    LOG_INFO("%s", "hello world\n");
}

int main(int argc, char** argv) {
    init_server();
    SAFE_DELETE(g_log);
    return 0;
}