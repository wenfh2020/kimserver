// g++ -g  -std='c++11' test_so.cpp -o tso && ./tso
#include <dlfcn.h>

#include <iostream>

#include "so/module.h"

typedef Module* CreateModule();

int main() {
    char* errstr;
    void* handle;

    handle = dlopen("./so/module.so", RTLD_NOW);
    errstr = dlerror();
    if (errstr != nullptr) {
        std::cerr << "open so failed! errstr: " << errstr << std::endl;
        return 1;
    }

    CreateModule* p = (CreateModule*)dlsym(handle, "create");
    errstr = dlerror();
    if (errstr != nullptr) {
        std::cerr << "create module failed! errstr: " << errstr << std::endl;
        dlclose(handle);
        return 1;
    }

    Module* module = (Module*)p();
    module->init();
    delete module;
    if (dlclose(handle) == -1) {
        std::cerr << "dlclose failed!" << std::endl;
    }
    return 0;
}