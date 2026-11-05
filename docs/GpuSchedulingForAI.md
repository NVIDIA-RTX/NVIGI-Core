# GPU Scheduling for AI

Games typically optimize graphics to maximize GPU utilization, so when you add AI compute to a game it is essential to configure the GPU scheduler to minimize the effect on the game's frame rate. This document describes how to do this.

## Supported Plugins
Optimized GPU scheduling is currently utilized by the following plugins.

### CUDA

  * Automatic Speech Recognition (`nvigi::plugin::asr::ggml::cuda`)
  * Embed (`nvigi::plugin::embed::ggml::cuda`)
  * Generative Pre-Trained Transformer (`nvigi::plugin::gpt::ggml::cuda`)
  * Stable Diffusion (`nvigi::plugin::sd::ggml::cuda`)
  * Chatterbox Text to Speech (`nvigi::plugin::tts::chatterbox::ggml::cuda`)

### D3D12

  * Automatic Speech Recognition (`nvigi::plugin::asr::ggml::d3d12`)
  * Generative Pre-Trained Transformer (`nvigi::plugin::gpt::ggml::d3d12`)
  * Stable Diffusion (`nvigi::plugin::sd::ggml::d3d12`)
  * Chatterbox Text to Speech (`nvigi::plugin::tts::chatterbox::ggml::d3d12`)

### Vulkan

  * Automatic Speech Recognition (`nvigi::plugin::asr::ggml::vulkan`)
  * Generative Pre-Trained Transformer (`nvigi::plugin::gpt::ggml::vulkan`)
  * Stable Diffusion (`nvigi::plugin::sd::ggml::vulkan`)
  * Chatterbox Text to Speech (`nvigi::plugin::tts::chatterbox::ggml::vulkan`)

## Supported Graphics APIs
Where optimized GPU scheduling is available, inference compute and graphics workloads may run in parallel on the GPU. NVIGI's GPU scheduling uses D3D12 async compute for D3D12 plugins, Vulkan async compute for Vulkan plugins, and CUDA in Graphics (CIG) for CUDA plugins. It also takes advantage of a driver interface for setting relative priorities of CUDA vs D3D12 graphics workloads and D3D12 compute vs D3D12 graphics workloads. Vulkan plugins use `VK_NV_compute_occupancy_priority` to set the relative priority of Vulkan inference compute and Vulkan graphics.
Optimized GPU scheduling support depends on both the compute API used by the NVIGI plugin, and the Graphics API used by the application. The following table shows the currently supported combinations. In parenthesis is the minimum driver required to get full functionality, including settable priorities and correct interaction with other libraries such as NVIDIA DLFG.

Note that the driver versions specified below are the minimum supported. For best scheduling performance **620.00 or higher is recommended for all configurations.**
<table style="break-inside: avoid; page-break-inside: avoid; border-collapse: collapse; border: 1px solid #888;">
  <tr>
    <th rowspan="2" style="border: 1px solid #888;">Plugin's compute API</th>
    <th colspan="2" style="border: 1px solid #888;">App's graphics API</th>
  </tr>
  <tr>
    <th style="border: 1px solid #888;">D3D12 Graphics</th>
    <th style="border: 1px solid #888;">Vulkan Graphics</th>
  </tr>
  <tr>
    <td style="border: 1px solid #888;">CUDA</td>
    <td style="border: 1px solid #888;">Supported (575.00)<sup>*</sup></td>
    <td style="border: 1px solid #888;">Supported (580.88)</td>
  </tr>
  <tr>
    <td style="border: 1px solid #888;">D3D12 Compute</td>
    <td style="border: 1px solid #888;">Supported (580.88)</td>
    <td style="border: 1px solid #888;">Not supported</td>
  </tr>
  <tr>
    <td style="border: 1px solid #888;">Vulkan Compute</td>
    <td style="border: 1px solid #888;">Not supported</td>
    <td style="border: 1px solid #888;">Supported (595.71)</td>
  </tr>
</table>

(*) CUDA requires that Hardware scheduling be enabled in Windows 11. Windows 11 has hardware scheduling enabled by default.


