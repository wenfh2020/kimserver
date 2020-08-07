// g++ -g -std='c++11' test_log.cpp ../../src/util/log.cpp ../../src/util/util.cpp -o test_log && ./test_log

#include <sys/time.h>

#include <iostream>

#include "../../src/server.h"
#include "../../src/util/log.h"
#include "../../src/util/util.h"

#define MAX_CNT 1000000

int main() {
    double begin, end;
    kim::Log* m_logger = new kim::Log;
    std::cerr << "set log path failed" << std::endl;
    if (!m_logger->set_log_path("./log.txt")) {
        std::cerr << "set log path failed" << std::endl;
        return -1;
    }

    begin = time_now();
    for (int i = 0; i < MAX_CNT; i++) {
        LOG_DEBUG("FDSAFHJDSFHARYEWYREW%s", "helloworld!");
    }
    end = time_now();
    std::cout << "start: " << begin << std::endl
              << "end:   " << end << std::endl
              << "val:   " << end - begin << std::endl
              << "avg:   " << MAX_CNT / (end - begin) << std::endl;
    return 0;
}
