# Architecture

NVIGI stands for NVIDIA In-Game Inferencing. It is a plugin manager that provides a simple way to obtain **typed and versioned** C style interfaces which can implement range of arbitrary algorithms and features.

Here are some key points about NVIGI:

* flexible and backwards compatible external and internal **explicit** API
* allows independent plugin development and deployment
* mix and match of different plugins from different SDK is totally fine
* each plugin can implement one or more interfaces as needed

## Typed And Versioned Structures

Each structure is assigned a GUID to represent its type and predefined constant `kStructVersionN` to represent its version as shown in this code snippet:

```cpp
//! Inference instance 
//! 
//! {AD9DC29C-0A89-4A4E-B900-A7183B48336E}
struct alignas(8) InferenceInstance
{
    NVIGI_UID(UID({ 0xad9dc29c, 0xa89, 0x4a4e, { 0xb9, 0x0, 0xa7, 0x18, 0x3b, 0x48, 0x33, 0x6e } }), kStructVersion1);
    ...       
}
// Performs static asserts to ensure given structure is C ABI compatible
NVIGI_VALIDATE_STRUCT(InferenceInstance);
```

Once shipped, structures cannot be modified other than appending new members **at the end and increasing the version**. Structures can be used as C style interfaces or simple data containers as needed.