## A Note on Performance Measurement
It is important not to use GPU begin/end event queries (such as D3D12_QUERY_TYPE_TIMESTAMP or CUDA events) to measure the cost of AI features. The reason is that such measurements do not take into account how much graphics work is running in parallel with the AI compute during the measured interval. For example, if AI is using 10% of the execution units of the GPU for 1ms, and graphics is running in parallel using the other 90%, then queries would report the AI cost as 1ms, which is not correct. Instead it's better to add a way to toggle the AI feature (for example a command line parameter), run a benchmark twice with the feature enabled and disabled, and report the difference in total frame time.

## How to Use

### D3D12 graphics with D3D12 or CUDA inference

In order to schedule D3D12 graphics with either CUDA or D3D12 inference, NVIGI needs the native D3D12 device and direct queue that your game uses for graphics. Fill in a `nvigi::D3D12Parameters` struct and chain it to the creation parameters of each NVIGI instance. The following example uses the ASR plugin; `common` contains the model and resource settings.

```cpp
nvigi::ASRWhisperCreationParameters asrParams{};
asrParams.language = "en";
asrParams.common = &common;

nvigi::D3D12Parameters d3d12Parameters{};
d3d12Parameters.device = myD3D12Device;
d3d12Parameters.queue = myGraphicsQueue;

// Tell ASR to run in parallel with graphics.
if (NVIGI_FAILED(result, asrParams.chain(d3d12Parameters)))
{
    // Handle the chaining error.
}
else if (NVIGI_FAILED(result, iasr->createInstance(asrParams, &asrInstance)))
{
    // Handle the instance creation error.
}
```

Keep the supplied native D3D12 device and queues alive until all NVIGI instances that use them have been destroyed and their outstanding GPU work has completed. NVIGI retains these raw pointers after instance creation; releasing the objects earlier can cause use-after-free errors.

### Vulkan graphics

#### Vulkan graphics with Vulkan inference

For Vulkan inference with Vulkan graphics, pass the application's Vulkan handles through `nvigi::VulkanParameters`. Chain this struct to the creation parameters of each Vulkan inference instance so NVIGI uses the same device as the graphics workload.

The NVIDIA device must support `VK_NV_compute_occupancy_priority`. When creating the application's logical device, add `VK_NV_COMPUTE_OCCUPANCY_PRIORITY_EXTENSION_NAME` to the enabled device extensions and chain a `VkPhysicalDeviceComputeOccupancyPriorityFeaturesNV` struct with `computeOccupancyPriority = VK_TRUE` into the `VkDeviceCreateInfo::pNext` chain. Check feature support before enabling it, and preserve the application's other extensions and feature structs. Chaining `VulkanParameters` to the inference parameters does not enable this device feature.

The following example assumes a Vulkan 1.1 or newer instance, `availableDeviceExtensions` populated by `vkEnumerateDeviceExtensionProperties`, and `deviceCreateInfo` already configured with the application's queues and other features. It uses `<algorithm>`, `<cstring>`, `<stdexcept>`, `<vector>`, and the Vulkan headers. For a Vulkan 1.0 instance, query features with `vkGetPhysicalDeviceFeatures2KHR` only when `VK_KHR_get_physical_device_properties2` is enabled.

