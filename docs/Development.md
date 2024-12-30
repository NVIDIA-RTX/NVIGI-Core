# Development

Before reading this document please read [the architecture document](./Architecture.md)

## Breaking ABI

In terminology below, "Breaking ABI" or "ABI-Breaking" means that a change is made that causes a compiled object with an understanding of
the object's binary layout before the change will no longer work correctly after the change.

Non-exhaustive examples of changes that break ABI (expounded further below):

* Function calling convention
* Function return type or parameters (including changing pass-by-value to pass-by-reference)
* Ordering of members within a ``struct`` or ``class``
* Type of a member (i.e. changing ``size_t`` to ``unsigned``)
* Offset or alignment (such as by inserting a member)
* Size of a member (i.e. changing ``char buffer[128]`` to ``char buffer[256]``)
* Using nested structures (i.e. struct B { A a}, any change in size for struct A breaks the expected offsets and/or alignment for struct B)

Making these types of changes is acceptable **only when working on plugin's INTERNAL data structures and APIs** which are NOT shared across plugin boundaries.

## Implementing a Plugin

> IMPORTANT:
> There should be no need to edit any source code outside of `source/plugins/nvigi.myplugin` and `source/tests/ai/main.cpp`, if changes are needed in the In-Game Inferencing core components please ask for the assistance (see below)

### Critical Reminders

* **Do NOT break C ABI compatibility**
  * Use `NVIGI_UID` and `NVIGI_VALIDATE_STRUCT` for all shared structures!
  * Never use virtual functions, volatile members or STL types (`std::vector` etc.) in shared interfaces
  * Never change or reorder existing members in any shared data or interfaces
  * Never use nested structures, always use pointer members
* **Do NOT forget to clearly mark your exported methods as thread safe of not** - this is normally done in the comment section above the function declaration.
* **Do NOT duplicate interface or plugin GUIDs** - always create new GUID when cloning an existing interface or data structure
* **Do NOT ship plugins unless they are PRODUCTION builds, DIGITALLY SIGNED and packaged on GitLab or TeamCity**
  * Shipping non production builds or any builds from someones local machine is strictly forbidden
* **Do NOT break other plugins or add unnecessary dependencies between your plugin and others**
* **Do NOT include internal headers in plugin's public header(s) - only public headers are allowed**  
  * Public headers are always in format `nvigi_xxxx.h` and can only include other public headers or standard C/C++ headers as needed.
  * All public headers are located in `_sdk\include` after NVIGI SDK is packaged
* **Always use `NVIGI_CATCH_EXCEPTION` macro when implementing custom interfaces**
  * This allows us to catch any exceptions out in the wild and produce mini-dumps for debugging
* If your plugin has custom error codes make sure to follow the guidelines provided in `nvigi_result.h`
* If a large memory allocation needs to cross plugin (NOT host) boundaries you must use `nvigi::types::string or vector`
  * This ensures C ABI compatibility and consistent memory allocation and de-allocation (see bellow for assistance if new data type needs to be implemented)
* When accessing shared data or interfaces which are version 2 or higher **always check the version before accessing any members**

It is worth noting that there are some exceptions when it comes to C ABI compatibility compliance and rules. They apply **only to the NON PRODUCTION plugins** which are used as helpers or debugging tools. Good example would be the `nvigi.plugin.imgui`, it is perfectly fine to use STL `std::function` and lambdas to provide UI rendering functions for your plugin(s) **as long as these code section are compiled out in production**.

### Thread Safety

Unless explicitly stated otherwise, **NVIGI has no built-in thread safety.** Each method exported by NVIGI SDK must be clearly marked as thread safe or unsafe depending on the implementation. Here is an example from the CORE API:

```cpp
//! Shuts down the NVIGI module
//!
//! Call this method when your application is shutting down. 
//!
//! @returns IResult interface with code nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
```

Note the following statement just before the API declaration:

```cpp
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiShutdown();
```

> IMPORTANT: When implementing interface(s) for your plugin always make sure to state thread safety clearly.

### Plugin Naming

All plugins must follow the following naming nomenclature `nvigi.plugin.$name.{$backend.$api}` where `$backend` and `$api` are optional unless there are multiple backends and/or APIs used by the same backend. Here are some examples:

