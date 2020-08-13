
#ifndef __KIM_MODULE_MGR_H__
#define __KIM_MODULE_MGR_H__

#include "module.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class ModuleMgr : Base {
   public:
    ModuleMgr(uint64_t id, Log* logger, INet* net, _cstr& name = "");
    virtual ~ModuleMgr();

    bool init(CJsonObject& config);
    Module* get_module(uint64_t id);
    Cmd::STATUS process_msg(std::shared_ptr<Request>& req);
    bool reload_so(_cstr& name);

   private:
    Module* get_module(_cstr& name);
    bool load_so(_cstr& name, _cstr& path, uint64_t id = 0);
    bool unload_so(_cstr& name);

   private:
    std::unordered_map<uint64_t, Module*> m_modules;  // modules.
};

}  // namespace kim

#endif  //__KIM_MODULE_MGR_H__