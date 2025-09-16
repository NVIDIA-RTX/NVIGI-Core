# Plugin Development

This is an advanced developer document, designed for those who wish to creat their own NVIGI plugins.  Before reading this document please read [the architecture document](./Architecture.md).  While writing a plugin only requires a copy of the NVIGI Core PDK (Plugin Development Kit), most of the plugins of interest are a part of the overall main SDK.  As a result, this document will make frequent reference to plugins from the main SDK when discussing concrete examples.

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

Making these types of changes is acceptable **only when working on the plugin's INTERNAL data structures and APIs** which are NOT shared across plugin boundaries or across the plugin/app boundary.

## Implementing a Plugin

> IMPORTANT:
> There should be no need to edit any core source code outside of `source/plugins/nvigi.myplugin` and `source/tests/ai/main.cpp`, if changes are needed in the In-Game Inferencing core components please ask for assistance from your NVIDIA Developer Relations Manager (see below).  Modifying core APIs or behaviors in `nvigi.core.framework` in any way can (and likely will) make the modified core malfunction or even fail to load standard NVIGI plugins.

### Kinds of Plugins

Technically, NVIGI plugins are simply dynamic link libraries that export a set of interfaces, and may have a set of plugin-specific structs that are used as inputs and outputs.  But in most IGI use cases, plugins fall into one of two categories:

1. **Utility Plugins:** These plugins do not tend to follow any specific interface standards, other than the basic functions required of all plugins for loading, unloading, and interface discovery.  There are several such plugins in the NVIGICore itself, specifically the `nvigi.plugin.hwi.*` plugins, which provide interfaces for connecting NVIGI and it plugins to the various hardware and rendering APIs.  The standard NVIGI SDK includes a utility plugin for network behavior.  Note that not all utility plugins are designed to be used by the application.  Some utility plugins, like the main SDK's networking plguin, does not export any app-facing interfaces; it is designed to have its interfaces loaded and used only by other plugins.
1. **Inference Plugins:** These plugins are designed specifically for some form of AI inference, and follow a more specific standard set of interfaces.  Creating inference plugins that follow these standards (discussed below) will allow you plugin to be more easily integrated into applications and into "parent" plugins, such as the main SDK's AI Pipeline plugin.

### Critical Reminders

* **Do NOT break C ABI compatibility**
  * Use `NVIGI_UID` and `NVIGI_VALIDATE_STRUCT` for all shared structures!
  * Never use virtual functions, volatile members or STL types (`std::vector` etc.) in shared interfaces
  * Never change or reorder existing members in any shared data or interfaces
  * Never use nested structures, always use pointer members
* **Do NOT forget to clearly mark your exported methods as thread safe or not** - this is normally done in the comment section above the function declaration.
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
  * This ensures C ABI compatibility and consistent memory allocation and de-allocation (see below for assistance if new data type needs to be implemented)
* When accessing shared data or interfaces which are version 2 or higher **always check the version before accessing any members**

It is worth noting that there are some exceptions when it comes to C ABI compatibility compliance and rules. They apply **only to the NON SHIPPING / NON-PRODUCTION plugins** which are designed to be used internal to NVIDIA or internal to the app developer only as helpers or debugging tools. It is perfectly fine to use STL `std::function` and lambdas to provide UI rendering functions for internal, non-end-user plugin(s) **as long as these code sections are compiled out in production builds**.

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
nvigi.plugin.tts.asqflow-trt
nvigi.plugin.net
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
* Run `[sdk]/bin/x64/nvigi.tool.utils.exe --plugin nvigi.plugin.$name.$backend{.$api}` to obtain UID and crc24 and paste that in the public header `nvigi_$name.h` under `namespace nvigi::$mynamespace`
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
* Run `[sdk]/bin/x64/nvigi.tool.utils.exe --plugin nvigi.plugin.$name.$backend{.$api}` to obtain UID and crc24 and paste that in the public header `nvigi_$name.h` under `namespace nvigi::$mynamespace`

Some examples, `nvigi.plugin.hwi.cuda`, `nvigi.plugin.gpt.ggml.d3d12` etc.

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
* Update `premake.lua` in your plugin's directory to include everything your project needs and make sure path to it is listed correctly in your `component` section, an example:
```py
'premake': 'source/plugins/nvigi.asr/ggml/premake.lua',
```
* Run `setup.bat vs20xx` 

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
* Build your plugin using `_project/vs20xx/nvigi.sln`

### Packaging

* Modify `package.py` to include your plugin(s) in the final SDK
* Make sure to include ONLY the public header(s), DLL and symbols for each plugin
* Run `package.bat -{debug, release, develop, production}` to package SDK locally under `_sdk` folder

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

