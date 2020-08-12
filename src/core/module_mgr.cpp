#include "module_mgr.h"

#include <dlfcn.h>

#define MODULE_DIR "/modules/"

namespace kim {

typedef Module* CreateModule();

ModuleMgr::ModuleMgr(uint64_t id, Log* logger, INet* net, _cstr& name)
    : Base(id, logger, net, name) {
}

ModuleMgr::~ModuleMgr() {
    char* errstr;
    for (const auto& it : m_modules) {
        dlclose(it.second->get_so_handle());
        errstr = dlerror();
        if (errstr != nullptr) {
            LOG_ERROR("close so failed! so: %s, errstr: %s",
                      it.second->get_name(), errstr);
        }
        delete it.second;
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

bool ModuleMgr::load_so(const std::string& name, const std::string& path) {
    uint64_t id;
    char* errstr;
    void* handle;
    Module* module;

    // check duplicate.
    for (const auto& it : m_modules) {
        module = it.second;
        if (module->get_name() == name) {
            LOG_ERROR("duplicate load so: %s", name.c_str());
            return false;
        }
    }

    // load so.
    handle = dlopen(path.c_str(), RTLD_NOW | RTLD_NODELETE);
    errstr = dlerror();
    if (errstr != nullptr) {
        LOG_ERROR("open so failed! so: %s, errstr: %s", path.c_str(), errstr);
        return false;
    }

    CreateModule* create_module = (CreateModule*)dlsym(handle, "create");
    errstr = dlerror();
    if (errstr != nullptr) {
        LOG_ERROR("open so failed! so: %s, errstr: %s", path.c_str(), errstr);
        dlclose(handle);
        return false;
    }

    module = (Module*)create_module();
    id = m_net->get_new_seq();
    if (!module->init(m_logger, m_net, id, name)) {
        dlclose(handle);
        LOG_ERROR("init module failed! module: %s", name.c_str());
        return false;
    }

    module->set_name(name);
    module->set_so_path(path);
    module->set_so_handle(handle);
    m_modules[id] = module;
    LOG_INFO("load so: %s success!", name.c_str());
    return true;
}

bool ModuleMgr::unload_so(const std::string& name) {
    char* errstr;
    Module* module = nullptr;

    for (const auto& it : m_modules) {
        module = it.second;
        if (module->get_name() == name) {
            break;
        }
    }

    if (module == nullptr) {
        LOG_ERROR("find so: %s failed!", name.c_str());
        return false;
    }

    dlclose(module->get_so_handle());
    errstr = dlerror();
    if (errstr != nullptr) {
        LOG_ERROR("open so failed! so: %s, errstr: %s.", module->get_so_path(), errstr);
    }

    auto it = m_modules.find(module->get_id());
    if (it != m_modules.end()) {
        m_modules.erase(it);
    } else {
        LOG_ERROR("find module: %s in map failed!", name.c_str());
    }
    SAFE_DELETE(module);

    LOG_INFO("unload module so: %s", name.c_str());
    return true;
}

Module* ModuleMgr::get_module(uint64_t id) {
    auto it = m_modules.find(id);
    return (it != m_modules.end()) ? it->second : nullptr;
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