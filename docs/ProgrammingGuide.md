
# NVIGI - Programming Guide

This guide primarily focuses on the general use of the In-Game Inferencing, including how to initialize it and utilize its interfaces.

> **IMPORTANT**: This guide might contain pseudo code, for the up to date implementation and source code which can be copy pasted please see the [basic sample](../source/samples/nvigi.basic/basic.cpp)

## Version 1.1.0 Release

## Table of Contents
- [Introduction](#introduction)
- [Key Concepts](#key-concepts)
- [Typed And Versioned Structures](#typed-and-versioned-structures)
- [Thread Safety](#thread-safety)
- [Security](#security)  
- [Core API](#core-api)
  - [Initialization](#initialization)  
  - [Shutdown](#shutdown)
  - [Interfaces](#interfaces)
- [Validation](#validation)  
- [3rd Party Dependencies](#3rd-party-dependencies)

## INTRODUCTION

NVIGI functions as a plugin manager that provides `secure plugin loading` and an `explicit` API, giving the host application complete control over every aspect of NVIGI's usage.

## Key Concepts

* Typed and version structures are used for all data and C-style interfaces. 
    * Structures can be chained together as needed
    * Structures are ABI and backwards compatible
    * Structures can contain C-style API (aka interfaces) or just plain data (aka parameters or properties)
* Each plugin implements at least one C-style interface which is provided to the host application and other plugins to use
* Interfaces are completely custom and it is up to the plugins to decide what functionality is needed in them
* Different plugins can implement identical interfaces if needed, normally they would use different `backends` to provide same functionality
* Plugins are developed independently and can be updated independently (as needed) in any application (even after it has shipped)
* Plugins have unique identifiers in the following namespace format `nvigi::plugin::$name{::$backend::$api}::kId` where backend and API are optional
* The core component `nvigi.core.framework` enumerates all available plugins, determines which plugins can run on user's system and what interfaces do they export 
  * Not all plugin locations need to be known when `nvigi.core.framework` is initialized
  * However, all shared dependencies must be known and stored in a single location

## Typed And Versioned Structures

Typed and versioned structures can be used to provide input data, obtain output results and call custom C-style APIs.

### Input Data

In this case, the host application provides information to NVIGI. Depending on scenario, multiple inputs (structures) can be chained together and later sought for with the `findStruct` API. Here are some examples:
```cpp

//! We are creating local GPT instance while providing common and D3D12 related information

//! Common
nvigi::CommonCreationParameters common{};
common.numThreads = myNumCPUThreads; // How many CPU threads is instance allowed to use 
common.vramBudgetMB = myVRAMBudget;  // How much VRAM is instance allowed to occupy
common.utf8PathToModels = myPathToNVIGIModelRepository; // Path to provided NVIGI model repository (using UTF-8 encoding)
common.modelGUID = "{175C5C5D-E978-41AF-8F11-880D0517C524}"; // Model GUID, for details please see NVIGI models repository

//! GPT specific 
nvigi::GPTCreationParameters params{};
params.maxNumTokensToPredict = 200;
params.contextSize = 512;
params.seed = -1;
```
Note that we are chaining common properties to our main properties like this:
```cpp
    if(NVIGI_FAILED(params.chain(common)))
    {
        // Handle error
    }
```
Now we provide additional information about D3D12 context, assuming the host application is using D3D graphics API:
```cpp
//! D3D12 specific 
nvigi::D3D12Parameters d3d12Params{};
d3d12Params.device = myDevice;
d3d12Params.queue = myQueue;
```
Then we chain it together like this:
```cpp
    if(NVIGI_FAILED(params.chain(d3d12Parameters)))
    {
        // Handle error
    }
```
Finaly, we end up with the following chained input structure which can be provided to NVIGI inteface(s) for processing:
```cpp
nvigi::GPTCreationParameters -> nvigi::D3D12Parameters -> nvigi::CommonCreationParameters
```
NVIGI plugins, in this case GPT, will use `findStruct` API to find all mandatory and optional inputs which are chained together.

### Output Data

When calling various NVIGI APIs it is a common case that they provide specific output(s) as typed and versioned structures. Here is one example:

```cpp
//! Obtain interface, note that interface is just a special typed and version structure
nvigi::IGeneralPurposeTransformer* igpt{};
nvigiGetInterface(nvigi::plugin::gpt::ggml::cuda::kId, &igpt, params.nvigiLoadInterface);

//! Tell our GPT interface where to find models
nvigi::GPTCreationParameters params{};
common.utf8PathToModels = myPathToModelRepo;
//! Chain it together so GPT interface can find it
if(NVIGI_FAILED(params.chain(common)))
{
    // Handle error
}

//! Now we obtain capabilities and requirements for this instance, again as typed and versioned structure but containing just data
nvigi::CommonCapabilitiesAndRequirements* caps{};
getCapsAndRequirements(igpt, params, &caps);
```
By design, **NVIGI will chain together any additional output(s)** so the requester can use the `findStruct` function to check if they are present like this:
```cpp
//! Check if optional output is chained with caps
auto sampler = findStruct<nvigi::GPTSamplerParameters>(*caps);
if (sampler)
{
    //! This interface supports sampler parameters
    //!
    //! They can be modified as needed and chained to the nvigi::GPTCreationParameters when creating an instance
    sampler->penalizeNewLine = true;
    if(NVIGI_FAILED(params.chain(sampler)))
    {
        // Handle error
    }
}
```
> NOTE: In this example, since interface in question supports `nvigi::GPTSamplerParameters`, sampler parameters can be chained together with the `nvigi::GPTCreationParameters` and serve as input (see the [above section](#input-data)) if it is required to change any sampler related options when creating the instance.
## Thread Safety

By default, any API in NVIGI is NOT thread safe **unless stated otherwise in the comment above the API declaration**. For example:

```cpp
//! ....
//!
//! This method is NOT thread safe.
nvigi::Result(*createInstance)(const nvigi::NVIGIParameter* params, nvigi::InferenceInstance** instance);
```
Note that the line immediately above the declaration explicitly states that this method is NOT thread safe. It is host's responsibility to provide synchronization is such scenarios.

## Security

### Digital Signatures

When running **on Windows and in production configuration**, NVIGI core framework will **enforce digital signature check on all NVIGI plugin libraries but NOT on their dependencies**. For example:

```sh
nvigi.plugin.$name.$backend.$api.dll  # signed by NVDA, signature checked by `nvigi.core.framework.dll`
├── cudaRt_12.dll                     # signed by NVDA, signature NOT checked by `nvigi.core.framework.dll`
├── tensorRT_10.dll                   # signed by NVDA, signature NOT checked by `nvigi.core.framework.dll`
├── libprotobuf.dll                   # not signed, up to the host app to enforce security
├── zlib.dll                          # not signed, up to the host app to enforce security
└── kernel32.dll                      # system library, ignored since it is signed by the OS and in the secure location

```

To prevent hackers from replacing `nvigi.core.framework.dll` with potentially malicious code it is expected that the host application uses API located in `nvigi_security.h` to ensure that digital signature is valid before loading any NVIGI libraries. Here is an example:

```cpp
//! IMPORTANT: Always use absolute path to DLL
auto pathToNVIGICore = std::filesystem::path(myPathToNVIGISDK) / L"nvigi.core.framework.dll";
if(nvigi::security::verifyEmbeddedSignature(pathToNVIGICore.wstring().c_str()))
{
  //! Safe to load core and initialize NVIGI

  //! IMPORTANT: Always use absolute path to DLL
  HMODULE lib = LoadLibraryW(pathToNVIGICore.wstring().c_str());
}
```

> IMPORTANT: Failure to check for digital signature on `nvigi.core.framework.dll` can result in executing potentially dangerous code with wide range of consequences.

As mentioned above, `nvigi.core.framework.dll` with NOT check any signatures on 3rd party dependencies. This is a security vulnerability and must be addressed by the host application either by enforcing CRC checks on all DLLs or some other method which ensures that libraries haven't been tampered with before loading them.

### Elevated Privileges

If the host process is running with elevated privileges **NVIGI core will attempt to downgrade some of them to mitigate security risks**. If this behavior is not desirable it can be overridden by setting `PreferenceFlags::eDisablePrivilegeDowngrade` flag. 

> IMPORTANT: Please note that **when opting out from the privilege downgrade the host application is taking over the responsibility for any security breaches that might occur** and in this scenario, it is highly recommended to install the NVIGI bits in secure location which can be modified only by admin users.

Here is the list of privileges in question:

```cpp
SE_LOAD_DRIVER_NAME
SE_DEBUG_NAME
SE_TCB_NAME
SE_ASSIGNPRIMARYTOKEN_NAME
SE_SHUTDOWN_NAME
SE_BACKUP_NAME
SE_RESTORE_NAME
SE_TAKE_OWNERSHIP_NAME
SE_IMPERSONATE_NAME
```

> NOTE: This is related only to Windows platform

## Core API

The core API is rather minimalistic and located in the `nvigi.h` header.

> **VERY IMPORTANT**: Always make sure to securely load `nvigi.core.framework.dll` before calling any NVIGI APIs (see the [security section](#security) above)

### Initialization

```cpp
//! Initializes the NVIGI framework
//!
//! Call this method when your application is initializing
//!
//! @param pref Specifies preferred behavior for the NVIGI framework (NVIGI will keep a copy)
//! @param pluginInfo Optional pointer to data structure containing information about plugins, user system
//! @param sdkVersion Current SDK version
//! @returns nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiInit(const nvigi::Preferences &pref, nvigi::PluginAndSystemInformation** pluginInfo = nullptr, uint64_t sdkVersion = nvigi::kSDKVersion);
```

This function initializes NVIGI SDK based on the provided preferences. It enumerates available plugins and, if requested, can report back detailed information on what plugins are available, which are compatible with the user's system and which are not, what are the basic requirements for each plugin etc. Here is an example:

```cpp
nvigi::Preferences pref{};
pref.logLevel = nvigi::LogLevel::eDefault; // eVerbose for more detailed log
pref.showConsole = true; // false when shipping
pref.numPathsToPlugins = count(myPathsToNVIGIPlugins);
//! NOTE: Plugins and their custom dependencies can be split up across different locations. Shared dependencies must be in one location (see below)
pref.utf8PathsToPlugins = myPathsToNVIGIPlugins;
//! NOTE: Location for shared dependencies, if not provided NVIGI will assume that there are NO shared dependencies only custom ones and they are all next to their respective plugins.
pref.utf8PathToDependencies = myPathToNVIGISharedPluginDependencies;
pref.logMessageCallback = myCallback; // OPTIONAL
pref.utf8PathToLogsAndData = myPathToLogs; // OPTIONAL but highly recommended, much easier to track down any issues

// Optional info about system and plugins
nvigi::PluginAndSystemInformation* info{};
if(NVIGI_FAILED(result, nvigiInit(pref, &info, nvigi::kSDKVersion)))
{
    // Handle error
}
```

Note that **not all plugins need to be enumerated here, later on when requesting interfaces it is possible to provide additional paths to search.**

> **IMPORTANT**:
> NVIGI SDK can come in various configurations (debug, release, production etc.) which can contain `different 3rd party dependencies`. To avoid runtime issues it is absolutely essential to use correct set of NVIGI plugins combined with matching set of 3rd party dependencies - do NOT mix debug, release etc.

### Shutdown

Once NVIGI is no longer needed the following API needs to be called to release any used resources and unload any plugins which might be resident in memory:

```cpp
//! Shuts down the NVIGI module
//!
//! Call this method when your application is shutting down. 
//!
//! @returns nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiShutdown();
```

### Interfaces

As mentioned in the introduction, each plugin is assigned a unique identifier and it must implement at least one interface which is represented as a typed and versioned structure. Host application requests an interface which is needed to perform specific tasks and releases it once it is no longer needed. Here is the API which loads an interface:

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
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiLoadInterface(nvigi::PluginID feature, const nvigi::UID& interfaceType, uint32_t interfaceVersion, void** _interface, const char* utf8PathToPlugin);
```

It is important to emphasize that this API allows host to provide an additional path to a plugin which implements this interface. In other words, **not all plugin locations need to be known when `nvigiInit` is called**.

> NOTE:
> This API will load and make resident in memory the shared library which matches the `PluginID` (unless it is already loaded due to earlier request for an interface it implements)

**It is highly recommended** to use the templated helpers `nvigiGetInterface` or `nvigiGetInterfaceDynamic` depending on the linking option used in the host application:

```cpp
//! Helper method when statically linking NVIGI framework
//! 
template<typename T>
inline nvigi::Result nvigiGetInterface(nvigi::PluginID feature, T** _interface)

//! Helper method when dynamically loading NVIGI framework
//! 
template<typename T>
inline nvigi::Result nvigiGetInterfaceDynamic(nvigi::PluginID feature, T** _interface, PFun_nvigiLoadInterface* func)
```

For example:

```cpp
nvigi::IGeneralPurposeTransformer* igpt{};

// Static linking, as an example requesting interface from ggml cuda backend
if(NVIGI_FAILED(result, nvigiGetInterface(nvigi::plugin::gpt::ggml::cuda::kId, &igpt)))
{
    // Handle error
}
 
// Dynamic `nvigi.core.framework` loading, again as an example requesting interface from ggml cuda backend
if(NVIGI_FAILED(result, nvigiGetInterfaceDynamic(nvigi::plugin::gpt::ggml::cuda::kId, &igpt, nvigiLoadInterfaceFunction)))
{
    // Handle error
}
```

When certain interface is no longer needed, it should to be unloaded so that the underlying shared library which implements it can be released. This is achieved with the following API:

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
NVIGI_API nvigi::Result nvigiUnloadInterface(nvigi::PluginID feature, void* _interface);
```

> NOTE:
> Interfaces are reference counted so the underlying shared library will be released only when ALL references to the requested interface(s) are released.

## Validation

Once successfully initialized the optional `nvigi::PluginAndSystemInformation`, if provided as shown in the above section when calling `nvigiInit`, contains useful information which can be used to determine if specific plugin and or interface is available. NVIGI comes with various helpers which can be used as shown below:

### Check If Specific Plugin Is Supported
```cpp
//! Replace $name::$backend::$api as appropriate
if(NVIGI_FAILED(result, nvigi::getPluginStatus(info, nvigi::plugin::$name::$backend::$api::kId)))
{
  // Not supported, check the following sections to find out how to detect min spec for OS, drivers etc.
}
```
If plugin in question is not supportedt the following functions can be used to obtain more details:
### Check For Minimum Required OS Version
```cpp
//! Replace $name::$backend::$api as appropriate
nvigi::Version version;
if(NVIGI_FAILED(result, nvigi::getPluginRequiredOSVersion(info, nvigi::plugin::$name::$backend::$api::kId, version)))
{
  // Not supported, check the version
}
```
### Check For Minimum Required Adapter Driver Version
```cpp
//! Replace $name::$backend::$api as appropriate
nvigi::Version version;
if(NVIGI_FAILED(result, nvigi::getPluginRequiredAdapterDriverVersion(info, nvigi::plugin::$name::$backend::$api::kId, version)))
{
  // Not supported, check the version
}
```
### Check For Minimum Required Adapter Vendor
```cpp
//! Replace $name::$backend::$api as appropriate
nvigi::VendorId vendor;
if(NVIGI_FAILED(result, nvigi::getPluginRequiredAdapterVendor(info, nvigi::plugin::$name::$backend::$api::kId, vendor)))
{
  // Not supported, check the vendor
}
```
### Check For Minimum Required Adapter Architecture
```cpp
//! Replace $name::$backend::$api as appropriate
uint32_t arch; // 0 indicates any architecture is fine
if(NVIGI_FAILED(result, nvigi::getPluginRequiredAdapterArchitecture(info, nvigi::plugin::$name::$backend::$api::kId, arch)))
{
  // Not supported, check the architecture number
}
```
If plugin in question is supported, next we can query if specific interface we are interested in is supported/implemented by the plugin:

### Check If Specific Plugin Exports An Interface
```cpp
//! Replace $name::$backend::$api and $some_interface_name as appropriate
if(NVIGI_FAILED(result, nvigi::isPluginExportingInterface(info, nvigi::plugin::$name::$backend::$api::kId, nvigi::$some_interface_name)))
{
  // Not supported, check the result
}
```

> IMPORTANT:
> At this point NO SHARED LIBRARIES (PLUGINS) ARE LOADED NOR RESIDENT IN MEMORY, only upon an explicit request for an interface implemented by a specific plugin will that plugin be loaded.

## NVIGI and D3D Wrappers (e.g. Streamline)

Care should be taken when integrating NVIGI into an existing application that is also using a D3D object wrapper like Streamline.  The queue/device parameters passed to NVIGI must be the **native** objects, not the app-level wrappers.  In the case of Streamline, this means using `slGetNativeInterface` to retrieve the base interface object before passing it to NVIGI.

## 3rd Party Dependencies

> **EXTREMELY IMPORTANT PLEASE READ**:
> If host application is using the exact same dependency as NVIGI but different version this can lead to runtime issues and random crashes. Please contact NVIDIA do discuss possible solutions.

NVIGI SDK can come in various configurations (debug, release, production) which can contain different 3rd party dependencies. 

**To avoid runtime issues please make sure that:**

* NVIGI shared dependencies (CUDA, tensorRT etc.) are stored in one location pointed by the `nvigi::Preferences::utf8PathToDependencies` parameter.
* Correct set of NVIGI plugins is combined with matching set of 3rd party dependencies (debug and release dependencies are NOT mixed up etc.)
* `nvigi::Preferences::utf8PathsToPlugins` and `nvigi::Preferences::utf8PathToDependencies` always point to the appropriate locations based on the build configuration.
* On Windows, `AddDLLDirectory` or similar APIs are NOT used to manage NVIGI paths implicitly - **this prevents NVIGI plugins from obtaining the location of their dependencies.**
* On Linux, `LD_LIBRARY_PATH` includes path(s) to NVIGI dependencies before host application starts

In addition to the above, please note the following:

* `nvigiInit` and `nvigiLoadInterface` **are NOT thread safe and they will TEMPORARILY modify DLL search path based on the information provided by the host**.
  * Host application should NOT do any DLL related operations while the above mentioned APIs are running without an explicit synchronization
* If `nvigi::Preferences::utf8PathToDependencies` is NOT provided **NVIGI will assume that there are NO shared dependencies and any plugin specific dependencies all MUST be next to their respective plugin(s)**. 
* Individual plugins might need the `nvigi::Preferences::utf8PathToDependencies` to dynamically load their own dependencies and validate digital signatures on them
* If plugin dependency is NOT located in `nvigi::Preferences::utf8PathsToPlugins` or `nvigi::Preferences::utf8PathToDependencies` that plugin will fail to load to avoid DLL ABI issues.

### Runtime Scenarios With Dependencies

* NVIGI and host application do NOT share any dependencies or share a subset of identical dependencies - same name(s), compatible ABI(s)
  * No runtime issues but **keep in mind that NVIGI expects to load its dependencies from `nvigi::Preferences::utf8PathsToPlugins` or `nvigi::Preferences::utf8PathToDependencies`**
* NVIGI and host application share a subset of dependencies with the same name(s) but incompatible ABI(s)
  * Unless each API exported by the dependencies in question is accessed explicitly using `HMODULE` and `GetProcAddress` there can be random crashes and instability
  * Preferably, the host application should rename the dependencies in question to avoid ABI collision(s)
  * Alternatively, either NVIGI plugin(s) or the host app should be recompiled to use the same version of shared dependencies

### Plugin And Dependencies Deployment Examples

#### Everything In One Location:

```cpp
// This is a very common and valid way to distribute NVIGI with your application
$some_path/nvigi
│
├──nvigi.core.framework.dll
├──nvigi.pugin.gpt.ggml.cuda.dll
├──nvigi.pugin.asr.ggml.cuda.dll
├──cudart64_12.dll
├──ggml_40f25557_cuda.dll
└──zlib1.dll
```
> NOTE: In this scenario both `nvigi::Preferences::utf8PathsToPlugins` and `nvigi::Preferences::utf8PathToDependencies` point to the same location - `$some_path/nvigi`

#### Split In Multiple Locations

Core and shared dependencies together
```cpp
// Care must be taken not to duplicate plugins or dependencies in this setup
$some_path/nvigi/core
│
├──nvigi.core.framework.dll
└──cudart64_12.dll // Shared by many plugins so must be in one location
```
> NOTE: Here `nvigi::Preferences::utf8PathToDependencies` points to `$some_path/nvigi/core` but `nvigi::Preferences::utf8PathsToPlugins` points to different locations (see below)

GPT plugin(s)
```cpp
// Care must be taken not to duplicate plugins or dependencies in this setup
$some_path/nvigi/gpt
│
├──nvigi.pugin.gpt.ggml.cuda.dll
└──tensorRT_10.dll // This is OK as long as other plugins don't use it, this is version 10
```
ASR plugin(s)
```cpp
// Care must be taken not to duplicate plugins or dependencies in this setup
$some_path/nvigi/asr
│
├──nvigi.pugin.asr.ggml.cuda.dll
└──tensorRT_11.dll // This is OK as long as other plugins don't use it, this is version 11
```
> NOTE: In the above example, if a new plugin is introduced and uses `tensorRT_10.dll` for example, this shared library now must move to `$some_path/nvigi/core` otherwise we have invalid configuration with duplicated dependency.

#### Invalid Configurations

> IMPORTANT: Note that in this scenario shared library is duplicated across different locations which is NOT allowed.

```cpp
$some_path/nvigi/core
│
├──nvigi.core.framework.dll
└──cudart64_12.dll
```
GPT plugin(s)
```cpp
$some_path/nvigi/gpt
│
├──nvigi.pugin.gpt.ggml.cuda.dll
└──cudart64_12.dll // !!! duplicates NOT allowed
```
ASR plugin(s)
```cpp
$some_path/nvigi/asr
│
├──nvigi.pugin.asr.ggml.cuda.dll
└──cudart64_12.dll // !!! duplicates NOT allowed
```

#### Collisions With The Host

In this example, host uses `AddDllDirectory` to include `$some_path/my_app_dependencies` in the OS search path or loads `libz.dll` prior to initializing NVIGI which then results in NVIGI using incorrect `libz.dll` which can lead to instability or incorrect runtime behavior.

```cpp
$some_path/nvigi/core
│
├──nvigi.core.framework.dll
├──cudart64_12.dll
└──libz.dll // !!! NVIGI version
```

```cpp
$some_path/my_app_dependencies
│
└──libz.dll // !!! HOST version
```
> NOTE: If similar issue occurs while deploying your application please contact NVIDIA