When implementing custom APIs in your exported interface always make sure to wrap your functions using the `NVIGI_CATCH_EXCEPTION` macro. Here is an example on an interface implementation:

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

**All hybrid AI plugins should implement similar APIs** based on what is described in the [hybrid AI guide](./HybridAI.md) and implemented in [nvigi_ai.h](../source/utils/nvigi.ai/nvigi_ai.h)

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
├── nvigi.plugin.gpt.cloud
│   └── {{01F43B70-CE23-42CA-9606-74E80C5ED0B6}}
│       └── nvigi.model.config.json
├── nvigi.plugin.gpt.ggml
│   └── {175C5C5D-E978-41AF-8F11-880D0517C524}
│       └── gpt-7b-chat-q4.gguf
│       └── nvigi.model.config.json
└── nvigi.plugin.asr.ggml
    └── {5CAD3A03-1272-4D43-9F3D-655417526170}
        └── ggml-asr-small.gguf
│       └── nvigi.model.config.json
```

Each model must provide an `nvigi.model.config.json` file containing the model's name, vram consumption and other model specific information. Here is an example from `nvigi.plugin.gpt.ggml`:

```json
{
  "name": "Qwen3-8B",
  "vram": 5280,
  "n_layers": 37,  
  "prompt_template_think": [
    "<|im_start|>system\n",
    "$system",
    "<|im_end|>",
    "<|im_start|>user\n",
    "$user",
    "<|im_end|>",
    "<|im_start|>assistant\n"
  ],
  "turn_template_think": [
    "<|im_start|>user\n",
    "$user",
    "<|im_end|>",
    "<|im_start|>assistant\n"
  ],
  "prompt_template": [
    "<|im_start|>system\n",
    "$system",
    "<|im_end|>",
    "<|im_start|>user\n",
    "$user",
    " /no_think <|im_end|>",
    "<|im_start|>assistant\n"
  ],
  "turn_template": [
    "<|im_start|>user\n",
    "$user",
    " /no_think <|im_end|>",
    "<|im_start|>assistant\n"
  ],
  "model":
  {
      "ext" : "gguf",
      "notes": "Must use .gguf extension and format, model(s) can be obtained for free on huggingface",
      "file":
      {
      "command": "curl -L -o Qwen3-8B-Q4_K_M.gguf 'https://huggingface.co/Qwen/Qwen3-8B-GGUF/resolve/main/Qwen3-8B-Q4_K_M.gguf?download=true'"
      }
  }
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

## Creating a Customized Plugin from An Existing Plugin

This document guides you through the process of creating a customized NVIGI GPT plugin, based on the original GPT plugin from the SDK. 

**> IMPORTANT: Although the document uses the GPT plugin as an example, the high-level steps also apply to other plugins.**

### Steps

This section presents one recommended approach to creating a new GPT plugin. Once you understand the project setup process, you can freely make customized changes without strictly following all the steps.

In the following instructions, we follow the directory setup described [here](https://github.com/NVIDIA-RTX/NVIGI-Plugins?tab=readme-ov-file#directory-setup).

- `<SDK_PLUGINS>` should be replaced with the full path to your NVIGI SDK directory (the path of this README).

#### Add a New Plugin Project

* Choose a name for your new plugin, such as `mygpt`.

* Prepare the source files for the new plugin by copying from an existing project.

  * Duplicate the folder containing the source code of the original plugin (`<SDK_PLUGINS>/source/plugins/nvigi.gpt`) and rename it to `<SDK_PLUGINS>/source/plugins/nvigi.mygpt`.

  * (Optional) Enter the new source folder and remove unused backend code (e.g. `rest`).

  * (Optional) Rename the source files with your new plugin name by replacing `gpt` with `mygpt`.

  * For example, assuming that we want to modify the GGML backend, the source tree should now have the following structure:

    ```
    <SDK_PLUGINS>/source/plugins
    |-- nvigi.mygpt
    |   |-- ggml
    |   |   |-- premake.lua
    |   |   |-- mygpt.h
    |   |   |-- mygpt.cpp
    |   |   |-- ... // Other source files
    |   |-- nvigi_mygpt.h
    |   |-- // "rest" is removed
    |-- ... // Other plugins
    ```

* Update the setup scripts to include the new source files.

  * Update `NVIGI-Plugins/source/plugins/nvigi.mygpt/ggml/premake.lua` by renaming it with the new plugin name (e.g., rename `group "plugins/gpt"` to `group "plugins/mygpt"` and `project "nvigi.plugin.gpt.ggml.$backend"` to `project "nvigi.plugin.mygpt.ggml.$backend"`).
  * Update `NVIGI-Plugins/premake.lua` to include the project premake file by adding `include("source/plugins/nvigi.mygpt/ggml/premake.lua")` at the end.

* Run `setup.bat` to reflect the changes.

* Open `NVIGI-Plugins/_project/vs2022/nvigi.sln` and check the Solution Explorer - a new project `nvigi.plugin.mygpt.ggml.{$backend}` should be added under `plugins.mygpt`.

#### Update Names and GUIDs in Source Files

* Find the NVIGI utility `nvigi.tool.utils.exe` located in `NVIGI-Core\bin\Release_x64`
* Open terminal and run `nvigi.tool.utils.exe --plugin nvigi.plugin.mygpt.ggml.$backend` (make sure to replace `mygpt` and `$backend` accordingly)
* Open `nvigi_mygpt.h`. This is a public header and will be provided to apps.

  * Remove the namespaces of unused backends (e.g., `namespace cloud::rest`).

  * Update the plugin GUIDs in this file by pasting the code provided by the NVIGI utilities tool:

    ```c++
    // Before
    // constexpr PluginID kId  = { {0x54bbefba, 0x535f, 0x4d77,{0x9c, 0x3f, 0x46, 0x38, 0x39, 0x2d, 0x23, 0xac}}, 0x4b9ee9 };  // {54BBEFBA-535F-4D77-9C3F-4638392D23AC} [nvigi.plugin.gpt.ggml.$backend]
    // After
    constexpr PluginID kId = { 0x576e1145, 0xf790, 0x46b4, { 0xbf, 0x9a, 0xde, 0x88, 0x9, 0x46, 0x3a, 0x15 } };  // {576E1145-F790-46B4-BF9A-DE8809463A15} [nvigi.plugin.mygpt.ggml.$backend]
    ```

* Open `mygpt.cpp`.

  * Include the new headers.

    ```c++
    // Before
    #include "source/plugins/nvigi.gpt/nvigi_gpt.h"
    #include "source/plugins/nvigi.gpt/ggml/gpt.h"
    // After
    #include "source/plugins/nvigi.mygpt/nvigi_mygpt.h"
    #include "source/plugins/nvigi.mygpt/ggml/mygpt.h"
    ```

  * Update function `getFeatureId()` to return the new plugin ID `return plugin::mygpt::ggml::$backend::kId;`.

* (Optional) Rename namespaces and variables in `mygpt.h/cpp`. These source files will not be visible to apps.

#### (Optional) Update Your App to Use the New Plugin

If you already have an app using the original GPT plugin "nvigi.plugin.gpt.ggml.cuda", follow these steps to replace it with the new plugin.

* Replace the public header `#include "nvigi_gpt.h"` with `#include "nvigi_mygpt.h"`.
* Replace the plugin ID `nvigi::plugin::gpt::ggml::$backend::kId` with `nvigi::plugin::mygpt::ggml::$backend::kId` when getting the interface.
* Rename the model folder from `nvigi.models\nvigi.plugin.gpt.ggml` to `nvigi.models\nvigi.plugin.mygpt.ggml` so the new plugin can find and load models.
* (Troubleshooting) If the new plugin DLL fails to load, start with checking if you are using the correct set of dependent DLLs, like ggml and llama.cpp and CUDA runtime.

#### Customize Your Plugin!

You have completed the setup and can now start modifying the plugin if you have plans in mind. If not, the example below shows how to add a new parameter `grammar` to `GPTSamplerParameters`.

The example demonstrates the steps to add the `grammar` parameter. Note that this parameter may be added in future versions. Similar steps can be applied to other parameters like `GPTRuntimeParameters`. 

* Open `nvigi_mygpt.h` and navigate to `GPTSamplerParameters`, then add the parameter at the end and update the version.

  ```c++
  // Before
  struct alignas(8) GPTSamplerParameters
  {
      NVIGI_UID(UID({ 0xfd183aa9, 0x6e50, 0x4021,{0x9b, 0x0e, 0xa7, 0xae, 0xab, 0x6e, 0xef, 0x49} }), kStructVersion1)
      // ... v1 parameters
      //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
  };
  // After
  struct alignas(8) GPTSamplerParameters
  {
      NVIGI_UID(UID({ 0xfd183aa9, 0x6e50, 0x4021,{0x9b, 0x0e, 0xa7, 0xae, 0xab, 0x6e, 0xef, 0x49} }), kStructVersion2)
      // ... v1 parameters
      //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
      const char* grammar{};
  };
  ```

* Open `mygpt.cpp` and navigate to function `ggmlEvaluate()`.

  ```c++
  // Before
  instance->params.sparams.ignore_eos = sampler->ignoreEOS;
  // After
  instance->params.sparams.ignore_eos = sampler->ignoreEOS;
  if (sampler->getVersion() >= 2)
  {
      instance->params.sparams.grammar = sampler->grammar ? sampler->grammar : "";
  }
  ```

* Update your app to set the new parameter.

#### Adding New Structures Or Interfaces

If you need to add completely new structure or new interface (structure with functions defining an API) please follow these steps:

* Find the NVIGI utility `nvigi.tool.utils.exe` located in `NVIGI-Core\bin\Release_x64`
* Open terminal and run `nvigi.tool.utils.exe --interface MyData` (make sure to replace `MyData` accordingly)
* Paste the provided code into your `nvigi_mygpt.h` header like for example:

> NOTE: Same command is used for data and interfaces since they are simply typed and versioned structures in NVIGI terminology

```cpp

//! Interface 'MyData'
//!
//! {45DF99CE-F5B8-4D66-90EE-FADEEFBBF713}
struct alignas(8) MyData
{
    MyData() { };
    NVIGI_UID(UID({0x45df99ce, 0xf5b8, 0x4d66,{0x90, 0xee, 0xfa, 0xde, 0xef, 0xbb, 0xf7, 0x13}}), kStructVersion1)

    //! v1 members go here, please do NOT break the C ABI compatibility:

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvigi::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(MyData)

```
* Add new data members to your structure
  
#### Using New Structures Or Interfaces

##### Data Structures

If your new structure contains only data simply chain it to the creation or runtime properties and then use `findStruct` in either `createInstance` or `evaluate` calls.

`host app`
```cpp

// Default GPT creation parameters
GPTCreationParameters creationParams{};
// Fill in the creation params 
MyData myData{};
// Fill in your data
myData.someData = 1;
// Chain it to the creation parameters
if(NVIGI_FAILED(error,creationParams.chain(myData)))
{
  // handle error
}
```

`mygpt.cpp`
```cpp
nvigi::Result ggmlCreateInstance(const nvigi::NVIGIParameter* _params, nvigi::InferenceInstance** _instance)
{
    auto myData = findStruct<MyData>(_params);
    if(myData)
    {
      // Do something with v1 data

      // If you modify your structure after having shipped your app, make sure to bump the version and check
      if(myData->getVersion() >= kStructVersion2)
      {
        // Do something with v2 data
      }
      // And so on ...
    }
    ...
}
```

##### Interface Structures

If your new structure contains and API then this new interface must be exported by your modified plugin in order to be used on the host side. Here is an example:

`nvigi_mygpt.h`
```cpp
//! Interface 'IMyInterface'
//!
//! {45DF99CE-F5B8-4D66-90EE-FADEEFBBF713}
struct alignas(8) IMyInterface
{
    IMyInterface() { };
    NVIGI_UID(UID({0x45df99ce, 0xf5b8, 0x4d66,{0x90, 0xee, 0xfa, 0xde, 0xef, 0xbb, 0xf7, 0x13}}), kStructVersion1)

    //! v1 members go here, please do NOT break the C ABI compatibility:

    nvigi::Result (*someFunction)();

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvigi::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(IMyInterface)
```

`mygpt.cpp`

```cpp

// Add new API to the context

struct GPTContext
{
    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(GPTContext);

    void onCreateContext() {};
    void onDestroyContext() {};
    
    IMyInterface myapi{};

    // other bits
};

namespace mygpt
{

nvigi::Result someFunction()
{
  // Implement your function!  
}

}

// Now export the API
Result nvigiPluginRegister(framework::IFramework* framework)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;

    auto& ctx = (*mygpt::getContext());

    ctx.feature = mygpt::getFeatureId(nullptr);

    ctx.myapi.someFunction = mygpt::someFunction;
    
    framework->addInterface(ctx.feature, &ctx.myapi, 0);

    ...
}
```

`host app`
```cpp
nvigi::mygpt::IMyInterface* iMyApi{};
if(NVIGI_FAILED(error,nvigiGetInterface("nvigi::plugin::mygpt::ggml::$backend::kId", &iMyApi)))
{
  // handle error
}

// use your interface v1
iMyApi->someFunction();

// If interface changed and your app can load both old and new plugins always check the version
if(iMyApi->getVersion() >= kStructureVersion2)
{
  // safe to use v2 interface members
  iMyApi->someFunction2();
}

```

## Assistance

For any assistance or for questions regarding **ANY** changes to NVIGI Core, please contact your NVIDIA Developer Relations Manager

