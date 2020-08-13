
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

   private:
    bool load_so(const std::string& name, const std::string& path);
    bool unload_so(const std::string& name);

   private:
    std::unordered_map<uint64_t, Module*> m_modules;  // modules.
};

}  // namespace kim

#endif  //__KIM_MODULE_MGR_H__