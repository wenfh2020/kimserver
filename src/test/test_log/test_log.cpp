// g++ -g -std='c++11' test_log.cpp ../../src/util/log.cpp ../../src/util/util.cpp -o test_log && ./test_log

#include <sys/time.h>

#include <iostream>

#include "server.h"
#include "util/log.h"
#include "util/util.h"

#define MAX_CNT 100000

int main() {
    double begin, end;
    kim::Log* m_logger = new kim::Log;
    if (!m_logger->set_log_path("./test.log")) {
        std::cerr << "set log path failed" << std::endl;
        return -1;
    }

    begin = time_now();
    for (int i = 0; i < MAX_CNT; i++) {
        LOG_DEBUG("FDSAFHJDSFHARYEWYREW %s", "hello world!");
    }
    end = time_now();
    std::cout << "val: " << end - begin << std::endl
              << "avg: " << MAX_CNT / (end - begin) << std::endl;

    SAFE_DELETE(m_logger);
    return 0;
}