> IMPORTANT:
> When creating new structures always use the [provided utility executable](./Development.md#guid-creation-for-interfaces-and-plugins)

### Interfaces

Typed and versioned structures which contain **a backwards compatible C style API** are called `interfaces`. Here is an example:

```cpp
//! Inference instance 
//! 
//! Contains in/out signatures and the inference execution method
//! 
// {AD9DC29C-0A89-4A4E-B900-A7183B48336E}
struct alignas(8) InferenceInstance
{
    NVIGI_UID(UID({ 0xad9dc29c, 0xa89, 0x4a4e, { 0xb9, 0x0, 0xa7, 0x18, 0x3b, 0x48, 0x33, 0x6e } }), kStructVersion1);

    //! Instance data, must be passed as input to all functions below
    InferenceInstanceData* data;

    //! Returns feature Id e.g. LLM, ASR etc.
    uint32_t (*getFeatureId)(InferenceInstanceData* data);

    //! Returns an array of descriptors for the input data expected by this instance
    const InferenceDataDescriptorArray* (*getInputSignature)(InferenceInstanceData* data);

    //! Returns an array of descriptors for the output data provided by this instance
    const InferenceDataDescriptorArray* (*getOutputSignature)(InferenceInstanceData* data);

    //! Evaluates provided execution context
    nvigi::Result(*evaluate)(nvigi::InferenceExecutionContext* execCtx);

    //! NEW MEMBERS GO HERE, BUMP THE VERSION!
};
```

## Core

The NVIGI core component `nvigi.core.framework` scans directories specified by the host for any NVIGI plugins, enumerates them and maps the plugin feature ids with the interfaces they implement. Plugin manager
ensures that plugins are digitally signed, supported on the system and that correct API version is used so we don't end up loading a plugin with the newer API.

> IMPORTANT:
> A plugin is started **only** upon a specific request for an interface implemented by the plugin in question. Request is made by the host app or another plugin.

## Internal API

Each plugin implements a rather simple API. Here are the core methods:

Exported:

* `nvigiPluginGetFunction`

Not exported but rather fetched via `nvigiPluginGetFunction`:

Core API

* `nvigiPluginGetInfo` - provides information about the plugin
* `nvigiPluginRegister` - registers interface(s) with the framework
* `nvigiPluginDeregister` - de-registers interface(s) and performs cleanup

> IMPORTANT:
> `nvigiPlugin{De}Register` should NOT contain any `heavy lifting` in terms of memory allocations or any complex actions. **Exported interfaces should handle all that**.

## Versioning

We track two versions:

* Plugin version
* API version

API version is increased if plugin exports new methods through `nvigiPluginGetFunction` or if the core API is modified in any way. For any other changes we increase the plugin version.

## Plugin Structure

First we need to define our global context containing data we need during the plugin's lifecycle, here is an example from the template plugin:

```cpp
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
    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(TemplateContext);

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

} // namespace tmpl
```

Now we define our plugin by specifying the name, version, API version and our context:

```cpp
//! Define our plugin, make sure to update version numbers in versions.h
NVIGI_PLUGIN_DEFINE("nvigi.plugin.template", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), tmpl, TemplateContext)
```

> **NOTE:**
> All plugin names must be in `nvigi.plugin.$name.$backend{.$api}` format and generated modules must follow the same naming convention. See existing plugins for examples.

Next we specify core function which provides information about our plugin:

```cpp
//! Main entry point - get information about our plugin
//! 
Result nvigiPluginGetInfo(framework::IFramework* framework, nvigi::plugin::PluginInfo** _info)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;

    // Internal API, we know that incoming pointer is always valid
    auto& info = plugin::getContext()->info;
    *_info = &info;

    //! Essential information, plugin id, API and implementation versions
    info.id = nvigi::plugin::tmpl::backend::api::kId;
    
    //! Provide additional information about the plugin
    info.description = "my plugin for doing some awesome stuff";
    info.author = "NVIDIA";
    info.build = GIT_BRANCH_AND_LAST_COMMIT;
    info.interfaces = { plugin::getInterfaceInfo<ITemplate>() }; // List GUIDs for interface(s) this plugin is going to export

    //! Specify minimum spec for the OS, driver and GPU architecture
    //! 
    //! Defaults indicate no restrictions - plugin can run on any system, even without any adapter
    info.requiredVendor = nvigi::VendorId::eNone;
    info.minDriver = {};
    info.minOS = {};
    info.minGPUArch = {};

    return kResultOk;
}
```

The following methods are required for the API v0.0.1 but please note that `they are NOT exported directly` but rather fetched via `nvigiPluginGetFunction`:

```cpp
//! Main entry point - starting our plugin
//! 
Result nvigiPluginRegister(framework::IFramework* framework)
{
    //! IMPORTANT: 
    //! 
    //! NO HEAVY LIFTING (MEMORY ALLOCATIONS, GPU RESOURCE CREATION ETC) IN THIS METHOD 
    //! 
    //! JUST PROVIDE INFORMATION REQUESTED AS QUICKLY AS POSSIBLE
    //! 
    
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;

    auto& ctx = (*tmpl::getContext());

    //! Add your interface(s) to the framework
    //!
    
    ctx.api.createInstance = tmpl::createInstance;
    ctx.api.destroyInstance = tmpl::destroyInstance;
    ctx.api.getCapsAndRequirements = tmpl::getCapsAndRequirements;

    framework->addInterface(nvigi::plugin::tmpl::backend::api::kId, &ctx.api);

    //! Obtain interfaces from other plugins or core (if needed)
    //! 
    //! For example, here we use our internal helper function to obtain networking interface
    //! 
    //! NOTE: This will trigger loading of the 'nvigi.net' plugin if it wasn't loaded already
    //! 
    net::INet* net{};
    if (!getInterface(framework, nvigi::plugin::net::kId, &net))
    {
        NVIGI_LOG_ERROR("Failed to obtain interface");
    }
    else
    {
        //! IMPORTANT: Check version before accessing v2+ members
        if (net->getVersion() >= kStructVersion2)
        {
            // OK to access v2 members
        }
        if (net->getVersion() >= kStructVersion3)
        {
            // OK to access v3 members
        }
    }
    
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
    auto& ctx = (*tmpl::getContext());

    //! Remove your interface(s) from the framework
    //!
    plugin::getContext()->framework->removeInterface(nvigi::plugin::tmpl::backend::api::kId, ITemplate::s_type);
    
    //! Do any other shutdown tasks here
    //!
    //! IMPORTANT: 
    //! 
    //! NO HEAVY LIFTING WHEN DE-REGISTERING PLUGINS
    //! 
    //! Use your interface(s) to destroy instance(s) or context(s)
    //! then do all de-initialization, memory de-allocations etc. within
    //! the API which is exposed with your interface(s)., *

    return kResultOk;
}
```

Finally we export a single function `nvigiPluginGetFunction` which is responsible for fetching the rest of the API:

```cpp
//! The only exported function - gateway to all functionality
NVIGI_EXPORT void* nvigiPluginGetFunction(const char* functionName)
{
    //! Core API
    NVIGI_EXPORT_FUNCTION(nvigiPluginGetInfo);
    NVIGI_EXPORT_FUNCTION(nvigiPluginRegister);
    NVIGI_EXPORT_FUNCTION(nvigiPluginDeregister);

    return nullptr;
}

```

## External API

When it comes to the external API, NVIGI follows a simple concept of loading and unloading typed and versioned C style interfaces based on the requests coming from the host application. Here are the details:

### Initialization

NVIGI framework is initialized with the following API:

```cpp
//! Initializes the NVIGI framework
//!
//! Call this method when your application is initializing
//!
//! @param pref Specifies preferred behavior for the NVIGI framework (NVIGI will keep a copy)
//! @param sdkVersion Current SDK version
//! @returns IResult interface with code nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiInit(const nvigi::Preferences &pref, uint64_t sdkVersion = nvigi::kSDKVersion);
```

> IMPORTANT:
> After calling nvigiInit **no plugins are started** - they are simply enumerated and ready to provide interfaces on demand.

### Interface loading

When a specific interface is needed the following API can be used to obtain it:

```cpp
//! Loads an interface for a specific NVIGI feature
//!
//! Call this method when specific interface is needed.
//!
//! NOTE: Interfaces are reference counted so they all must be released before underlying plugin is released.
//!
//! @param feature Specifies feature which needs to provide the requested interface
//! @param interfaceType Type of the interface to obtain
//! @param interfaceVersion Minimal version of the interface to obtain
//! @param interface Pointer to the interface
//! @param utf8PathToPlugin Optional path to a new plugin which provides this interface
//! @returns nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! NOTE: It is recommended to use the template based helpers `nvigiGetInterface` or `nvigiGetInterfaceDynamic` (see below in this header)
//! 
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiLoadInterface(nvigi::PluginID feature, const nvigi::UID& interfaceType, uint32_t interfaceVersion, void** _interface, const char* utf8PathToPlugin);
```
> IMPORTANT: This API allows new plugins to be loaded and registered **after** `nvigiInit` has been called

### Interface unloading

When a specific interface is no longer needed the following API can be used to unload it and free all resources associated with it:

```cpp
//! Unloads an interface for a specific NVIGI feature
//!
//! Call this method when specific interface is no longer needed
//!
//! @param feature Specifies feature which provided the interface
//! @param interface Pointer to the interface
//! @returns IResult interface with code nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiUnloadInterface(nvigi::UID feature, void* _interface);
```

> NOTE:
> If the unloaded interface was the last interface implemented by a plugin, the plugin in question will be unloaded and shutdown sequence for it will be triggered.

### Shutdown

When NVIGI is no longer needed the following API will unload any existing interfaces and destroy the framework:

```cpp
//! Shuts down the NVIGI module
//!
//! Call this method when your application is shutting down. 
//!
//! @returns IResult interface with code nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiShutdown();
```
