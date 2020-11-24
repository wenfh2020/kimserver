/*
 * wenfh2020.com / 2020-07-30
 * g++ -std='c++11' -g test_jemalloc.cpp -o tjemalloc && ./tjemalloc
 * g++ -std='c++11' -g test_jemalloc.cpp -o tjemalloc -DUSE_JEMALLOC -ljemalloc && ./tjemalloc
*/

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>

#ifdef USE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif

#define MALLOC_CNT 10000000

long long mstime() {
    long long mst;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    mst = ((long long)tv.tv_sec) * 1000;
    mst += tv.tv_usec / 1000;
    return mst;
}

int main() {
    srand((unsigned)time(NULL));
    long long begin = mstime();
    for (int i = 0; i < MALLOC_CNT; i++) {
        int size = rand() % 1024 + 4;
        char* p = (char*)malloc(size);
        memset(p, rand() % 128, size);
        free(p);
    }
    long long end = mstime();

    std::cout << "begin: " << begin << std::endl
              << "end:   " << end << std::endl
              << "val:   " << end - begin << std::endl;
    return 0;
}