```cpp
const char* extensionName = VK_NV_COMPUTE_OCCUPANCY_PRIORITY_EXTENSION_NAME;
const bool extensionSupported = std::any_of(
    availableDeviceExtensions.begin(), availableDeviceExtensions.end(),
    [extensionName](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, extensionName) == 0;
    });
if (!extensionSupported)
    throw std::runtime_error("Vulkan compute occupancy priority is unavailable");

VkPhysicalDeviceComputeOccupancyPriorityFeaturesNV occupancyPriority{};
occupancyPriority.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_OCCUPANCY_PRIORITY_FEATURES_NV;

VkPhysicalDeviceFeatures2 supportedFeatures{};
supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
supportedFeatures.pNext = &occupancyPriority;
vkGetPhysicalDeviceFeatures2(myVulkanPhysicalDevice, &supportedFeatures);
if (!occupancyPriority.computeOccupancyPriority)
    throw std::runtime_error("Vulkan compute occupancy priority feature is unavailable");

// Preserve the application's existing device extensions and avoid duplicates.
std::vector<const char*> enabledExtensions;
for (uint32_t i = 0; i < deviceCreateInfo.enabledExtensionCount; ++i)
    enabledExtensions.push_back(deviceCreateInfo.ppEnabledExtensionNames[i]);
if (std::none_of(enabledExtensions.begin(), enabledExtensions.end(),
                 [extensionName](const char* name) {
                     return std::strcmp(name, extensionName) == 0;
                 }))
    enabledExtensions.push_back(extensionName);
deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();

// Enable the feature while preserving the existing device creation chain.
occupancyPriority.computeOccupancyPriority = VK_TRUE;
occupancyPriority.pNext = const_cast<void*>(deviceCreateInfo.pNext);
deviceCreateInfo.pNext = &occupancyPriority;

VkResult result = vkCreateDevice(myVulkanPhysicalDevice, &deviceCreateInfo,
                                nullptr, &myVulkanDevice);
if (result != VK_SUCCESS)
    throw std::runtime_error("Vulkan device creation failed");
```

Keep `occupancyPriority` and `enabledExtensions` alive until `vkCreateDevice` returns.

After creating the device and obtaining its queues, set and chain the parameters as follows. Load the Vulkan ASR plugin (`nvigi::plugin::asr::ggml::vulkan`) for this example.

```cpp
nvigi::ASRWhisperCreationParameters asrParams{};
asrParams.language = "en";
asrParams.common = &common;

nvigi::VulkanParameters vulkanParameters{};
vulkanParameters.instance = myVulkanInstance;
vulkanParameters.physicalDevice = myVulkanPhysicalDevice;
vulkanParameters.device = myVulkanDevice;
vulkanParameters.queue = myGraphicsQueue; // VK_QUEUE_GRAPHICS_BIT

// Optional dedicated queues from the same device; leave null if unavailable.
vulkanParameters.queueCompute = myComputeQueue;   // Compute, without graphics
vulkanParameters.queueTransfer = myTransferQueue; // Transfer, without compute or graphics

// Tell ASR to use the application's Vulkan device and queues.
if (NVIGI_FAILED(result, asrParams.chain(vulkanParameters)))
{
    // Handle the chaining error.
}
else if (NVIGI_FAILED(result, iasr->createInstance(asrParams, &asrInstance)))
{
    // Handle the instance creation error.
}
```

Keep the supplied Vulkan instance, device, and queues alive while the inference instance uses them. The scheduling mode is set through `IHWICommon`, as described below; supported Vulkan plugins apply it before recording inference dispatches.

#### Vulkan graphics with CUDA inference

For CUDA inference with Vulkan graphics, load a supported CUDA plugin (for example, `nvigi::plugin::asr::ggml::cuda`) and run inference on the same NVIDIA GPU as the graphics workload. Use driver 580.88 or newer and enable Windows hardware-accelerated GPU scheduling, as required for the supported CUDA configuration above.

Set the desired mode with `IHWICommon::SetGpuInferenceSchedulingMode`: `kPrioritizeCompute`, `kBalance`, or `kPrioritizeGraphics`. Supported CUDA plugins apply the selected mode to their inference streams.

Unlike Vulkan graphics with Vulkan inference, these three priority levels do not require `VK_NV_external_compute_queue`, `VK_NV_compute_occupancy_priority`, or Vulkan device feature structs. No Vulkan parameter chain is needed to set the CUDA priority. 

## How to Set the Relative Priority of Compute and Graphics
In the past GPU compute workloads in games were often directly coupled to graphics frame generation, for example raytracing and animation. In contrast, NVIGI pipelines, consisting of automatic speech recognition (ASR), language models (LM), text to speech (TTS) etc. can run asynchronously to frame generation. Their latency requirements are dictated not by human visual perception (measured in milliseconds), but how fast humans listen, think and speak (measured in hundreds of milliseconds). Each invocation of the pipeline (for example to process a question) can run across many graphics frames. For this reason the workload can be thought of as floating relative to the game’s graphics and compute workload.

