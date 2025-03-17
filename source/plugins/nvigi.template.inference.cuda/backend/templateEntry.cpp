// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "nvigi.h"
#include "nvigi.api/internal.h"
#include "nvigi.log/log.h"
#include "nvigi.exception/exception.h"
#include "nvigi.plugin/plugin.h"
#include "versions.h"
#include "../nvigi_template_infer.h"
#include "source/utils/nvigi.ai/ai.h"
#include "source/utils/nvigi.hwi/cuda/push_poppable_cuda_context.h"
#include "_artifacts/gitVersion.h"

namespace nvigi
{

namespace tmpl_infer_cuda
{

//! Our common context
//! 
//! Here we can keep whatever global state we need
//! 
struct TemplateInferContext
{
    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(TemplateInferContext)

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
    ITemplateInferCuda api{};

    // NOTE: No instance data should be here
};

struct MyInstanceData
{
#ifdef NVIGI_WINDOWS
    MyInstanceData(const nvigi::NVIGIParameter* params)
        : cudaContext(params)
#else
    MyInstanceData(const nvigi::NVIGIParameter*)
#endif
    {}

    // All per instance data goes here

    // All plugins that use CUDA must use PushPoppableCudaContext to work 
    // correctly with CIG (CUDA in Graphics)
#ifdef NVIGI_WINDOWS
    PushPoppableCudaContext cudaContext;
#endif
};

} // tmpl

//! Define our plugin
//! 
//! IMPORTANT: Make sure to place this macro right after the context declaration and always within the 'nvigi' namespace ONLY.
NVIGI_PLUGIN_DEFINE("nvigi.plugin.template.inference.cuda", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(API_MAJOR, API_MINOR, API_PATCH), tmpl_infer_cuda, TemplateInferContext)

//! Example interface implementation
//! 
nvigi::Result tmplCreateInstance(const nvigi::TemplateInferCudaCreationParameters& params, nvigi::InferenceInstance** _instance)
{
    //! An example showing how to obtain an optional chained parameters
    //! 
    //! User can chain extra structure(s) using the params._next
    //! 
    auto extraParams = findStruct<nvigi::TemplateInferCudaCreationParametersEx>(params);
    if (extraParams)
    {
        //! User provided extra parameters!
    }

    
    //! Information about our model(s)
    json modelInfo;
    //! Note that we can also look for additional models (if provided by host).
    //! This would be scenario where extra models are downloaded by user or other
    //! packages so here we have a choice of using one or the other or combining
    //! models as needed (for example, use TRT engine from one location and new
    //! weights from another)    
    //!
    //! Host must provide 'params.common->utf8PathToAdditionalModels'
    json additionalModelInfo;
    //! Find our model(s) - make sure to rename path and extension(s) accordingly
    if (!ai::findModels(params.common, { "my_ext1", "my_ext2" }, modelInfo, &additionalModelInfo))
    {
        // Failed to find models
        return kResultInvalidParameter;
    }

    // Now get our files
    std::vector<std::string> files1 = modelInfo[params.common->modelGUID]["my_ext1"];
    if (files1.empty())
    {
        NVIGI_LOG_ERROR("Failed to find model in the expected directory '%s'", params.common->utf8PathToModels);
        return kResultInvalidParameter;
    }

    std::vector<std::string> files2 = modelInfo[params.common->modelGUID]["my_ext2"];
    if (files2.empty())
    {
        NVIGI_LOG_ERROR("Failed to find model in the expected directory '%s'", params.common->utf8PathToModels);
        return kResultInvalidParameter;
    }

    //! We can proceed and create our instance using the model information
    //! 
    //! Look at gpt/asr or other plugins for a guidance
    
    auto instanceData = new tmpl_infer_cuda::MyInstanceData(params);

    auto instance = new InferenceInstance();
    instance->data = instanceData;

    // TODO: implement and assign your API
    //instance->getFeatureId = tmpl_infer_cuda::getFeatureId;
    //instance->getInputSignature = tmpl_infer_cuda::getInputSignature;
    //instance->getOutputSignature = tmpl_infer_cuda::getOutputSignature;
    //instance->evaluate = tmpl_infer_cuda::evaluate;

    *_instance = instance;

    return kResultOk;
}

nvigi::Result tmplDestroyInstance(const nvigi::InferenceInstance* instance)
{
    if (!instance) return kResultInvalidParameter;
    auto data = static_cast<tmpl_infer_cuda::MyInstanceData*>(instance->data);
    
    // Do any other cleanup here

    delete data;
    delete instance;

    return kResultOk;
}

nvigi::Result tmplGetCapsAndRequirements(nvigi::TemplateInferCudaCapabilitiesAndRequirements* /*modelInfo*/, const nvigi::TemplateInferCudaCreationParameters* /*params*/)
{
    return kResultOk;
}

//! Making sure our implementation is covered with our exception handler
//! 
namespace tmpl_infer_cuda
{
nvigi::Result createInstance(const nvigi::TemplateInferCudaCreationParameters& params, nvigi::InferenceInstance** instance)
{
    NVIGI_CATCH_EXCEPTION(tmplCreateInstance(params, instance));
}

nvigi::Result destroyInstance(const nvigi::InferenceInstance* instance)
{
    NVIGI_CATCH_EXCEPTION(tmplDestroyInstance(instance));
}

nvigi::Result getCapsAndRequirements(nvigi::TemplateInferCudaCapabilitiesAndRequirements* modelInfo, const nvigi::TemplateInferCudaCreationParameters* params)
{
    NVIGI_CATCH_EXCEPTION(tmplGetCapsAndRequirements(modelInfo, params));
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

    info.id = plugin::tmpl_infer_cuda::kId;
    info.description = "template plugin implementation";
    info.author = "NVIDIA";
    info.build = GIT_BRANCH_AND_LAST_COMMIT;
    info.interfaces = { plugin::getInterfaceInfo<ITemplateInferCuda>() };

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

    auto& ctx = (*tmpl_infer_cuda::getContext());

    //! Add your interface(s) to the framework
    //!
    //! Note that we are exporting functions with the exception handler enabled
    ctx.api.createInstance = tmpl_infer_cuda::createInstance;
    ctx.api.destroyInstance = tmpl_infer_cuda::destroyInstance;
    ctx.api.getCapsAndRequirements = tmpl_infer_cuda::getCapsAndRequirements;

    framework->addInterface(plugin::tmpl_infer_cuda::kId, &ctx.api, 0);

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
    // auto& ctx = (*tmpl_infer_cuda::getContext());

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
