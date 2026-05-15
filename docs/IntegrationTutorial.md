# NVIGI Integration Tutorial

## Introduction

This document is meant to be a step-by-step tutorial on how to integrate the NVIGI core runtime and a GPU-accelerated AI inference plugin into an existing 3D game or application codebase. The main audience for this document are developers integrating NVIGI (or evaluating such integration) into their own 3D games or applications in order to take advantage of on-device, GPU-accelerated AI inference.

>**NOTE**: This tutorial document describes two types of integrations, addressing different project needs:
>- Modern: makes use of modern C++ practices (i.e. smart pointers, RAII, template type deduction, C++2x style function calling). Code is simpler, less error-prone, easier to read, and the official and best supported method of NVIGI integration going forward. It requires setting the compiler to C++23, which may clash with some existing codebases.
>- Legacy: uses more traditional C-like interfaces, which are more prevalent in legacy codebases, thus making it likely easier to integrate in some situations.
>
>All of the setup, deployment and testing steps are the same for both integration methods. Therefore, this document will only separate the integration steps into different sections while keeping the rest in common.

>**NOTE**: This document is meant to show an single, vertical end-to-end integration of a single NVIGI AI inference plugin. For more details about adding other types of NVIGI plugins, or how to chain multiple NVIGI plugins into a single pipeline, or how to create custom NVIGI pugins (AI inference or otherwise), please consult the combined [NVIGI SDK documentation](https://docs.nvidia.com/nvigi-sdk/1.6.0/docs/).

There are two components that will be integrated as part of this tutorial:
- The [latest binary release](https://github.com/NVIDIA-RTX/NVIGI-Core/releases) of the [NVIGI core runtime](https://github.com/NVIDIA-RTX/NVIGI-Core), and
- The [GPT GGML CUDA plugin](https://github.com/NVIDIA-RTX/NVIGI-Plugins/tree/main/source/plugins/nvigi.gpt/ggml) from the [NVIGI Plugin SDK](https://github.com/NVIDIA-RTX/NVIGI-Plugins), which we will obtain and copy over from the [latest Plugin SDK binary release pack](https://developer.nvidia.com/rtx/in-game-inferencing). We will need
    - The plugin DLL,
    - Any dependency DLLs, and
    - One of the supported AI models (in this case, [Qwen3 8B](https://huggingface.co/Qwen/Qwen3-8B))

>**NOTE**: We will include the steps for acquiring the Qwen3 8B model files later in this document; there is no need to manually download them from the HuggingFace website).

While the NVIGI SDK binary pack will not be formally integrated into the existing 3D game of application, certain parts of it (i.e. a few header files, the model download scripts, etc.) will be used without copying them over onto the existing 3D game or application codebase. Therefore, we will bootstrap the NVIGI SDK, and then we will add some include paths to it to our existing 3D game project files as needed.

>**NOTE**: Both soon-to-be-integrated NVIGI components mentioned above (the core runtime and the GPT GGML CUDA plugin), as well as the NVIGI SDK binary pack, can be built from source; for instructions on how to do it, please refer to the README files in their respective GitHub repositories.

For this specific tutorial, the existing codebase that will be the target of NVIGI integration will be the "[Hello, triangle!](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle)" sample in the "[Direct3D 12 Hello World! samples](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld)", which are part of the [DirectX Graphics Samples](https://github.com/microsoft/DirectX-Graphics-Samples) collection published by Microsoft.

## Setup

### Development Environment Setup

For NVIGI's plugin setup requirements, check the [Prerequisites](https://github.com/NVIDIA-RTX/NVIGI-Plugins/blob/main/docs/Plugins.md#prerequisites) section in the NVIGI SDK documentation for minimum supported hardware, operating system, graphics drivers and development tools.

One additional system-wide dependency that is required in order to take advantage of all possible optimizations (such as CUDA performance optimizations when running concurrently with a heavy graphics workload) is the LunarG Vulkan SDK. Go to https://vulkan.lunarg.com/sdk/home and download the latest SDK installer. Please execute the installer and remember the path the SDK was installed to (e.g. `C:\VulkanSDK\y.y.y.y\`, where `y.y.y.y` is the Vulkan SDK version number). We will refer to this path (including the directory with the version number) as `${VULKAN_ROOT}`.

### DirectX Graphics Samples setup

Follow the [instructions](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/README.md) on how to clone and build the DirectX Graphics Samples from Microsoft's GitHub repository. Make sure to get the commit with the following SHA commit ID: `bf02bf045dbfc799aaca67a70afefcf9c0a40751`

>**NOTE**: From this point on we will refer to the directory path where the DirectX Graphics Samples repository has been cloned to as `${SAMPLES_ROOT}`.

Specifically, make sure you are able to build the solution file `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\D3D12HelloWorld.sln` in the `Debug` configuration.

Also, make sure to set the `D3D12HelloTriangle` Visual Studio project as the startup project, and that you can run and debug it through Visual Studio.

### NVIGI Core Setup

Download the latest NVIGI core runtime (file named `nvigi_core_runtime_sdk_x.x.x.7z`) from the project's [Releases page](https://github.com/NVIDIA-RTX/NVIGI-Core/releases) in the GitHub repository.

Decompress the contents of the 7-zip file into a folder called `nvigi_core_runtime_sdk_x.x.x` (where `x.x.x` is the version number; please replace the correct version number wherever `x.x.x` is used in this document).

Move the folder with the decompressed contents into `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle`.

### NVIGI SDK Setup

Download the NVIDIA NVIGI SDK binary pack from https://developer.nvidia.com/rtx/in-game-inferencing. Unzip the contents of the 7-zip file to some location, which we will refer to as `${SDK_ROOT}` in this document.

Open the `${SDK_ROOT}\README.html` file in a web browser, and then click on the `First Steps` link in the left navigation bar. At a minimum, please follow the first two recommended first steps (downloading the AI models and running the 3D sample).

### NVIGI GPT GGML CUDA Plugin Setup

The NVIGI core runtime does not ship with the GPT plugin, so it is necessary to copy its files (header, plugin DLL and dependency DLLs) from the NVIGI SDK binary pack into the NVIGI core runtime location.
Copy the following files into the locations detailed below:
- `${SDK_ROOT}\include\nvigi_gpt.h` -> `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\nvigi_core_runtime_sdk_x.x.x\include\`
- `${SDK_ROOT}\bin\x64\nvigi.plugin.gpt.ggml.cuda.dll` -> `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\nvigi_core_runtime_sdk_x.x.x\bin\x64\Debug\`
- `${SDK_ROOT}\bin\x64\cublas64_12.dll` -> `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\nvigi_core_runtime_sdk_x.x.x\bin\x64\Debug\`
- `${SDK_ROOT}\bin\x64\cublasLt64_12.dll` -> `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\nvigi_core_runtime_sdk_x.x.x\bin\x64\Debug\`

## Performing the Integration: Modern Method

As explained in a note above, this method is best suited for integrating into a new project, or into an existing project built against the C++23 standard. Whenever convenient, this method is the preferred method of integration, as it allows for a simpler, cleaner and easier to maintain code integration.

### Modifications to D3D12HelloTriangle Project Files
- First, open the solution file `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\D3D12HelloWorld.sln` in Visual Studio.
- In the Solution Explorer, right-click on `D3D12HelloTriangle` project, and select `Properties` to open the project's Properties window.
- With the `D3D12HelloTriangle` project's Properties window open, change the following settings:
    - In `Configuration Properties` -> `General` -> `Language`, click on `C++ Language Standard` and change the value to `Preview - Features from the Latest C++ Working Draft (/std:c++latest)`.
    - In `Configuration Properties` -> `C/C++` -> `General`, click on `Additional Include Directories`, and add the following entries:
        - `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\nvigi_core_runtime_sdk_x.x.x\include`.
        - `${SDK_ROOT}\source\samples\nvigi.basic.cxx`
        - `${VULKAN_ROOT}\Include`
    - In `Configuration Properties` -> `Build Events` -> `Post-Build Event`, click on `Command Line` and add the following text: `XCOPY "$(ProjectDir)"\nvigi_core_runtime_sdk_x.x.x\bin\Debug_x64\*.dll "$(TargetDir)" /D /K /Y`
    - (Optional) In `Configuration Properties` -> `Build Events` -> `Post-Build Event`, click on `Description` and add the following text: `Copy NVIGI DLLs to Target Directory`
- (Optional) In the Solution Explorer, right-click on `D3D12HelloTriangle` project, and select `Set as Startup Project`.

Alternatively, the file `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\D3D12HelloTriangle.vcxproj` can be manually edited to achieve the same results without using the Visual Studio graphical user interface.
- Add the `<LanguageStandard>stdcpplatest</LanguageStandard>` tag to the `<ClInclude>` tag.
- Edit the `<AdditionalIncludeDirectories>` tag to append `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\nvigi_core_runtime_sdk_x.x.x\include;${SDK_ROOT}\source\samples\nvigi.basic.cxx;${VULKAN_ROOT}\Include` to its current value.
- Add the following to the `<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">` tag:
```xml
<PostBuildEvent>
  <Command>XCOPY "$(ProjectDir)"\nvigi_core_runtime_sdk_x.x.x\bin\Debug_x64\*.dll "$(TargetDir)" /D /K /Y</Command>
</PostBuildEvent>
<PostBuildEvent>
  <Message>Copy NVIGI DLLs to Target Directory</Message>
</PostBuildEvent>
```

### Modifications to D3D12HelloTriangle Header File

First, our project will need to keep a few NVIGI data structures kept in `D3D12HelloTriangle.h`. This is so that these can be accessible from the different lifecycle- and rendering-loop-related member functions of the sample class.

We need to add the following header files:
```cpp
#include <atomic>
#include <chrono>
#include <memory>
#include <string>

#include "nvigi.h"
#include "nvigi_ai.h"
#include "nvigi_gpt.h"
#include "nvigi_cloud.h"
#include "nvigi_d3d12.h"
#include "nvigi_vulkan.h"
#include "core.hpp"
#include "d3d12.hpp"
#include "vulkan.hpp"
#include "gpt/gpt.hpp"
```

Also, inside of the `D3D12HelloTriangle` class definition, we need to add the following private member fields:
```cpp
    std::atomic_bool m_inferenceRunning{ false };
    std::chrono::time_point<std::chrono::steady_clock> m_lastInferenceTime{};
    std::string m_cummulativeResponse; // Store cumulative response for chat history
    std::unique_ptr<nvigi::Core> m_IGICore{ nullptr };
    std::unique_ptr<nvigi::d3d12::DeviceAndQueue> m_IGIDeviceAndQueue{ nullptr };
    std::unique_ptr<nvigi::d3d12::D3D12Config> m_IGID3D12Config{ nullptr };
    std::unique_ptr<nvigi::gpt::Instance> m_IGIGPTInstance{ nullptr };
    std::unique_ptr<nvigi::gpt::Instance::Chat> m_IGIGPTChat{ nullptr };
    std::unique_ptr<nvigi::gpt::Instance::AsyncOperation> m_IGIGPTAsyncOp{ nullptr };
```

The first three help manage the logic of the demo (i.e. start inference every 10 seconds). The last few are data structures needed for NVIGI to work.

Lastly, we will also add the declarations for a few private member functions that will assist with handling NVIGI's lifecycle (these will be explained as they are implemented later):
```cpp
void IGIInit();
void IGIShutdown();
void IGIGPTInit();
void IGIGPTRelease();
void IGIGPTStartInference();
void IGIGPTUpdateInference();
```

### Modifications to D3D12HelloTriangle Implementation File

There are a few changes that need to be made to `D3D12HelloTriangle.cpp` to help it compile in C++23 (since the Microsoft DirectX Graphics Samples codebase predates that newer standard, and a couple of idioms used are no longer valid).
- Replace the following line:
    ```cpp
    extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }
    ```
    with
    ```cpp
    extern "C" { __declspec(dllexport) extern const char8_t* D3D12SDKPath = u8".\\D3D12\\"; }
    ```
- Replace the following function call:
    ```cpp
    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));
    ```
    with
    ```cpp
    auto heapType{ CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD) };
    auto buffer{ CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize) };
    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapType,
        D3D12_HEAP_FLAG_NONE,
        &buffer,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));
    ```
- Replace the following function call:
    ```cpp
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    ```
    with
    ```cpp
    auto transition{ CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET) };
    m_commandList->ResourceBarrier(1, &transition);
    ```
- Replace the following function call:
    ```cpp
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    ```
    with
    ```cpp
    auto transition2{ CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT) };
    m_commandList->ResourceBarrier(1, &transition2);
    ```

Now, we resume the NVIGI integration. We will first add a few additional header includes.
```cpp
#include <iostream>
#include <stringapiset.h>
```

Now, we want to define a couple of global variables (in the default namespace for this source file) that will help us direct how NVIGI will integrate with the sample. For simplicity's sake, these values will be hardcoded in our source file; a real integration would utilize a more appropriate method for setting these up. Right below the header includes, add the following:
```cpp
namespace
{
    constexpr unsigned int NVIGI_INFERENCE_INTERVAL_IN_SECS{ 10u };
    constexpr const char* NVIGI_SYSTEM_PROMPT{ "You are a helpful math tutor." };
    constexpr const char* NVIGI_USER_PROMPT{ "What is two plus two?" };
    constexpr const char* NVIGI_PATH_TO_MODELS{ "D:\\git\\IGI\\sdk\\data\\nvigi.models\\" };
    constexpr const char* NVIGI_MODEL_GUID{ "{545F7EC2-4C29-499B-8FC8-61720DF3C626}" }; // Qwen3 8B Q4
    constexpr nvigi::PluginID NVIGI_PLUGIN_ID{ nvigi::plugin::gpt::ggml::cuda::kId };
}
```

For a quick explanation of these variables:
- `NVIGI_INFERENCE_INTERVAL_IN_SECS` determines how often the sample will use NVIGI to run GPT inference.
- `NVIGI_SYSTEM_PROMPT` and `NVIGI_USER_PROMPT` are the text that the sample will be passing to the AI model as system and user prompt, respectively.
- `NVIGI_PATH_TO_MODELS` is the hardcoded path to the folder inside your downloaded instance of the NVIGI SDK release pack where you have downloaded the models. For example, if you extracted the NVIGI SDK pack 7-zip file to `${NVIGI_SDK_ROOT}`, by default this string should be `${NVIGI_SDK_ROOT}\\data\\nvigi.models\\` (the backslashes in the C-style string path should be properly escaped).
- `NVIGI_MODEL_GUID` is the globally-unique identifier for the specific GPT model we will be using. You can find the ones that are available inside `${NVIGI_SDK_ROOT}\data\nvigi.models\nvigi.plugin.gpt.ggml`.
- `NVIGI_PLUGIN_ID` is a data structure defined inside the `nvigi_gpt.h` header which uniquely identifies a specific plugin backend (and specific DLL). The value of this variable should match the plugin DLL that is copied over from the NVIGI SDK binary files.

Next, let us go to the definition of `D3D12HelloTriangle::OnInit`. Here we want to create the NVIGI global context, load a specific plugin and model, and initialize the execution logic. Let us modify the function to look like this:
```cpp
void D3D12HelloTriangle::OnInit()
{
    LoadPipeline();
    LoadAssets();

    IGIInit();
    IGIGPTInit();
}
```

We will provide the implementation of those new member functions later.

Likewise, we want to perform the opposite steps in `D3D12HelloTriangle::OnDestroy`:
```cpp
void D3D12HelloTriangle::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);

    IGIGPTRelease();
    IGIShutdown();
}
```

In `D3D12HelloTriangle::OnUpdate`, we will add logic to check if it is time to perform inference (and do so if this is the case).

Also, since this NVIGI integration uses a non-blocking, polling approach (well suited for integration into game engines, which have strict requirements for how much work can be done in each engine tick), we call `IGIGPTUpdateInference` which will retrieve results from the NVIGI GPT plugin only if they are available.
```cpp
void D3D12HelloTriangle::OnUpdate()
{
    const std::chrono::time_point<std::chrono::steady_clock> currentTime{
        std::chrono::steady_clock::now() };

    const std::chrono::duration<double> timeDiff{ currentTime - m_lastInferenceTime };

    if ( (timeDiff.count() > NVIGI_INFERENCE_INTERVAL_IN_SECS) && !m_inferenceRunning.load() )
    {
        IGIGPTStartInference();
        m_lastInferenceTime = currentTime;
    }

    if (m_inferenceRunning.load())
    {
        IGIGPTUpdateInference();
    }
}
```

For convenience's sake, we will add a couple of helper functions to the void `D3D12HelloTriangle` class:
```cpp
std::string getExecutablePath()
{
    CHAR pathAbsW[MAX_PATH] = {};
    GetModuleFileNameA(GetModuleHandleA(NULL), pathAbsW, ARRAYSIZE(pathAbsW));
    std::string searchPathW = pathAbsW;
    searchPathW.erase(searchPathW.rfind('\\'));
    return searchPathW + "\\";
}

void loggingCallback(nvigi::LogType type, const char* msg)
{
    if (type == nvigi::LogType::eError)
    {
        OutputDebugStringA(msg);
        std::cout << msg;
    }
}
```

These two functions are related to NVIGI's idiosyncrasies. The former helps us find the location of the sample's executable (which is an NVIGI context creation parameter used for log file saving and other automation tasks), and the latter is a callback we provide to NVIGI for logging-to-console purposes.

Now, with all the new member functions called at the appropriate times in the sample's lifecycle, we provide the implementations for such functions. First, `D3D12HelloTriangle::IGIInit`:
```cpp
void D3D12HelloTriangle::IGIInit()
{
    const std::string path{ getExecutablePath() };

    m_IGICore = std::make_unique<nvigi::Core>(nvigi::Core::Config({
        .sdkPath = path,
        .logLevel = static_cast<nvigi::LogLevel>(nvigi::LogLevel::eDefault),
        .showConsole = true
        }));
    m_IGIDeviceAndQueue = std::make_unique<nvigi::d3d12::DeviceAndQueue>(
        nvigi::d3d12::D3D12Helper::create_best_compute_device());
    m_IGID3D12Config = std::make_unique<nvigi::d3d12::D3D12Config>();
    (*m_IGID3D12Config)
        .set_device(m_IGIDeviceAndQueue->device.Get())
        .set_queue(m_IGIDeviceAndQueue->compute_queue.Get())
        .set_create_committed_resource_callback(nvigi::d3d12::default_create_committed_resource)
        .set_destroy_resource_callback(nvigi::d3d12::default_destroy_resource)
        .set_create_resource_user_context(nullptr)
        .set_destroy_resource_user_context(nullptr);
}
```
`D3D12HelloTriangle::IGIInit` does a few important things:
- It creates and initializes NVIGI itself, including loads the NVIGI core framework DLL,
- It retrieves the D3D12 data structures in the 3D game that are relevant for NVIGI, and
- It configures NVIGI to reuse those data structures in order to avoid GPU resources and bandwidth contention

Conversely, `D3D12HelloTriangle::IGIShutdown` looks as such:
```cpp
void D3D12HelloTriangle::IGIShutdown()
{
    m_IGID3D12Config.reset();
    m_IGIDeviceAndQueue.reset();
    m_IGICore.reset();
}
```

Next, `D3D12HelloTriangle::IGIGPTInit` will:
- Create a instance of an API that will automatically get the capabilities of the hardware as well as references to the relevant D3D12 data structures,
- Create an instance of a GPT interface,
- Create an instance of a polled inference interface, and
- Load a specific plugin and model pair.

```cpp
void D3D12HelloTriangle::IGIGPTInit()
{
    m_IGIGPTInstance = nvigi::gpt::Instance::create(
        nvigi::gpt::ModelConfig{
            .backend = "cuda",
            // guid and model_path are used in traditional mode. When model_card_json
            // is provided, these are overridden to empty by the wrapper internally.
            .guid = NVIGI_MODEL_GUID,
            .model_path = NVIGI_PATH_TO_MODELS,
            .context_size = 4096,
            .num_threads = 4,
            .vram_budget_mb = 8192u,
            .flash_attention = true,
            .cache_type = "fp16"
        },
        *m_IGID3D12Config,
        {},
        {},
        m_IGICore->loadInterface(),
        m_IGICore->unloadInterface(),
        ""
    ).value();

    nvigi::gpt::RuntimeConfig runtime_config;
    runtime_config.set_tokens(256)
        .set_batch_size(2048)
        .set_temperature(0.3f)
        .set_top_p(0.8f)
        .set_interactive(true)
        .set_reverse_prompt("\nAssistant:");

    m_IGIGPTChat = std::make_unique<nvigi::gpt::Instance::Chat>(m_IGIGPTInstance->create_chat(runtime_config));
}
```

The GPT instance is an abstration of the type of plugin we want to use (e.g. a general-purpose transformer AI inference plugin). The specifics of the implementation vary in terms of the technology used (i.e. GGML, cloud, a CPU implementation, etc.) as well as the specific backend for such implementation(such as GGML having CUDA, D3D12, Vulkan and CPU implementation backends).

Likewise, `D3D12HelloTriangle::IGIGPTRelease` will dispose of the data structures created in `D3D12HelloTriangle::IGIGPTInit`.
```cpp
void D3D12HelloTriangle::IGIGPTRelease()
{
    m_IGIGPTChat.reset();
    m_IGIGPTInstance.reset();
}
```

The `D3D12HelloTriangle::IGIGPTStartInference` implementation is as follows:
```cpp
void D3D12HelloTriangle::IGIGPTStartInference()
{
    auto op = m_IGIGPTChat->send_message_polled(
        {
            .role = nvigi::gpt::Instance::Chat::Message::Role::System,
            .content = NVIGI_SYSTEM_PROMPT
        }
    ).value();

    op = m_IGIGPTChat->send_message_polled(
        {
            .role = nvigi::gpt::Instance::Chat::Message::Role::User,
            .content = NVIGI_USER_PROMPT
        }
    ).value();

    m_IGIGPTAsyncOp = std::make_unique<nvigi::gpt::Instance::AsyncOperation>(std::move(op));

    m_inferenceRunning.store(true);
}
```

In summary, this function:
- Submits two messages with inputs (the system and user prompts) to the NVIGI GPT plugin's work queue, and
- Kicks off asynchronous inference

Finally, the implementation of `D3D12HelloTriangle::IGIGPTUpdateInference`:
```cpp
void D3D12HelloTriangle::IGIGPTUpdateInference()
{
    // Poll for tokens (non-blocking - returns immediately!)
    if (auto result = m_IGIGPTAsyncOp->try_get_results())
    {
        std::cout << result->tokens; // Show immediately!
        m_cummulativeResponse += result->tokens;

        // Can inspect state for debugging or control flow
        if (result->state == nvigi::gpt::ExecutionState::Done)
        {
            std::cout << "\n[System prompt inference complete]";
        }
        else if (result->state == nvigi::gpt::ExecutionState::Cancel)
        {
            std::cout << "\n[System prompt inference cancelled]";
        }
    }

    // Check if done
    if (m_IGIGPTAsyncOp->is_complete())
    {
        m_IGIGPTChat->finalize_async_response(*m_IGIGPTAsyncOp);
        m_IGIGPTAsyncOp->reset();
        m_IGIGPTAsyncOp.reset();

        std::cout << "NVIGI GPT response:\t\"" << m_cummulativeResponse << "\"" << std::endl;
        m_cummulativeResponse = "";
        m_inferenceRunning.store(false);
    }
}
```

This method will use the polled inference interface to check if there are any incoming tokens and, if so, it will:
- Retrieve the tokens (if any are available),
- Append it to the cummulative response member variable,
- Check if this is the last token; if so, print the result and reset inference logic so it can start again with the next call to `D3D12HelloTriangle::IGIGPTStartInference`.

## Performing the Integration: Legacy Method

As explained in the note at the beginning of this document, this method is best suited for integrating into an existing codebase built against the C++17 standard.

### Modifications to D3D12HelloTriangle Project Files
- First, open the solution file `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\D3D12HelloWorld.sln` in Visual Studio.
- In the Solution Explorer, right-click on `D3D12HelloTriangle` project, and select `Properties` to open the project's Properties window.
- With the `D3D12HelloTriangle` project's Properties window open, change the following settings:
    - In `Configuration Properties` -> `General` -> `Language`, click on `C++ Language Standard` and change the value to `ISO C++17 Standard (/std:c++17)`.
    - In `Configuration Properties` -> `C/C++` -> `General`, click on `Additional Include Directories`, and add the following entries:
        - `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\nvigi_core_runtime_sdk_x.x.x\include`.
        - `${SDK_ROOT}\nvigi_core\`
        - `${SDK_ROOT}\external\cuda\include`
        - `${SDK_ROOT}\external\cuda\extras\CUPTI\include`
        - `${SDK_ROOT}\external\cig_scheduler_settings\include`
        - `${VULKAN_ROOT}\Include`
    - In `Configuration Properties` -> `Build Events` -> `Post-Build Event`, click on `Command Line` and add the following text: `XCOPY "$(ProjectDir)"\nvigi_core_runtime_sdk_x.x.x\bin\Debug_x64\*.dll "$(TargetDir)" /D /K /Y`
    - (Optional) In `Configuration Properties` -> `Build Events` -> `Post-Build Event`, click on `Description` and add the following text: `Copy NVIGI DLLs to Target Directory`
- (Optional) In the Solution Explorer, right-click on `D3D12HelloTriangle` project, and select `Set as Startup Project`.

Alternatively, the file `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\D3D12HelloTriangle.vcxproj` can be manually edited to achieve the same results without using the Visual Studio graphical user interface.
- Add the `<LanguageStandard>stdcpp17</LanguageStandard>` tag to the `<ClInclude>` tag.
- Edit the `<AdditionalIncludeDirectories>` tag to append `${SAMPLES_ROOT}\Samples\Desktop\D3D12HelloWorld\src\HelloTriangle\nvigi_core_runtime_sdk_x.x.x\include;${SDK_ROOT}\nvigi_core\;${SDK_ROOT}\external\cuda\include;${SDK_ROOT}\external\cuda\extras\CUPTI\include;${SDK_ROOT}\external\cig_scheduler_settings\include;${VULKAN_ROOT}\Include` to its current value.
- Add the following to the `<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">` tag:
```xml
<PostBuildEvent>
  <Command>XCOPY "$(ProjectDir)"\nvigi_core_runtime_sdk_x.x.x\bin\Debug_x64\*.dll "$(TargetDir)" /D /K /Y</Command>
</PostBuildEvent>
<PostBuildEvent>
  <Message>Copy NVIGI DLLs to Target Directory</Message>
</PostBuildEvent>
```

### Modifications to D3D12HelloTriangle Header File

First, our project will need to keep a few NVIGI data structures kept in `D3D12HelloTriangle.h`. This is so that these can be accessible from the different lifecycle- and rendering-loop-related member functions of the sample class.

We need to add the following header files:
```cpp
#include <atomic>
#include <chrono>
#include <string>

#include "nvigi.h"
#include "nvigi_ai.h"
#include "nvigi_gpt.h"
#include "nvigi_hwi_cuda.h"
```

Also, inside of the `D3D12HelloTriangle` class definition, we need to add the following private member fields:
```cpp
    std::atomic_bool m_inferenceRunning{ false };
    std::chrono::time_point<std::chrono::steady_clock> m_lastInferenceTime{};
    std::string m_cummulativeResponse; // Store cumulative response for chat history
#define DECLARE_NVIGI_CORE_FUN(F) PFun_##F* F
    DECLARE_NVIGI_CORE_FUN(nvigiInit);
    DECLARE_NVIGI_CORE_FUN(nvigiShutdown);
    DECLARE_NVIGI_CORE_FUN(nvigiLoadInterface);
    DECLARE_NVIGI_CORE_FUN(nvigiUnloadInterface);
    nvigi::IHWICuda* m_IGICuda{ nullptr };
    nvigi::IGeneralPurposeTransformer* m_IGIGPTInterface{ nullptr };
    nvigi::IPolledInferenceInterface* m_IGIPolledInterface{ nullptr };
    nvigi::InferenceInstance* m_IGIGPTInstance{ nullptr };
    nvigi::InferenceExecutionContext* m_IGIGPTExecCtx{ nullptr };
```

The first three help manage the logic of the demo (i.e. start inference every 10 seconds). The ones used with the `DECLARE_NVIGI_CORE_FUN` macro hold function pointers to the runtime-loaded NVIGI core DLL. The last few are data structures needed for NVIGI to work.

Lastly, we will also add the declarations for a few private member functions that will assist with handling NVIGI's lifecycle (these will be explained as they are implemented later):
```cpp
void IGIInit();
void IGIShutdown();
void IGIGPTInit();
void IGIGPTRelease();
void IGIGPTStartInference();
void IGIGPTUpdateInference();
```

### Modifications to D3D12HelloTriangle Implementation File

Now, open `D3D12HelloTriangle.cpp`. We will first add a few additional header includes.
```cpp
#include <iostream>

#include "nvigi_stl_helpers.h"
#include "source/utils/nvigi.cig_compatibility_checker/CIG_compatibility_checker.h"
```

Now, we want to define a couple of global variables (in the default namespace for this source file) that will help us direct how NVIGI will integrate with the sample. For simplicity's sake, these values will be hardcoded in our source file; a real integration would utilize a more appropriate method for setting these up. Right below the header includes, add the following:
```cpp
namespace
{
    constexpr unsigned int NVIGI_INFERENCE_INTERVAL_IN_SECS{ 10u };
    constexpr const char* NVIGI_SYSTEM_PROMPT{ "You are a helpful math tutor." };
    constexpr const char* NVIGI_USER_PROMPT{ "What is two plus two?" };
    constexpr const char* NVIGI_PATH_TO_MODELS{ "D:\\git\\IGI\\sdk\\data\\nvigi.models\\" };
    constexpr const char* NVIGI_MODEL_GUID{ "{545F7EC2-4C29-499B-8FC8-61720DF3C626}" }; // Qwen3 8B Q4
    constexpr nvigi::PluginID NVIGI_PLUGIN_ID{ nvigi::plugin::gpt::ggml::cuda::kId };
}
```

For a quick explanation of these variables:
- `NVIGI_INFERENCE_INTERVAL_IN_SECS` determines how often the sample will use NVIGI to run GPT inference.
- `NVIGI_SYSTEM_PROMPT` and `NVIGI_USER_PROMPT` are the text that the sample will be passing to the AI model as system and user prompt, respectively.
- `NVIGI_PATH_TO_MODELS` is the hardcoded path to the folder inside your downloaded instance of the NVIGI SDK release pack where you have downloaded the models. For example, if you extracted the NVIGI SDK pack 7-zip file to `${NVIGI_SDK_ROOT}`, by default this string should be `${NVIGI_SDK_ROOT}\\data\\nvigi.models\\` (the backslashes in the C-style string path should be properly escaped).
- `NVIGI_MODEL_GUID` is the globally-unique identifier for the specific GPT model we will be using. You can find the ones that are available inside `${NVIGI_SDK_ROOT}\data\nvigi.models\nvigi.plugin.gpt.ggml`.
- `NVIGI_PLUGIN_ID` is a data structure defined inside the `nvigi_gpt.h` header which uniquely identifies a specific plugin backend (and specific DLL). The value of this variable should match the plugin DLL that is copied over from the NVIGI SDK binary files.

Next, let us go to the definition of `D3D12HelloTriangle::OnInit`. Here we want to create the NVIGI global context, load a specific plugin and model, and initialize the execution logic. Let us modify the function to look like this:
```cpp
void D3D12HelloTriangle::OnInit()
{
    LoadPipeline();
    LoadAssets();

    IGIInit();
    IGIGPTInit();
}
```

We will provide the implementation of those new member functions later.

Likewise, we want to perform the opposite steps in `D3D12HelloTriangle::OnDestroy`:
```cpp
void D3D12HelloTriangle::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);

    IGIGPTRelease();
    IGIShutdown();
}
```

In `D3D12HelloTriangle::OnUpdate`, we will add logic to check if it is time to perform inference (and do so if this is the case).

Also, since this NVIGI integration uses a non-blocking, polling approach (well suited for integration into game engines, which have strict requirements for how much work can be done in each engine tick), we call `IGIGPTUpdateInference` which will retrieve results from the NVIGI GPT plugin only if they are available.
```cpp
void D3D12HelloTriangle::OnUpdate()
{
    const std::chrono::time_point<std::chrono::steady_clock> currentTime{
        std::chrono::steady_clock::now() };

    const std::chrono::duration<double> timeDiff{ currentTime - m_lastInferenceTime };

    if ( (timeDiff.count() > NVIGI_INFERENCE_INTERVAL_IN_SECS) && !m_inferenceRunning.load() )
    {
        IGIGPTStartInference();
        m_lastInferenceTime = currentTime;
    }

    if (m_inferenceRunning.load())
    {
        IGIGPTUpdateInference();
    }
}
```

For convenience's sake, we will add a couple of helper functions to the void `D3D12HelloTriangle` class:
```cpp
std::string getExecutablePath()
{
    CHAR pathAbsW[MAX_PATH] = {};
    GetModuleFileNameA(GetModuleHandleA(NULL), pathAbsW, ARRAYSIZE(pathAbsW));
    std::string searchPathW = pathAbsW;
    searchPathW.erase(searchPathW.rfind('\\'));
    return searchPathW + "\\";
}

void loggingCallback(nvigi::LogType type, const char* msg)
{
    if (type == nvigi::LogType::eError)
    {
        OutputDebugStringA(msg);
        std::cout << msg;
    }
}
```

These two functions are related to NVIGI's idiosyncrasies. The former helps us find the location of the sample's executable (which is an NVIGI context creation parameter used for log file saving and other automation tasks), and the latter is a callback we provide to NVIGI for logging-to-console purposes.

Now, with all the new member functions called at the appropriate times in the sample's lifecycle, we provide the implementations for such functions. First, `D3D12HelloTriangle::IGIInit`:
```cpp
void D3D12HelloTriangle::IGIInit()
{
    const std::string path{ getExecutablePath() };

    std::string libPath{ path + "/nvigi.core.framework.dll" };
    int wchars_num = MultiByteToWideChar(CP_UTF8, 0, libPath.c_str(), -1, NULL, 0);
    wchar_t* libPathW = new wchar_t[wchars_num];
    MultiByteToWideChar(CP_UTF8, 0, libPath.c_str(), -1, libPathW, wchars_num);
    HMODULE lib = LoadLibraryW(libPathW);
    delete[] libPathW;

    if (lib == NULL)
    {
        // Handle error
        return;
    }

#define GET_NVIGI_CORE_FUN(F) F = (PFun_##F*)GetProcAddress(lib, #F)
    GET_NVIGI_CORE_FUN(nvigiInit);
    GET_NVIGI_CORE_FUN(nvigiShutdown);
    GET_NVIGI_CORE_FUN(nvigiLoadInterface);
    GET_NVIGI_CORE_FUN(nvigiUnloadInterface);

    const char* paths[] =
    {
        path.c_str()
    };

    nvigi::Preferences pref{};
    pref.showConsole = true;
    pref.logLevel = nvigi::LogLevel::eVerbose;
    pref.logMessageCallback = loggingCallback;
    pref.numPathsToPlugins = 1;
    pref.utf8PathsToPlugins = paths;
    pref.utf8PathToLogsAndData = path.c_str();

    if (nvigi::resultIsError(nvigiInit(pref, nullptr, nvigi::kSDKVersion)))
    {
        // Handle error
        return;
    }

    if (nvigi::resultIsError(nvigiGetInterfaceDynamic(nvigi::plugin::hwi::cuda::kId, &m_IGICuda, nvigiLoadInterface)))
    {
        // Handle error
        return;
    }
}
```
`D3D12HelloTriangle::IGIInit` does a few important things:
- It finds where the executable is located,
- It dynamically loads the NVIGI core framework DLL,
- It gets function pointers to NVIGI's core functions,
- It initializes NVIGI itself, and
- It initializes an instance of the (optional) NVIGI hardware interface plugin, which can help streamline much of the work performed by NVIGI plugins when additional GPU workloads are executing.

Conversely, `D3D12HelloTriangle::IGIShutdown` looks as such:
```cpp
void D3D12HelloTriangle::IGIShutdown()
{
    if (nvigi::resultIsError(nvigiUnloadInterface(nvigi::plugin::hwi::cuda::kId, &m_IGICuda)))
    {
        // Handle error
        return;
    }

    if (nvigi::resultIsError(nvigiShutdown()))
    {
        // Handle error
        return;
    }
}
```

Next, `D3D12HelloTriangle::IGIGPTInit` will:
- Create a instance of an API that will automatically get the capabilities of the hardware as well as references to the relevant D3D12 data structures,
- Create an instance of a GPT interface,
- Create an instance of a polled inference interface, and
- Load a specific plugin and model pair.

```cpp
void D3D12HelloTriangle::IGIGPTInit()
{
    nvigi::D3D12Parameters cigParameters{};
    cigParameters = CIGCompatibilityChecker::init(nvigiLoadInterface, nvigiUnloadInterface);

    if (nvigi::resultIsError(nvigiGetInterfaceDynamic(NVIGI_PLUGIN_ID, &m_IGIGPTInterface, nvigiLoadInterface)))
    {
        // Handle error
        return;
    }

    if (nvigi::resultIsError(nvigiGetInterfaceDynamic(NVIGI_PLUGIN_ID, &m_IGIPolledInterface, nvigiLoadInterface)))
    {
        // Handle error
        return;
    }

    nvigi::GPTCreationParameters gptCreationParams{};
    gptCreationParams.contextSize = 4096;
    if (cigParameters.queue || cigParameters.device)
    {
        gptCreationParams.chain(cigParameters);
    }

    nvigi::CommonCreationParameters commonCreationParams{};
    commonCreationParams.utf8PathToModels = NVIGI_PATH_TO_MODELS;
    commonCreationParams.numThreads = 4u;
    commonCreationParams.vramBudgetMB = 8192u;
    commonCreationParams.modelGUID = NVIGI_MODEL_GUID;

    if (nvigi::resultIsError(commonCreationParams.chain(gptCreationParams)))
    {
        // Handle error
        return;
    }

    nvigi::CommonCapabilitiesAndRequirements* caps{};
    if (nvigi::resultIsError(nvigi::getCapsAndRequirements(m_IGIGPTInterface, commonCreationParams, &caps)))
    {
        // Handle error
        return;
    }

    //! We provided model GUID and VRAM budget so caps and requirements will contain just one model, assuming VRAM budget is sufficient or if cloud backend is selected!
    //! 
    //! NOTE: This will be >=1 if we provide null as modelGUID in common creation parameters
    if (caps->numSupportedModels != 1)
    {
        return;
    }

    if (nvigi::resultIsError(m_IGIGPTInterface->createInstance(commonCreationParams, &m_IGIGPTInstance)))
    {
        // Handle error
        return;
    }
}
```

The GPT interface is an abstration of the type of plugin we want to use (e.g. a general-purpose transformer AI inference plugin), while the specific plugin refers to the implementation of such interface inside a DLL. The specifics of the implementation vary in terms of the technology used (i.e. GGML, cloud, a CPU implementation, etc.) as well as the specific backend for such implementation(such as GGML having CUDA, D3D12, Vulkan and CPU implementation backends).

The polled inference interface helps with asynchronous handling of messages between the host 3D application and the GPT inference plugin (i.e. sending input to the plugin, receiving tokens back, etc.) The alternative would be to use a blocking call in a separate thread that waits for the token to arrive, but this latter approach is less convenient when integrating into a 3D interactive application.

Likewise, `D3D12HelloTriangle::IGIGPTRelease` will dispose of the data structures created in `D3D12HelloTriangle::IGIGPTInit`.
```cpp
void D3D12HelloTriangle::IGIGPTRelease()
{
    if (nvigi::resultIsError(m_IGIGPTInterface->destroyInstance(m_IGIGPTInstance)))
    {
        // Handle error
        return;
    }

    if (nvigi::resultIsError(nvigiUnloadInterface(NVIGI_PLUGIN_ID, m_IGIPolledInterface)))
    {
        // Handle error
        return;
    }

    if (nvigi::resultIsError(nvigiUnloadInterface(NVIGI_PLUGIN_ID, m_IGIGPTInterface)))
    {
        // Handle error
        return;
    }
}
```

The `D3D12HelloTriangle::IGIGPTStartInference` implementation is as follows:
```cpp
void D3D12HelloTriangle::IGIGPTStartInference()
{
    nvigi::InferenceDataTextSTLHelper systemPrompt(NVIGI_SYSTEM_PROMPT);
    nvigi::InferenceDataTextSTLHelper userPrompt(NVIGI_USER_PROMPT);

    nvigi::InferenceDataSlot inputSlots[] = {
        { nvigi::kGPTDataSlotUser, userPrompt },
        { nvigi::kGPTDataSlotSystem, systemPrompt }
    };
    nvigi::InferenceDataSlotArray inputs = { 2, inputSlots };

    nvigi::GPTRuntimeParameters runtime{};
    runtime.seed = -1;
    runtime.tokensToPredict = 200;
    runtime.interactive = false;
    runtime.reversePrompt = "User: ";

    m_IGIGPTExecCtx = new nvigi::InferenceExecutionContext{};
    m_IGIGPTExecCtx->instance = m_IGIGPTInstance;
    m_IGIGPTExecCtx->inputs = &inputs;
    m_IGIGPTExecCtx->outputs = nullptr;  // Plugin allocates outputs
    m_IGIGPTExecCtx->callback = nullptr; // Not using callback, but rather polling for results, so set to null
    m_IGIGPTExecCtx->runtimeParameters = runtime;

    if (nvigi::resultIsError(m_IGIGPTInstance->evaluateAsync(m_IGIGPTExecCtx)))
    {
        // Handle error
        return;
    }

    m_inferenceRunning.store(true);
}
```

In summary, this function:
- Prepares the input text to the GPT model,
- Connects the input data to the appropriate input slots for the plugin interface,
- Creates an `nvigi::InferenceExecutionContext` instance with the parameters for inference execution, and
- Kicks off asynchronous inference

Finally, the implementation of `D3D12HelloTriangle::IGIGPTUpdateInference`:
```cpp
void D3D12HelloTriangle::IGIGPTUpdateInference()
{
    nvigi::InferenceExecutionState state{ nvigi::kInferenceExecutionStateDataPending };

    auto result = m_IGIPolledInterface->getResults(m_IGIGPTExecCtx, true, &state);
    if (result == nvigi::kResultOk)
    {
        const nvigi::InferenceDataText* text{};
        m_IGIGPTExecCtx->outputs->findAndValidateSlot(nvigi::kGPTDataSlotResponse, &text);

        m_cummulativeResponse += std::string(text->getUTF8Text());

        if (state == nvigi::kInferenceExecutionStateDone || state == nvigi::kInferenceExecutionStateCancel)
        {
            std::cout << "NVIGI GPT response:\t\"" << m_cummulativeResponse << "\"" << std::endl;
            m_cummulativeResponse = "";
            m_inferenceRunning.store(false);
        }

        if (nvigi::resultIsError(m_IGIPolledInterface->releaseResults(m_IGIGPTExecCtx, state)))
        {
            // Handle error
            return;
        }

        if (state == nvigi::kInferenceExecutionStateDone || state == nvigi::kInferenceExecutionStateCancel)
        {
            delete m_IGIGPTExecCtx;
        }
    }
    else if (result == nvigi::kResultNotReady)
    {
        // Nothing to do, just return and wait for the next update tick to check again
    }
    else if (result == nvigi::kResultTimedOut)
    {
        // Handle error
        return;
    }
}
```

This method will use the polled inference interface to check if there are any incoming tokens and, if so, it will:
- Retrieve the tokens (if any are available),
- Convert them from raw bytes into a C++ string,
- Append it to the cummulative response member variable,
- Check if this is the last token; if so, print the result and reset inference logic so it can start again with the next call to `D3D12HelloTriangle::IGIGPTStartInference`.

## Building and Testing the Integration
Now build the entire solution in the Debug configuration.

Before executing, place a breakpoint over the line with `std::cout` in `D3D12HelloTriangle::IGIGPTUpdateInference`. Unfortunately, the way the D3D12 samples are set up redirects the standard output away from the console, so we cannot see the results printed. The easiest way is to place that breakpoint and inspect the contents of `m_cummulativeResponse` with the Visual C++ debugger to confirm inference was successful.