For past compute workloads coupled to graphics generation, the compute work was often on the critical path of the graphics frame, and so the hardware executed compute with high priority to try to maximize graphics frame rate.

As NVIGI pipelines run independently to graphics generation we now have two variables we might want to optimize, graphics frame rate, and AI inference latency (and throughput). As the rate at which an unloaded GPU can produce AI tokens can be many times higher than a human's ability to listen to and understand them, it often doesn't make sense to run AI at maximum rate at the expense of graphics frame rate.

However, different game situations have different needs, and they might change at runtime. For example, a player might want snappy interaction with an NPC between quests (where graphics load might be light), but maximum FPS during gameplay. For this reason, NVIGI exposes the following three GPU scheduling modes:

* `SchedulingMode::kPrioritizeGraphics` - try to maximize game FPS
* `SchedulingMode::kPrioritizeCompute` - try to minimize AI inference latency
* `SchedulingMode::kBalance` - balance scheduling between graphics and compute

The default scheduling mode in NVIGI is kBalance. The default in normal CUDA usage is kPrioritizeCompute. Note that these modes are hints to the GPU scheduling hardware. Results may vary according to workload composition and GPU type.


### API
You can set the priority at any time using SetGpuInferenceSchedulingMode(). For supported CUDA plugins, evaluate() applies the priority to the CUDA kernels it launches, and those kernels may execute after evaluate() returns. Supported D3D12 and Vulkan plugins apply the current mode while recording inference dispatches. The following example shows a game that has two phases, the first is before gameplay starts, and prioritizes NPC responsiveness, the second prioritizes FPS during gameplay.
    
    nvigi::IHWICommon* ihwiCommon{};
    nvigiGetInterfaceDynamic(plugin::hwi::common::kId, &ihwiCommon,nvigiLoadInterface);

    ihwiCommon->SetGpuInferenceSchedulingMode(SchedulingMode::kPrioritizeCompute);
    // evaluate() calls for pre-quest NPC interaction go here

    ihwiCommon->SetGpuInferenceSchedulingMode(SchedulingMode::kPrioritizeGraphics);
    // evaluate() calls for high FPS quest gameplay go here

## CIG and D3D Wrappers (e.g. Streamline)

Care should be taken when integrating NVIGI into an existing application that is also using a D3D object wrapper like Streamline.  The queue/device parameters passed to NVIGI must be the **native** objects, not the app-level wrappers.  In the case of Streamline, this means using `slGetNativeInterface` to retrieve the base interface object before passing it to NVIGI.

### Limitations
Note that CUDA Compute In Graphics contexts are not compatible with compute-only CUDA contexts. In particular, accessing CUDA pointers outside of the CUDA context that created them may cause CUDA API errors, which if not handled can cause crashes. So using multiple CUDA contexts in a game is not recommended. CUDA virtual memory and cudaMallocAsync are also not supported.

## UnrealEngine 5 example code

The following example shows how to initialize NVIGI's GPU scheduling in UnrealEngine5 (UE5). It uses UE5's global dynamic RHI to get the D3D12 device and command queue that the game uses for graphics.

    // UE5 specific code
    #include "ID3D12DynamicRHI.h" // For GDynamicRHI
    ID3D12DynamicRHI* RHI = nullptr;
    if (GDynamicRHI && 
        GDynamicRHI->GetInterfaceType()==ERHIInterfaceType::D3D12)
    {
        RHI = static_cast<ID3D12DynamicRHI*>(GDynamicRHI);
    }
    ID3D12CommandQueue* CmdQ = nullptr;
    ID3D12Device* D3D12Device = nullptr;
    if (RHI)
    {
        CmdQ = RHI->RHIGetCommandQueue();
        int DeviceIndex = 0;
        D3D12Device = RHI->RHIGetDevice(DeviceIndex);
    }
    // Engine-independent code
    nvigi::D3D12Parameters d3d12Params;
    d3d12Params.device = D3D12Device;
    d3d12Params.queue = CmdQ;

    // Code to chain the d3d12Parameters to each NVIGI instance's parameters goes here


