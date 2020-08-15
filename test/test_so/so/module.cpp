#include "module.h"

#ifdef __cplusplus
extern "C" {
#endif
Module* create() {
    return (new ModuleTest());
}
#ifdef __cplusplus
}
#endif

/////////////////////////////////////////////

ModuleTest::ModuleTest() {
    std::cout << "ModuleTest()" << std::endl;
}
ModuleTest::~ModuleTest() {
    std::cout << "~ModuleTest()" << std::endl;
}

void ModuleTest::init() {
    std::cout << "init()" << std::endl;
}