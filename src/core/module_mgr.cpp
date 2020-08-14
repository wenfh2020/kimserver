#include "module_mgr.h"

#include <dlfcn.h>

#define MODULE_DIR "/modules/"
#define DL_ERROR() (dlerror() != nullptr) ? dlerror() : "unknown error"

namespace kim {

typedef Module* CreateModule();

ModuleMgr::ModuleMgr(uint64_t id, Log* logger, INet* net, const std::string& name)
    : Base(id, logger, net, name) {
}

ModuleMgr::~ModuleMgr() {
    Module* module;
    for (const auto& it : m_modules) {
        module = it.second;
        if (dlclose(module->get_so_handle()) == -1) {
            LOG_ERROR("close so failed! so: %s, errstr: %s",
                      module->get_name(), DL_ERROR());
        }
        SAFE_DELETE(module);
    }
    m_modules.clear();
}

bool ModuleMgr::init(CJsonObject& config) {
    std::string name, path;
    CJsonObject& array = config["modules"];

    for (int i = 0; i < array.GetArraySize(); i++) {
        name = array(i);
        path = get_work_path() + MODULE_DIR + name;
        LOG_DEBUG("loading so: %s, path: %s!", name.c_str(), path.c_str());

        if (0 != access(path.c_str(), F_OK)) {
            LOG_WARN("%s not exist!", path.c_str());
            return false;
        }

        if (!load_so(name, path)) {
            LOG_CRIT("load so: %s failed!", name.c_str());
            return false;
        }
        LOG_INFO("load so: %s ok!", name.c_str());
    }

    return true;
}

bool ModuleMgr::load_so(const std::string& name, const std::string& path, uint64_t id) {
    void* handle;
    Module* module;
    CreateModule* create_module;

    module = get_module(name);
    if (module != nullptr) {
        LOG_ERROR("duplicate load so: %s", name.c_str());
        return false;
    }

    // load so.
    handle = dlopen(path.c_str(), RTLD_NOW);
    if (handle == nullptr) {
        LOG_ERROR("open so failed! so: %s, errstr: %s", path.c_str(), DL_ERROR());
        return false;
    }

    create_module = (CreateModule*)dlsym(handle, "create");
    if (create_module == nullptr) {
        LOG_ERROR("open so failed! so: %s, errstr: %s", path.c_str(), DL_ERROR());
        if (dlclose(handle) == -1) {
            LOG_ERROR("close so failed! so: %s, errstr: %s",
                      module->get_name(), DL_ERROR());
        }
        return false;
    }

    module = (Module*)create_module();
    id = (id != 0) ? id : m_net->get_new_seq();
    if (!module->init(m_logger, m_net, id, name)) {
        LOG_ERROR("init module failed! module: %s", name.c_str());
        if (dlclose(handle) == -1) {
            LOG_ERROR("close so failed! so: %s, errstr: %s",
                      module->get_name(), DL_ERROR());
        }
        return false;
    }

    module->set_name(name);
    module->set_so_path(path);
    module->set_so_handle(handle);
    m_modules[id] = module;
    LOG_INFO("load so: %s done!", name.c_str());
    return true;
}

bool ModuleMgr::reload_so(const std::string& name) {
    uint64_t id = 0;
    Module* module = nullptr;
    std::string path = get_work_path() + MODULE_DIR + name;
    LOG_DEBUG("reloading so: %s, path: %s!", name.c_str(), path.c_str());

    if (0 != access(path.c_str(), F_OK)) {
        LOG_WARN("%s not exist!", path.c_str());
        return false;
    }

    module = get_module(name);
    if (module != nullptr) {
        id = module->get_id();
    }

    unload_so(name);
    return load_so(name, path, id);
}

bool ModuleMgr::unload_so(const std::string& name) {
    Module* module = get_module(name);
    if (module == nullptr) {
        LOG_ERROR("find so: %s failed!", name.c_str());
        return false;
    }

    if (dlclose(module->get_so_handle()) == -1) {
        LOG_ERROR("close so failed! so: %s, errstr: %s",
                  module->get_name(), DL_ERROR());
    }

    auto it = m_modules.find(module->get_id());
    if (it != m_modules.end()) {
        m_modules.erase(it);
    } else {
        LOG_ERROR("find module: %s failed!", name.c_str());
    }
    SAFE_DELETE(module);

    LOG_INFO("unload module so: %s", name.c_str());
    return true;
}

Module* ModuleMgr::get_module(uint64_t id) {
    auto it = m_modules.find(id);
    return (it != m_modules.end()) ? it->second : nullptr;
}

Module* ModuleMgr::get_module(const std::string& name) {
    Module* module = nullptr;
    for (const auto& it : m_modules) {
        module = it.second;
        if (module->get_name() == name) {
            break;
        }
    }
    return module;
}

Cmd::STATUS ModuleMgr::process_msg(std::shared_ptr<Request>& req) {
    Module* module;
    Cmd::STATUS cmd_stat;
    for (const auto& it : m_modules) {
        module = it.second;
        LOG_DEBUG("module name: %s", module->get_name());
        cmd_stat = module->process_message(req);
        if (cmd_stat != Cmd::STATUS::UNKOWN) {
            return cmd_stat;
        }
    }
    return Cmd::STATUS::UNKOWN;
}

}  // namespace kim