```text
nvigi.plugin.hwi.d3d12
nvigi.plugin.hwi.cuda
nvigi.plugin.gpt.ggml.cpu
nvigi.plugin.gpt.ggml.cuda
nvigi.plugin.a2f.trt.cuda
nvigi.plugin.reshade
```

> NOTE:
> NVIGI core components are named `nvigi.core.$name` like for example `nvigi.core.framework`

### GUID Creation for Interfaces and Plugins

For creating IDs for plugins the easiest way is to use built in tool (packaged in any SDK `[sdk]`, including a pre-packaged one as `[root]` or a locally-packaged one as `[root]/_sdk`) as follows:

```sh
[sdk]/bin/x64/nvigi.tool.utils.exe --plugin nvigi.plugin.$name.$backend.$api
```
which then outputs:
```text
namespace nvigi::plugin::$name::$backend::$api
{
constexpr PluginID kId = {{0x9be1b413, 0xda31, 0x445b,{0xbe, 0x1b, 0x20, 0x30, 0x7f, 0x4f, 0x0c, 0x00}}, 0x4dc06d}; 
//{9BE1B413-DA31-445B-BE1B-20307F4F0C00} [nvigi.plugin.$name.$backend.$api]
}
```

When it comes to the interfaces the approach is very similar:

```sh
[sdk]/bin/x64/nvigi.tool.utils.exe --interface MyInterface
```
which generates unique GUID and new interface that looks like this:
```text
//! Interface MyInterface
//!
//! {41021ED0-1BB2-4255-9545-AD5C902017E0}
struct alignas(8) MyInterface
{
    MyInterface(){}
    NVIGI_UID(UID({0x41021ed0, 0x1bb2, 0x4255,{0x95, 0x45, 0xad, 0x5c, 0x90, 0x20, 0x17, 0xe0}}), kStructVersion1);
    
    //! v1 members go here, please do NOT break the C ABI compatibility:

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvigi::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};
```

> NOTE:
> The above generated code is automatically copied to the clipboard for easy pasting into your public header `nvigi_$name.h`

### Files and Folders

Most plugins have the following file structure:

```text
source/plugins/nvigi.$name/
├── nvigi_$name.h   <- public header, must NOT include any internal headers
├── $backendA
│   ├── internal headers, source, version, resources
│   └── $3rdparty  <- optional, only if not on packman and cannot be under 'external'
│       └── 3rd party includes, source, libs
├── $backendB
│   ├── internal headers, source, version, resources
│   └── $3rdparty  <- optional, only if not on packman and cannot be under 'external'
│       └── 3rd party includes, source, libs
```

> IMPORTANT:
> If having a backend makes no sense for your plugin there is no need to have the subfolder for it.

### New Plugin Setup

#### Inference Plugins

Here are the basic steps for setting up a new plugin performing **AI inference** tasks:

* Clone `nvigi.template.inference` folder and rename it to your plugin's name
* Rename public header `nvigi_template.h` to match your name
* Search for "tmpl", "template", "Template" and "TEMPLATE" and replace with your name
* Run `[sdk]/bin/x64/nvigi.tool.utils.exe --plugin nvigi.plugin.$name.$backend{.$api}` to obtain UID and crc24 and paste that in the public header `nvigi_$name.h` under `namespace nvigi::$mynamspace`
* Rename folder `backend` to match target backend you are using (`ggml`, `cuda`, `trt`, `vk` etc.)
  * Add more backends as needed if your plugin supports more than one (see gpt plugin for details)
  * Ideally, all backends should implement the same interface declared in `nvigi_myplugin.h`

