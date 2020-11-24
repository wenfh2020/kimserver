
#ifndef __KIM_MODULE_MGR_H__
#define __KIM_MODULE_MGR_H__

#include "module.h"
#include "util/json/CJsonObject.hpp"

namespace kim {

class ModuleMgr : Base {
   public:
    ModuleMgr(uint64_t id, Log* logger, INet* net, const std::string& name = "");
    virtual ~ModuleMgr();

    bool init(CJsonObject& config);
    Module* get_module(uint64_t id);

    Cmd::STATUS process_req(const Request& req);
    Cmd::STATUS process_ack(Request& req);
    bool reload_so(const std::string& name);

   private:
    Module* get_module(const std::string& name);
    bool load_so(const std::string& name, const std::string& path, uint64_t id = 0);
    bool unload_so(const std::string& name);

   private:
    std::unordered_map<uint64_t, Module*> m_modules;  // modules.
};

}  // namespace kim

#endif  //__KIM_MODULE_MGR_H__