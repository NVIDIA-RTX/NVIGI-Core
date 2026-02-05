# Plugin Development Tutorial

## Introduction

This document is meant to be a step-by-step tutorial on creating a new custom NVIGI plugin from a provided code template, and then integrating it into a simple command-line utility. This process is also described (at a much coarser and higher level) in the [Plugin Development Guide](PluginDevelopmentGuide.md), which we recommend to study as well.

As explained in the [Kinds of Plugins](PluginDevelopmentGuide.md#kinds-of-plugins) section of the plugin development guide, there are inference and utility NVIGI plugins. This tutorial will focus on creating one that follows an inference-like pattern from a provided code template; many of the same steps can also be used for utility plugins.

>**NOTE:** This document will assume you are developing your custom NVIGI plugin _inside_ of the NVIGI Core PDK directory structure in order to take advantage of the build scripts and procedures already in place. At a later time you may want to integrate your own development processes or build pipelines.

## Development Setup
Download the NVIGI Core Plugin Development Kit (PDK) from the latest entry in the [Releases](https://github.com/NVIDIA-RTX/NVIGI-Core/releases) page in the GitHub repository.
>**NOTE**: From this point on we will refer the directory path where the NVIGI Core PDK 7-zip file was extracted to as `<CORE_PDK_ROOT>`.

## Duplicating and Renaming the Template Plugin Source
>**NOTE**: For purposes of this guide, we will assume your new custom NVIGI plugin will be called `nvigi.mygpt`.
1. Go to `<CORE_PDK_ROOT>\sources\plugins\` and duplicate the folder `nvigi.template.inference` inside; rename it as `nvigi.mygpt`.
1. Go to `<CORE_PDK_ROOT>\sources\plugins\nvigi.mygpt\`
1. Rename public header `nvigi_template_infer.h` to `nvigi_mygpt.h`.
1. Open `<CORE_PDK_ROOT>\sources\plugins\nvigi.mygpt\nvigi_mygpt.h` in a text editor, and perform the following replacements:
    1. Rename the namespace for the plugin ID:
        - E.g.:
        ```cpp
        namespace template_ai
        ```
        becomes
        ```cpp
        namespace mygpt
        ```
    1. Rename all of the input slot IDs:
        - E.g.:
        ```cpp
        constexpr const char* kTemplateAIInputPrompt = "prompt";
        constexpr const char* kTemplateAIOutputResponse = "response";
        ```
        becomes
        ```cpp
        constexpr const char* kMyGPTInputPrompt = "prompt";
        constexpr const char* kMyGPTOutputResponse = "response";
        ```
    1. Rename each of the structs in the header file (declarations, constructors and also in the `NVIGI_VALIDATE_STRUCT` macro), replacing the substring `TemplateAI` with `MyGPT`
        - E.g.:
        ```cpp
        struct alignas(8) TemplateAICreationParameters
        {
            TemplateAICreationParameters() = default;
            NVIGI_UID(UID({ 0x11111111, 0x2222, 0x3333, {0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb} }), kStructVersion1)
        };

        NVIGI_VALIDATE_STRUCT(TemplateAICreationParameters)
        ```
        becomes
        ```cpp
        struct alignas(8) MyGPTCreationParameters
        {
            MyGPTCreationParameters() = default;
            NVIGI_UID(UID({ 0x11111111, 0x2222, 0x3333, {0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb} }), kStructVersion1)
        };

        NVIGI_VALIDATE_STRUCT(MyGPTCreationParameters)
        ```
    1. At the end of the header file, there is an using-declaration for the `InferenceInterface` class so it can be referred to as `ITemplateAI`; rename that using-declaration to `IMyGPT`
        - E.g.:
        ```cpp
        using ITemplateAI = InferenceInterface;
        ```
        becomes
        ```cpp
        using IMyGPT = InferenceInterface;
        ```
1. Use the NVIGI utility tool to generate new UIDs for the plugin
    1. Open a Visual Studio 2022 Developer Console to `<CORE_PDK_ROOT>`.
    1. Run the following command:
        ```
        bin\Debug_x64\nvigi.tool.utils.exe --plugin nvigi.plugin.mygpt
        ```
    1. Copy the entire line that begins with `constexpr PluginID kId = ...` in the console output, and replace the corresponding line in the header
        ```cpp
        namespace nvigi
        {
        namespace plugin
        {

        namespace mygpt
        {
            constexpr PluginID kId = {{0x54571404, 0x3d6a, 0x44a4,{0x8e, 0xf3, 0x70, 0x83, 0x97, 0x7b, 0x97, 0xf6}}, 0xa1e72b}; //{54571404-3D6A-44A4-8EF3-7083977B97F6} [nvigi.plugin.mygpt]

            ...
        }
        }
        ...
        ```
1. Use the same NVIGI utility tool to generate new UIDs for each struct
    1. For each struct in the header file, run the following command (replacing the name of the struct accordingly):
        ```cpp
        bin\Debug_x64\nvigi.tool.utils.exe --interface MyGPTCreationParameters
        ```
    1. Then replace the UID string as appropriate
        ```cpp
        struct alignas(8) MyGPTCreationParameters
        {
            MyGPTCreationParameters() { };
            NVIGI_UID(UID({0xc9d66831, 0x52e1, 0x4f02,{0x80, 0xed, 0x9f, 0x29, 0x2a, 0x7e, 0x30, 0x34}}), kStructVersion1)

            ...
        };

        NVIGI_VALIDATE_STRUCT(MyGPTCreationParameters)
        ```
    >**IMPORTANT NOTE**: Please notice some of the structs have a regular version (e.g. `MyGPTCreationParameters`) and an extended version (e.g. `MyGPTCreationParametersEx`); please make sure to repeat the UID generation process for each separately.
1. Rename folder `<CORE_PDK_ROOT>\sources\plugins\nvigi.mygpt\backend` to match the target backend you are using.
    - For example, the GPT plugin interface in the [NVIGI SDK release pack](https://developer.nvidia.com/rtx/in-game-inferencing) has multiple backends:
        - One based on REST APIs for AI inference in a cloud instance
        - One based on the [GGML](https://ggml.ai/) tensor library for local AI inference.
            - Further, this GGML backend also supports multiple APIs to implement the tensors: CUDA, Direct3D 12, Vulkan, a CPU-based non-accelerated one, etc.
        - For more details, please inspect the public git source available at the [GitHub Plugins source repo](https://github.com/NVIDIA-RTX/NVIGI-Plugins/tree/main/source/plugins/nvigi.gpt).
    - Ideally, all backends should implement the same interface declared in `nvigi_myplugin.h`
    - For purposes of this tutorial, we will leave the `backend` folder intact and as the only existing backend for our `nvigi.mygpt` plugin.
1. Next step, is to modify the implementation file to match the changes made to the interface in the header file.
    1. Rename the file `<CORE_PDK_ROOT>\sources\plugins\nvigi.mygpt\backend\templateEntry.cpp` to something else (e.g. `<CORE_PDK_ROOT>\sources\plugins\nvigi.mygpt\backend\myGPTEntry.cpp`), then open it in a text editor.
    1. Rename the class `TemplateAIPlugin` to `MyGPTPlugin`, including all references to it in the file. Best practice is using the text editor's search-and-replace functionality with the "match case" and "whole word" options enabled. This should replace it within the C++ class itself as well as the `NVIGI_MODERN_PLUGIN` plugin export macro at the end of the file.
    1. Repeat the previous substitution step with the `MyGPTPluginContext` struct, using the same parameters (both "match case" and "whole word" options on). Again, this should substitute it in the struct declaration and the `NVIGI_MODERN_PLUGIN` plugin export macro at the end of the file again.
    1. Next, apply the same substitution scheme to all the renamings that were done to the header file to the implementation file; meaning:
        1. Nested namespace `template_ai` to `mygpt`.
        1. Using-declaration `ITemplateAI` to `IMyGPT`.
        1. String constants `kTemplateAIInputPrompt` and `kTemplateAIOutputResponse` to `kMyGPTInputPrompt` and `kMyGPTOutputResponse`, respectively.
        1. Structs `TemplateAICreationParameters` and `TemplateAICreationParametersEx` to `MyGPTCreationParameters` and `MyGPTCreationParametersEx`, respectively.

## Modifying the Unit Tests
NVIGI Core ships with a set of unit tests for each plugin; these are implemented using the [Catch2](https://catch2.org/) testing framework. These unit tests are executed via a command-line utility (`nvigi.test.exe`), which source code is included in the PDK pack. In order to run those unit tests, simply run open a Visual Studio 2022 Development console to the `<CORE_PDK_ROOT>` directory, and execute the following command:
```
bin\Debug_x64\nvigi.test.exe
```
After the tests run, a timestamped log file inside `<CORE_PDK_ROOT>\bin\Debug_x64\` is created with the output.
This section explains how to modify the unit tests for the new custom plugin `nvigi.mygpt` and how to add them to the `nvigi.test.exe` utility.
1. Open the file `<CORE_PDK_ROOT>\source\plugins\nvigi.mygpt\backend\tests.h` in a file editor.
1. Update the first include macro to point to the renamed header file for the new plugin:
    ```cpp
    #include "source/plugins/nvigi.template.inference/nvigi_template_infer.h"
    ```
    becomes
    ```cpp
    #include "source/plugins/nvigi.mygpt/nvigi_mygpt.h"
    ```
1. Rename the nested namespace
    ```cpp
    namespace nvigi
    {
    namespace template_ai
    {
        ...
    }}
    ```
    becomes
    ```cpp
    namespace nvigi
    {
    namespace mygpt
    {
        ...
    }}
    ```
1. Use your text editor's search-and-replace functionality (with the "match case" and "whole word" options enabled) to replace the many uses of `plugin::template_ai::kId` for `plugin::mygpt::kId`. These are always used in calls to load or unload an interface to the new plugin.
1. Note the declaration of the old template plugin's interface:
    ```cpp
    nvigi::ITemplateAI* itemplate{};
    ```
    Using search-and-replace, rename all the occurences of `ITemplateAI` and `itemplate` to `IMyGPT` and `iMyGPT`, respectively.
1. Repeat the previous step for the old template plugin's creation parameters:
    ```cpp
    TemplateAICreationParameters templateParams{};
    ```
    Rename `TemplateAICreationParameters` and `templateParams` to `MyGPTCreationParameters` and `myGPTParams`, respectively.
1. Likewise, repeat the previous step and rename string constants `kTemplateAIInputPrompt` and `kTemplateAIOutputResponse` to `kMyGPTInputPrompt` and `kMyGPTOutputResponse`, respectively (as we did in the new plugin's header and implementation).
1. Seek all unit tests declarations in `tests.h`, and replace the test names to reflect the new plugin's name. For example:
    ```cpp
    TEST_CASE("template_ai_basic", "[template],[inference],[cpu]")
    {
        ...
    }
    ```
    becomes
    ```cpp
    TEST_CASE("mygpt_basic", "[inference],[cpu]")
    {
        ...
    }
    ```
    (Also do remove the `[template]` Catch2-style tag from the test case string)

## Duplicating and Renaming the Model Files
NVIGI inference plugins are always paired with one or more sets of AI model files. In the case of the inference template plugin (the one used as a starting point for the new plugin), the included model files are simply file placeholders, do not contain real data, and they are not truly loaded by the plugin. Given that, for now, the new plugin replicates the functionality of the inference template plugin, we must replicate the placeholder model files as well.
1. In a Windows Explorer window open to `<CORE_PDK_ROOT>\data\nvigi.models\`, duplicate the `nvigi.plugin.template.inference` directory and rename it to `nvigi.plugin.mygpt`.

## Modifying the Build Scripts
1. Modify `<CORE_PDK_ROOT>\tools\packaging\package.py` to add your project to the build system; we recommend using the corresponding sections for other plugins as an example. Here is a snapshot from the file showing various components:

    1. First, add the details of `plugin.mygpt` to the all component list:
        ```py
        all_components = {
            ...
            'plugin.template.inference' : {
                'platforms': all_plat,
                'sharedlib': ['nvigi.plugin.template.inference'],
                'includes': ['source/plugins/nvigi.template.inference/nvigi_template_infer.h'],
                'sources': ['plugins/nvigi.template.inference', 'shared'],
                'premake': 'source/plugins/nvigi.template.inference/premake.lua',
                'model': 'nvigi.plugin.template.inference',
                'public_models': ['{01234567-0123-0123-0123-0123456789AB}']
            },
            ...
        }
        ```
        becomes
        ```py
        all_components = {
            ...
            'plugin.template.inference' : {
                'platforms': all_plat,
                'sharedlib': ['nvigi.plugin.template.inference'],
                'includes': ['source/plugins/nvigi.template.inference/nvigi_template_infer.h'],
                'sources': ['plugins/nvigi.template.inference', 'shared'],
                'premake': 'source/plugins/nvigi.template.inference/premake.lua',
                'model': 'nvigi.plugin.template.inference',
                'public_models': ['{01234567-0123-0123-0123-0123456789AB}']
            },
            'plugin.mygpt' : {
                'platforms': all_plat,
                'sharedlib': ['nvigi.plugin.mygpt'],
                'includes': ['source/plugins/nvigi.mygpt/nvigi_mygpt.h'],
                'sources': ['plugins/nvigi.mygpt', 'shared'],
                'premake': 'source/plugins/nvigi.mygpt/premake.lua',
                'model': 'nvigi.plugin.mygpt',
                'public_models': ['{01234567-0123-0123-0123-0123456789AB}']
            },
            ...
        }
        ```
    1. Next, add `plugin.mygpt` to the runtime component list (the ones to be included in the package built):
        ```py
        runtime_components = [
            'core.framework',
            'plugin.hwi.common',
            'plugin.hwi.cuda',
            'plugin.hwi.d3d12',
            'plugin.template.generic',
            'plugin.template.inference',
            'test',
            'tool.utils'
        ]
        ```
        becomes
        ```py
        runtime_components = [
            'core.framework',
            'plugin.hwi.common',
            'plugin.hwi.cuda',
            'plugin.hwi.d3d12',
            'plugin.template.generic',
            'plugin.template.inference',
            'plugin.mygpt',
            'test',
            'tool.utils'
        ]
        ```
    1. Lastly, add `plugin.mygpt` to the runtime source component list (the ones included in the source for the build):
        ```py
        runtime_source_components = [
            'core.framework',
            'plugin.template.generic',
            'plugin.template.inference'
        ]
        ```
        becomes
        ```py
        runtime_source_components = [
            'core.framework',
            'plugin.template.generic',
            'plugin.template.inference',
            'plugin.mygpt'
        ]
        ```
1. Next we need to modify file `premake.lua` in your plugin's directory (`<CORE_PDK_ROOT>\source\plugins\nvigi.mygpt\premake.lua`) to include everything required to make your plugin build:
    ```lua
    group "plugins/template"
        project "nvigi.plugin.template.inference"
            kind "SharedLib"	
            
            pluginBasicSetup("template.inference")
        
            files { 
                "./**.h",
                "./**.cpp",			
            }	
            
            vpaths { ["impl"] = {"./**.h", "./**.cpp" }}
        
            includedirs {
                ROOT .. "source/plugins/nvigi.template.inference/backend",
                ROOT .. "source/plugins/nvigi.template.inference",
            }

            ...

    group ""
    ```
    becomes
    ```lua
    group "plugins/template"
        project "nvigi.plugin.mygpt"
            kind "SharedLib"	
            
            pluginBasicSetup("mygpt")
        
            files { 
                "./**.h",
                "./**.cpp",			
            }	
            
            vpaths { ["impl"] = {"./**.h", "./**.cpp" }}
        
            includedirs {
                ROOT .. "source/plugins/nvigi.mygpt/backend",
                ROOT .. "source/plugins/nvigi.mygpt",
            }

            ...

    group ""
    ```
1. Lastly, modify the global `<CORE_PDK_ROOT>\premake.lua` file to include the new plugin's `premake.lua` file. Simply append the following line:
    ```lua
    include("source/plugins/nvigi.mygpt/premake.lua")
    ```

## Building the CorePDK Source with the New Plugin
1. Open a Visual Studio 2022 Developer Console to `<CORE_PDK_ROOT>`.
1. Run `setup.bat vs2022`. This will create a folder called `_project` that contains all of the Visual Studio 2022 project and solution files.
    > NOTE:
    > The first time `setup.bat` runs it will download some build dependencies and install them in a central repository. Subsequent executins of `setup.bat` will not do that and will be much quicker.

    > NOTE:
    > If `setup.bat` fails with an error from the `packman` package downloader, please re-run `setup.bat` again as there are rare but possible issues with link-creation on initial run.
1. To build the project, there are two options:
    1. Open `<CORE_PDK_ROOT>\_project\vs2022\nvigicoresdk.sln` in Visual Studio 2022, and use it to build the entire solution, or
    1. Run `build.bat -Debug` on the developer console
1. Either of the previous two options will create a temporary folder called `_artifacts`, and it will also update the executable files and shared libraries inside `<CORE_PDK_ROOT>\bin\Debug_x86`

## Considerations when Developing Custom Plugins
* Modify source code (add new headers or cpp files as needed) to perform actions you need for your plugin
  * If adding new files, make sure to re-run `setup.bat`
  * If adding a new directory with source code, make sure to update your plugin's `premake.lua` file as well as re-running `setup.bat`
* Make sure NOT to include internal headers in your public `nvigi_$name.h` header
* When adding new data structures/interfaces which are shared either publicly or internally, you must use the `nvigi.tool.utils` as described in [GUID creation](#guid-creation-for-interfaces-and-plugins)  
* When modifying an existing shared data or interfaces **always follow the "do not break C ABI compatibility" guidelines**
* Wrap all you externally exposed functions with `NVIGI_CATCH_EXCEPTION`
* Choose your GPU backend(s) by uncommenting the appropriate sections in `premake.lua`:
  * `PLUGIN_USES_CUDA` for NVIDIA GPU support
  * `PLUGIN_USES_D3D12` for DirectX 12 support (Windows)
  * `PLUGIN_USES_VULKAN` for Vulkan support (cross-platform)
  * You can enable multiple backends simultaneously