In addition, the public git source is also available for many of the shipping plugins as a part of the public [GitHub Plugins source repo](https://github.com/NVIDIA-RTX/NVIGI-Plugins).

> IMPORTANT:
> All inference plugins are expected to implement the same interface and follow the same execution flow as defined in `source/utils/nvigi.ai`

#### Generic Plugins

Here are the basic steps for setting up a new plugin performing **completely generic and custom tasks**:

* Clone `nvigi.template.generic` folder and rename it to your plugin's name
* Rename public header `nvigi_template.h` to match your name
* Search for "tmpl", "template", "Template" and "TEMPLATE" and replace with your name
* Run `[sdk]/bin/x64/nvigi.tool.utils.exe --plugin nvigi.plugin.$name.$backend{.$api}` to obtain UID and crc24 and paste that in the public header `nvigi_$name.h` under `namespace nvigi::$mynamspace`

Some examples, `nvigi.plugin.hwi.cuda`, `nvigi.plugin.imgui.d3d12` etc.

> NOTE:
> Generic plugins can do rendering, networking or any other custom tasks. If backends make no sense there is no need to use the subfolder.

#### Common Steps

* Modify `package.py` to add your project to the build system (again clone from template or any other plugin as needed). Here is a snapshot from the file showing various components:
```py
components = {
        'core': {
            'platforms': all_plat,
            'externals': [coresdk_ext, cuda_ext, nlohmann_json_ext]
        },
        'ai.pipeline' : {
            'platforms': win_plat,
            'sharedlib': ['nvigi.plugin.ai.pipeline'],
            'includes': ['source/plugins/nvigi.aip/nvigi_aip.h'],
            'sources': ['plugins/nvigi.aip', 'shared'],
            'premake': 'source/plugins/nvigi.aip/premake.lua' 
        },
        'asr.ggml.cpu' : {
            'platforms': win_plat,
            'sharedlib': ['nvigi.plugin.asr.ggml.cpu'],
            'docs': ['ProgrammingGuideASRWhisper.md'],
            'includes': ['source/plugins/nvigi.asr/nvigi_asr_whisper.h'],
            'sources': ['plugins/nvigi.asr/nvigi_asr_whisper.h', 'plugins/nvigi.asr/ggml', 'shared'],
            'externals': [nlohmann_json_ext, whispercpp_ext],
            'premake': 'source/plugins/nvigi.asr/ggml/premake.lua',
            'models': ['nvigi.plugin.asr.ggml'],
            'data': ['nvigi.asr']
        },
        ....
    
```
* If your plugin needs 3rd party lib(s) make sure to include them on packman (see [nvigi packman storage](https://lspackages.nvidia.com/packages/my?remote=Habilis-ext) for package availability and if needed to upload your own packages)
  * All 3rd party libraries go under the `external` folder
  * Make sure to list them as `'externals': ...` under your component in `package.py`, see `nlohmann_json_ext` for example
  * Please make sure to include both Windows and Linux 3rd party packages
  * Update 3rd party licensing document in the root folder (SWIPAT is required to use 3rd party software)
  * If `packman` does not have your package and the package itself is not large, it is fine to simply check it in, for example see `source/plugins/nvigi.gpt/ggml/external/`  
* Update `premake.lua` in your plugin's directory to include everythign your project needs and make sure path to it is listed correctly in your `component` section, an example:
```py
'premake': 'source/plugins/nvigi.asr/ggml/premake.lua',
```
* Run `setup.bat vs20xx` for Windows (`setup.sh` for Linux)

> NOTE:
> If `setup.bat` fails with an error from the `packman` package downloader, please re-run `setup.bat` again as there are rare but possible issues with link-creation on initial run.

> NOTE:
> If it makes things easier it is totally fine to clone and modify **any existing plugin** (gpt, asr etc.) to speed up development.

### Building and Code Modifications

* Modify source code (add new headers or cpp files as needed) to perform actions you need for your plugin
  * If adding new files make sure to update `premake` and re-run `setup.xx`
* Make sure NOT to include internal headers in your public `nvigi_$name.h` header
* When adding new data structures/interfaces which are shared either publicly or internally you must use the `nvigi.tool.utils` as described in [GUID creation](#guid-creation-for-interfaces-and-plugins)  
* When modifying an existing shared data or interfaces **always follow the "do not break C ABI compatibility" guidelines**
* Wrap all you externally exposed functions with `NVIGI_CATCH_EXCEPTION`
* Build your plugin using `_project/vs20xx/nvigi.sln` (on Windows) or run `build.sh --build {debug, release, production}` (on Linux)

> IMPORTANT:
> Linux is a first class citizen so all plugins **that can run on Linux** are expected to do so. Some exceptions would be plugins with only Windows specific backends like for example WinML, DirectML, DirectX etc.

### Packaging

* Modify `package.py` to include your plugin(s) in the final SDK
* Make sure to include ONLY the public header(s), DLL and symbols for each plugin
* Modify `scripts/nvigi.cmake` to include your plugin(s) into the `CMakeLists.txt` that is packaged with the SDK.  This is used by the Sample (and other apps) for easy integration of NVIGI into CMake-based build systems.
* Run `package.{bat,sh} -{debug, release, develop, production}` to package SDK locally under `_sdk` folder

### Unit Tests

* In-Game Inferencing is using [`Catch2` testing framework](https://github.com/catchorg/Catch2)
* Modify `source/nvigi.$name/$backend/tests.h` to include your unit test(s)
* Use appropriate tags so your test(s) can be filtered correctly, here is some pseudo code:

```cpp
//! Make sure to apply appropriate tags for your tests ([gpu], [cpu], [cuda], [d3d12] etc. see other tests for examples)
//! 
//! This allows easy unit test filtering as described here https://github.com/catchorg/Catch2/blob/devel/docs/command-line.md#specifying-which-tests-to-run
//! 
TEST_CASE("template_backend", "[tag1],[tag2]")
{
    //! Use global params as needed (see source/tests/ai/main.cpp for details and add modify if required)

    nvigi::ITemplate* itemplate{};
    nvigiGetInterfaceDynamic(plugin::tmpl::backend::api::kId, &itemplate, params.nvigiLoadInterface);
    REQUIRE(itemplate != nullptr);

    // Get instance(s) from your interface, run tests, check results etc.

    auto res = params.nvigiUnloadInterface(plugin::tmpl::backend::api::kId, itemplate);
    REQUIRE(res == nvigi::kResultOk);
}
```

* Ensure that `REQUIRE` macro is used to validate all results and expected outputs
* Include your `tests.h` in `source/tests/ai/main.cpp` and make sure that unit test is compiling

```cpp
//! TODO: ALL NEW TESTS GO HERE
//! 
#include "source/plugins/nvigi.gpt/ggml/tests.h"
#include "source/plugins/nvigi.asr/ggml/tests.h"
```

* Run `package.{bat,sh} -{debug, release, develop, production}` to prepare final SDK
* Execute `run.{bat,sh} --tags "[mytag1],[mytag2],..." --models $path_to_nvigi_models --audio $path_to_wav_file` to trigger your test and make sure it is passing

> **IMPORTANT**:
> Unit test always runs in the `_sdk/bin/x64` folder so make sure to package your plugin first before running unit tests.

### UI Rendering with ImGui

For testing and experimental purposes In-Game Inferencing is using modified version of [`ReShade`](https://github.com/crosire/reshade) to load NVIGI plugins and run them in any D3D12 application (support for Vulkan or D3D11 can be added as needed). For more details please see [reshade plugin](https://gitlab-master.nvidia.com/habilis/plugins/reshade)

### Using Interfaces

* All interfaces in NVIGI are typed and versioned structures
* If multiple versions of an interface are available, **it is mandatory to check the version before accessing members v2 or higher+**.

> IMPORTANT: **Always keep in mind that plugins can be updated individually so at any point in time, plugin code can be handed an interface or data structure from another plugin that is older or newer than the one used at the compile time.**

```cpp
//! Obtain interfaces from other plugins or core (if needed)
//! 
//! For example, here we use our internal helper function to obtain networking interface. 
//!
//! NOTE: This will trigger loading of the 'nvigi.plugin.net' if it wasn't loaded already
//! 
net::INet* net{};
if (!framework::getInterface(framework, nvigi::plugin::net::kId, &net))
{
    NVIGI_LOG_ERROR("Failed to obtain interface");
}
else
{
    if(isOlderStruct(net))
    {
      //! Our plugin is coming from a newer SDK, INet interface is older so we can NOT access latest members or API(s) in net::INet

      if(net->getVersion() == 1)
      {
        // ONLY access members from v1
      }
      else if(net->getVersion() == 2)
      {
         // ONLY access members from v2
      }
      // and so on
    }
    else // same version or newer
    {
      // our plugin is coming from an older or identical SDK as plugin::net::kId, we can access all members and API(s) in net::INet known to us at the compile time
    }
}
```

> IMPORTANT: Always check versions or interfaces or data structures received from other plugins, they could be older or newer since plugins can be updated independently

### Exception Handling

When implementing custom API in your exported interface always make sure to wrap your functions using the `NVIGI_CATCH_EXCEPTION` macro. Here is an example on an interface implementation:

```cpp
//! Example interface implementation
//! 
nvigi::Result tmplCreateInstance(const nvigi::NVIGIParameter* params, nvigi::InferenceInstance** instance)
{
    //! An example showing how to obtain an optional chained parameters
    //! 
    //! User can chain extra structure(s) using the params._next
    //! 
    auto extraParams = findStruct<nvigi::TemplateCreationParametersEx>(*params);
    if (extraParams)
    {
        //! User provided extra parameters!
    }

    return kResultOk;
}

nvigi::Result tmplDestroyInstance(const nvigi::InferenceInstance* instance)
{
    return kResultOk;
}

nvigi::Result tmplGetCapsAndRequirements(nvigi::TemplateCapabilitiesAndRequirements* modelInfo, const nvigi::TemplateCreationParameters* params)
{
    return kResultOk;
}
```

Here is how one would wrap this implementation to make sure we catch any exceptions:

```cpp
//! Making sure our implementation is covered with our exception handler
//! 
namespace tmpl
{
nvigi::Result createInstance(const nvigi::NVIGIParameter* params, nvigi::InferenceInstance** instance)
{
    NVIGI_CATCH_EXCEPTION(tmplCreateInstance(params, instance));
}

nvigi::Result destroyInstance(const nvigi::InferenceInstance* instance)
{
    NVIGI_CATCH_EXCEPTION(tmplDestroyInstance(instance));
}

nvigi::Result getCapsAndRequirements(nvigi::TemplateCapabilitiesAndRequirements* modelInfo, const nvigi::TemplateCreationParameters* params)
{
    NVIGI_CATCH_EXCEPTION(tmplGetCapsAndRequirements(modelInfo, params));
}
} // namespace tmpl
```

Finally, here is how our interface is shared with the host application or other plugins:

```cpp
//! Main entry point - starting our plugin
//! 
Result nvigiPluginRegister(framework::IFramework* framework)
{
    if (!plugin::internalPluginSetup(framework)) return ResultInvalidState;

    auto& ctx = (*tmpl::getContext());

    //! Add your interface(s) to the framework
    //!
    //! NOTE: We are exporting functions with the exception handler enabled
    ctx.api.createInstance = tmpl::createInstance;
    ctx.api.destroyInstance = tmpl::destroyInstance;
    ctx.api.getCapsAndRequirements = tmpl::getCapsAndRequirements;

    framework->addInterface(plugin::tmpl::kIdBackendApi, &ctx.api);

    ...
}
```

> NOTE:
> Logs and mini-dumps from any exception caught by NVIGI can be found in `%PROGRAM_DATA%\NVIDIA\NVIGI\{APP_NAME}\{UNIQUE_ID}`

### Input Parameters and Chained Data Structures

NVIGI data structures can be chained together for convenience. For example, host application can provide mandatory creation parameters to the `createInstance` API in an interface but it can also add an optional input(s) as needed. Here is an example:

```cpp
nvigi::Result llama2CreateInstance(const nvigi::NVIGIParameters* params, nvigi::InferenceInstance** _instance)
{
    //! Look for an optional input parameter(s) chained using params._next 
    auto cugfxCreate = findStruct<GPTcugfxCreationParameters>(*params);
    if (cugfxCreate)
    {
        //! User provided optional cuGfx creation parameters!
    }
    ...
}
```

> IMPORTANT:
> If you add new NVIGI data structures for your plugin make sure to use utils library as shown [here](#guid-creation-for-interfaces-and-plugins)

### Hybrid AI plugins

This section provides more information about developing hybrid AI (local and cloud inference) plugins

#### Inference API

**All hybrid AI plugins should implement similar API** based on what is described in the [hybrid AI guide](./HybridAI.md) and implemented in [nvigi_ai.h](../source/utils/nvigi.ai/nvigi_ai.h)

#### AI Model File Structure

For consistency, all NVIGI AI inference plugins are expected to store their models using the following directory structure:

```text
nvigi.models/
├── nvigi.plugin.$name.$backend
    └── {MODEL_GUID}
        ├── files
```

Here is an example structure for the existing NVIGI plugins and models:

```text
nvigi.models/
├── nvigi.plugin.gpt.cuda.cugfx
│   └── {D6249385-2381-4400-B5A9-2088424660D2}
│       ├── gpt-13b-chat-awq-q4.bin
│       └── tokenizer.tokbin
│       └── nvigi.model.config.json
├── nvigi.plugin.gpt.ggml
│   └── {175C5C5D-E978-41AF-8F11-880D0517C524}
│       └── gpt-7b-chat-q4.gguf
│       └── nvigi.model.config.json
├── nvigi.plugin.gpt.trtllm
│   └── {e171c127-ccb3-40be-9b61-942472008024}
│       ├── build.log
│       ├── config.json
│       ├── llama_float16_tp1_rank0.engine
│       ├── model.cache
│       └── tokenizer.bin
│       └── nvigi.model.config.json
├── nvigi.plugin.sbert.ggml
│   └── {3A823EAA-DB44-4A8C-9979-06F0917B6D94}
│       └── ggml-model-q4_0.bin
│       └── nvigi.model.config.json
└── nvigi.plugin.asr.ggml
    └── {5CAD3A03-1272-4D43-9F3D-655417526170}
        └── ggml-asr-small.gguf
│       └── nvigi.model.config.json
```

Each model must provide `nvigi.model.config.json` file containing model's name, vram consumption and other model specific information. Here is an example from `nvigi.plugin.a2f.trt`:

```json
{
  "name" : "Audio To Face - A2F (Mark)",
  "vram": 834,
  "numPoses": 52,
  "boneNames": [
    "EyeBlinkLeft",
    "EyeLookDownLeft",
    "EyeLookInLeft",
    "EyeLookOutLeft",
    "EyeLookUpLeft",
    "EyeSquintLeft",
    "EyeWideLeft",
    "EyeBlinkRight",
    "EyeLookDownRight",
    "EyeLookInRight",
    "EyeLookOutRight",
    "EyeLookUpRight",
    "EyeSquintRight",
    "EyeWideRight",
    "JawForward",
    "JawLeft",
    "JawRight",
    "JawOpen",
    "MouthClose",
    "MouthFunnel",
    "MouthPucker",
    "MouthLeft",
    "MouthRight",
    "MouthSmileLeft",
    "MouthSmileRight",
    "MouthFrownLeft",
    "MouthFrownRight",
    "MouthDimpleLeft",
    "MouthDimpleRight",
    "MouthStretchLeft",
    "MouthStretchRight",
    "MouthRollLower",
    "MouthRollUpper",
    "MouthShrugLower",
    "MouthShrugUpper",
    "MouthPressLeft",
    "MouthPressRight",
    "MouthLowerDownLeft",
    "MouthLowerDownRight",
    "MouthUpperUpLeft",
    "MouthUpperUpRight",
    "BrowDownLeft",
    "BrowDownRight",
    "BrowInnerUp",
    "BrowOuterUpLeft",
    "BrowOuterUpRight",
    "CheekPuff",
    "CheekSquintLeft",
    "CheekSquintRight",
    "NoseSneerLeft",
    "NoseSneerRight",
    "TongueOut"
  ]
}
```

An optional `configs` subfolder, with the identical folder structure, can be added under `nvigi.models` to provide `nvigi.model.config.json` overrides as shown below:

```text
nvigi.models/
├── configs
    ├── nvigi.plugin.$name.$backend
    └── {MODEL_GUID}
        ├── nvigi.model.config.json
        | etc
├── nvigi.plugin.$name.$backend
    └── {MODEL_GUID}
        ├── files        
```
> NOTE:
> This allows quick way of modifying only model config settings in JSON without having to reupload the entire model which can be GBs in size

Various helper methods are available in `source/utils/nvigi.ai/ai.h` allowing easy model enumeration and filtering with custom extension support. Example usage can be found in any existing AI plugin (gpt, asr etc.)

## Assistance

For any assistance please contact [Lars Bishop](mailto:lbishop@nvidia.com) or [Bojan Skaljak](mailto:bskaljak@nvidia.com)
