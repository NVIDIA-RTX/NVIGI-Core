// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "nvigi.h"
#include "nvigi.api/internal.h"
#include "nvigi.log/log.h"
#include "nvigi.exception/exception.h"
#include "nvigi.plugin/plugin.h"
#include "versions.h"
#include "../nvigi_template.h"
#include "_artifacts/gitVersion.h"

namespace nvigi
{

namespace tmpl
{

//! Our common context
//! 
//! Here we can keep whatever global state we need
//! 
struct TemplateContext
{
    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(TemplateContext)

    // Called when plugin is loaded, do any custom constructor initialization here
    void onCreateContext()
    {
        //! For example
        //!
        //! onHeap = new std::map<...>
    };

    // Called when plugin is unloaded, destroy any objects on heap here
    void onDestroyContext()
    {
        //! For example
        //!
        //! delete onHeap;
    };

    // For example, interface we will export
    ITemplate api{};
};

} // tmpl

//! Define our plugin
//! 
//! IMPORTANT: Make sure to place this macro right after the context declaration and always within the 'nvigi' namespace ONLY.
NVIGI_PLUGIN_DEFINE("nvigi.plugin.template", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(API_MAJOR, API_MINOR, API_PATCH), tmpl, TemplateContext)

//! Example interface implementation
//! 
nvigi::Result tmplFunA()
{
    return kResultOk;
}

//! Making sure our implementation is covered with our exception handler
//! 
namespace tmpl
{
nvigi::Result funA(...)
{
    NVIGI_CATCH_EXCEPTION(tmplFunA());
}

} // namespace tmpl

//! Main entry point - get information about our plugin
//! 
Result nvigiPluginGetInfo(nvigi::framework::IFramework* framework, nvigi::plugin::PluginInfo** _info)
{
    //! IMPORTANT: 
    //! 
    //! NO HEAVY LIFTING (MEMORY ALLOCATIONS, GPU RESOURCE CREATION ETC) IN THIS METHOD 
    //! 
    //! JUST PROVIDE INFORMATION REQUESTED AS QUICKLY AS POSSIBLE
    //! 

    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;
    
    // Internal API, we know that incoming pointer is always valid
    auto& info = plugin::getContext()->info;
    *_info = &info;

    info.id = plugin::tmpl::kIdBackendApi;
    info.description = "template plugin implementation";
    info.author = "NVIDIA";
    info.build = GIT_BRANCH_AND_LAST_COMMIT;
    info.interfaces = { plugin::getInterfaceInfo<ITemplate>() };

    //! Specify minimum spec for the OS, driver and GPU architecture
    //! 
    //! Defaults indicate no restrictions - plugin can run on any system
    info.minDriver = {};
    info.minOS = {};
    info.minGPUArch = {};

    return kResultOk;
}

//! Main entry point - starting our plugin
//! 
Result nvigiPluginRegister(framework::IFramework* framework)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;

    auto& ctx = (*tmpl::getContext());

    //! Add your interface(s) to the framework
    //!
    //! Note that we are exporting functions with the exception handler enabled
    ctx.api.funA = tmpl::funA;

    framework->addInterface(plugin::tmpl::kIdBackendApi, &ctx.api, 0);

    //! Obtain interfaces from other plugins or core (if needed)
    //! 
    //! For example, here we use our internal helper function to obtain a GPT interface (a rate example of a plugin using another plugin)
    //! 
    //! NOTE: This will trigger loading of the 'nvigi.plugin.gpt.ggml.cuda' plugin if it wasn't loaded already
    //! 
    //! 
    //! nvigi::IGeneralPurposeTransformer* igpt{};
    //! if (!getInterface(framework, nvigi::plugin::gpt::ggml::cuda::kId, &igpt))
    //! {
    //!     NVIGI_LOG_ERROR("Failed to obtain interface");
    //! }
    //! else
    //! {
    //!     if (net->getVersion() >= kStructVersion2)
    //!     {
    //!         // OK to access v2 members
    //!     }
    //!     if (net->getVersion() >= kStructVersion3)
    //!     {
    //!         // OK to access v3 members
    //!     }
    //! }

    //! Do any other startup tasks here
    //! 
    //! IMPORTANT: 
    //! 
    //! NO HEAVY LIFTING WHEN REGISTERING PLUGINS
    //! 
    //! Use your interface(s) to create instance(s) or context(s)
    //! then do all initialization, memory allocations etc. within
    //! the API which is exposed with your interface(s).
    
    return kResultOk;
}

//! Main exit point - shutting down our plugin
//! 
Result nvigiPluginDeregister()
{
    //auto& ctx = (*tmpl::getContext());

    //! Do any other shutdown tasks here
    //!
    //! IMPORTANT: 
    //! 
    //! NO HEAVY LIFTING WHEN DE-REGISTERING PLUGINS
    //! 
    //! Use your interface(s) to destroy instance(s) or context(s)
    //! then do all de-initialization, memory de-allocations etc. within
    //! the API which is exposed with your interface(s).
    return kResultOk;
}

//! The only exported function - gateway to all functionality
NVIGI_EXPORT void* nvigiPluginGetFunction(const char* functionName)
{
    //! Core API
    NVIGI_EXPORT_FUNCTION(nvigiPluginGetInfo);
    NVIGI_EXPORT_FUNCTION(nvigiPluginRegister);
    NVIGI_EXPORT_FUNCTION(nvigiPluginDeregister);

    return nullptr;
}

}
