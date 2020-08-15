// g++ -g -std='c++11' -fPIC -rdynamic -shared module.cpp -o module.so
#include <iostream>

class Module;

#ifdef __cplusplus
extern "C" {
#endif
Module* create();
#ifdef __cplusplus
}
#endif

class Module {
   public:
    Module() {}
    Module(int a) { std::cout << "~Module(" << a << ")" << std::endl; }
    virtual ~Module() { std::cout << "~Module()" << std::endl; }
    virtual void register_handle_func() {}

   public:
    virtual void init() {}
};

class ModuleTest : public Module {
   public:
    ModuleTest();
    ModuleTest(int i) : Module(i) { std::cout << "~ModuleTest" << std::endl; }
    virtual ~ModuleTest();

   public:
    virtual void init();
};