# Running on Multiple GPU Systems

This document describes how to select which GPU to use for inference computation on multi-GPU systems.

## Overview

In multi-GPU configurations, you can run inference on a different GPU than the one used for graphics rendering. This is particularly useful when:
- You want to dedicate a GPU to graphics for maximum FPS
- You want to dedicate a GPU to AI inference for maximum responsiveness
- You have an older GPU that still has sufficient capability for AI workloads

For instructions on running inference and graphics on the same GPU with optimized scheduling, please refer to [GpuSchedulingForAI.md](GpuSchedulingForAI.md).

## Supported Plugins

The following plugins currently support GPU selection:

### CUDA Plugins
* Automatic Speech Recognition (`nvigi::plugin::asr::ggml::cuda`)
* Embed (`nvigi::plugin::embed::ggml::cuda`)
* Generative Pre-Trained Transformer (`nvigi::plugin::gpt::ggml::cuda`)
* Riva Magpie Flow / ASqFlow Text to Speech (`nvigi::plugin::tts::asqflow_trt`, `nvigi::plugin::tts::asqflow_ggml`)

### D3D12 Plugins
* Automatic Speech Recognition (`nvigi::plugin::asr::ggml::d3d12`)
* Generative Pre-Trained Transformer (`nvigi::plugin::gpt::ggml::d3d12`)
* ASqFlow Text to Speech (`nvigi::plugin::tts::asqflow_ggml::d3d12`)

### Vulkan Plugins
* Automatic Speech Recognition (`nvigi::plugin::asr::ggml::vulkan`)
* Generative Pre-Trained Transformer (`nvigi::plugin::gpt::ggml::vulkan`)
* ASqFlow Text to Speech (`nvigi::plugin::tts::asqflow_ggml::vulkan`)

## GPU Enumeration and Device Matching

### The Challenge: Different APIs Enumerate GPUs Differently

An important consideration when working with multiple GPUs is that **each API (CUDA, D3D12, Vulkan) is free to enumerate devices in any order**. For example:
- GPU with CUDA ordinal 1 might be D3D12 adapter 0
- GPU with Vulkan physical device index 0 might be CUDA device 1

This means you cannot simply assume that device index 0 in CUDA corresponds to device 0 in D3D12 or Vulkan.

### Best Practice: Use a Single API for GPU Selection

To avoid confusion, we recommend picking a single API as your "standard" for GPU enumeration and selection. Typically, **using your graphics API as the standard makes the most sense** since that's what users will see in their graphics settings.

Once you've selected a GPU using your standard API, you can convert to the device ordinal for other APIs using LUID (Locally Unique Identifier) or UUID matching.

### D3D12 to CUDA Device Matching (Using LUID)

When you have a D3D12 device and need to find the corresponding CUDA device, match using the adapter LUID:

```cpp
#include <d3d12.h>
#include <cuda_runtime.h>

// Given: A D3D12 device
ID3D12Device* d3d12Device = /* your D3D12 device */;

// Get the D3D12 adapter LUID
LUID d3d12Luid = d3d12Device->GetAdapterLuid();

// Find the matching CUDA device
int matchingCudaDevice = -1;
int numCudaDevices = 0;
cudaGetDeviceCount(&numCudaDevices);

for (int cudaDeviceId = 0; cudaDeviceId < numCudaDevices; cudaDeviceId++) {
    cudaDeviceProp devProp;
    cudaGetDeviceProperties(&devProp, cudaDeviceId);
    
    // Compare the LUID (8 bytes: 4 bytes LowPart + 4 bytes HighPart)
    if ((memcmp(&d3d12Luid.LowPart, devProp.luid, sizeof(d3d12Luid.LowPart)) == 0) &&
        (memcmp(&d3d12Luid.HighPart, devProp.luid + sizeof(d3d12Luid.LowPart), 
                sizeof(d3d12Luid.HighPart)) == 0)) {
        matchingCudaDevice = cudaDeviceId;
        break;
    }
}

if (matchingCudaDevice == -1) {
    // Handle error: Could not find matching CUDA device
}

// Now use matchingCudaDevice for CudaParameters.device
```

### Vulkan to CUDA Device Matching (Using UUID)

When you have a Vulkan physical device and need to find the corresponding CUDA device, match using the device UUID:

```cpp
#include <vulkan/vulkan.h>
#include <cuda_runtime.h>

// Given: A Vulkan physical device
VkPhysicalDevice vkPhysicalDevice = /* your Vulkan physical device */;
VkInstance vkInstance = /* your Vulkan instance */;

// Get the Vulkan device UUID
VkPhysicalDeviceIDProperties deviceIDProps = {};
deviceIDProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

VkPhysicalDeviceProperties2 physicalDeviceProps2 = {};
physicalDeviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
physicalDeviceProps2.pNext = &deviceIDProps;

vkGetPhysicalDeviceProperties2(vkPhysicalDevice, &physicalDeviceProps2);

// Find the matching CUDA device by UUID
int matchingCudaDevice = -1;
int numCudaDevices = 0;
cudaGetDeviceCount(&numCudaDevices);

for (int cudaDeviceId = 0; cudaDeviceId < numCudaDevices; cudaDeviceId++) {
    cudaDeviceProp devProp;
    cudaGetDeviceProperties(&devProp, cudaDeviceId);
    
    // Compare device UUIDs (16 bytes)
    if (memcmp(devProp.uuid.bytes, deviceIDProps.deviceUUID, VK_UUID_SIZE) == 0) {
        matchingCudaDevice = cudaDeviceId;
        break;
    }
}

if (matchingCudaDevice == -1) {
    // Handle error: Could not find matching CUDA device
}

// Now use matchingCudaDevice for CudaParameters.device
```

### CUDA to D3D12 Device Matching (Using LUID)

If you need to go the other direction (CUDA device to D3D12 adapter):

```cpp
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cuda_runtime.h>

// Given: A CUDA device ordinal
int cudaDeviceOrdinal = 1;  // For example, CUDA device 1

// Get CUDA device properties including LUID
cudaDeviceProp devProp;
cudaGetDeviceProperties(&devProp, cudaDeviceOrdinal);

// Create DXGI factory
IDXGIFactory4* factory = nullptr;
CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

// Find matching D3D12 adapter
IDXGIAdapter1* matchingAdapter = nullptr;
UINT adapterIndex = 0;

while (factory->EnumAdapters1(adapterIndex, &matchingAdapter) != DXGI_ERROR_NOT_FOUND) {
    DXGI_ADAPTER_DESC1 desc;
    matchingAdapter->GetDesc1(&desc);
    
    // Compare the LUID
    if ((memcmp(&desc.AdapterLuid.LowPart, devProp.luid, sizeof(desc.AdapterLuid.LowPart)) == 0) &&
        (memcmp(&desc.AdapterLuid.HighPart, devProp.luid + sizeof(desc.AdapterLuid.LowPart),
                sizeof(desc.AdapterLuid.HighPart)) == 0)) {
        // Found matching adapter
        break;
    }
    
    matchingAdapter->Release();
    matchingAdapter = nullptr;
    adapterIndex++;
}

if (matchingAdapter) {
    // Create D3D12 device on this adapter
    ID3D12Device* device = nullptr;
    D3D12CreateDevice(matchingAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    // Use device...
}
```

## Instructions for Device Selection

### 1. CUDA Inference on Different GPU from Graphics

When using CUDA-based inference plugins, you can specify which GPU to use by providing a `CudaParameters` struct and setting the `device` field.

#### Example: D3D12 or Vulkan Graphics on GPU 0, CUDA Inference on GPU 1

```cpp
#include <cuda.h>

// Initialize CUDA
cuInit(0);

// Get the second CUDA device (GPU 1)
CUdevice cudaDevice1;
cuDeviceGet(&cudaDevice1, 1);  // 1 = second GPU

// Create CUDA parameters for GPU 1
nvigi::CudaParameters cudaParams{};
cudaParams.device = cudaDevice1;

// Create inference instance parameters
nvigi::GPTCreationParameters gptParams{};
gptParams.common = &commonParams;
gptParams.common->numThreads = 4;
gptParams.common->vramBudgetMB = 8192;

if(NVIGI_FAILED(gptParams.chain(cudaParams)))
{
    // Handle error
}

// Create the GPT instance - it will run inference on GPU 1
nvigi::IGPT* igpt = nullptr;
nvigiGetInterface(nvigi::plugin::gpt::ggml::cuda::kId, &igpt);
igpt->createInstance(gptParams, &gptInstance);
```

### 2. D3D12 Inference on Different GPU from Graphics

For D3D12-based inference plugins, you select the GPU by creating a D3D12 device on the desired GPU and passing it in the `D3D12Parameters` struct.

#### Example: D3D12 Graphics on GPU 0, D3D12 Inference on GPU 1

```cpp
#include <d3d12.h>
#include <dxgi1_6.h>

// Create factory
IDXGIFactory4* factory = nullptr;
CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));

// Get GPU 0 for graphics
IDXGIAdapter1* adapter0 = nullptr;
factory->EnumAdapters1(0, &adapter0);

// Get GPU 1 for inference
IDXGIAdapter1* adapter1 = nullptr;
factory->EnumAdapters1(1, &adapter1);

// Create D3D12 device on GPU 0 for graphics
ID3D12Device* graphicsDevice = nullptr;
D3D12CreateDevice(adapter0, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&graphicsDevice));

// Create command queue for graphics on GPU 0
D3D12_COMMAND_QUEUE_DESC queueDesc = {};
queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
ID3D12CommandQueue* graphicsQueue = nullptr;
graphicsDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&graphicsQueue));

// Create D3D12 device on GPU 1 for inference
ID3D12Device* inferenceDevice = nullptr;
D3D12CreateDevice(adapter1, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&inferenceDevice));

// Create compute queue for inference on GPU 1
D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
ID3D12CommandQueue* inferenceQueue = nullptr;
inferenceDevice->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&inferenceQueue));

// Set up D3D12 parameters for inference (GPU 1)
nvigi::D3D12Parameters inferenceParams{};
inferenceParams.device = inferenceDevice;
inferenceParams.queueCompute = inferenceQueue;

// Create GPT instance parameters
nvigi::GPTCreationParameters gptParams{};
gptParams.common = &commonParams;
gptParams.common->numThreads = 4;
gptParams.common->vramBudgetMB = 8192;

// Chain the inference parameters (this device will be used for inference)
if(NVIGI_FAILED(gptParams.chain(inferenceParams)))
{
    // Handle error
}

// Create the GPT instance - it will run inference on GPU 1
nvigi::IGPT* igpt = nullptr;
nvigiGetInterface(nvigi::plugin::gpt::ggml::d3d12::kId, &igpt);
igpt->createInstance(gptParams, &gptInstance);
```

### 3. Vulkan Inference on Different GPU from Graphics

For Vulkan-based inference plugins (which use Vulkan compute for inference), GPU selection is done by specifying the appropriate Vulkan physical device and logical device in `VulkanParameters`. 

#### Example: Vulkan Graphics on GPU 0, Vulkan Inference on GPU 1

```cpp
#include <vulkan/vulkan.h>

// Enumerate all physical devices
uint32_t deviceCount = 0;
vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);
std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
vkEnumeratePhysicalDevices(vkInstance, &deviceCount, physicalDevices.data());

// Select GPU 0 for graphics
VkPhysicalDevice graphicsPhysicalDevice = physicalDevices[0];

// Select GPU 1 for inference
VkPhysicalDevice inferencePhysicalDevice = physicalDevices[1];

// Create Vulkan logical device on GPU 0 for graphics
VkDevice graphicsDevice;
// ... (device creation code for graphics)

// Create graphics queue
VkQueue graphicsQueue;
vkGetDeviceQueue(graphicsDevice, graphicsQueueFamilyIndex, 0, &graphicsQueue);

// Create Vulkan logical device on GPU 1 for inference
VkDevice inferenceDevice;
// ... (device creation code for inference, including compute queue family)

// Create compute queue for inference
VkQueue inferenceComputeQueue;
vkGetDeviceQueue(inferenceDevice, computeQueueFamilyIndex, 0, &inferenceComputeQueue);

// Set up Vulkan parameters for inference (GPU 1)
nvigi::VulkanParameters inferenceVulkanParams{};
inferenceVulkanParams.physicalDevice = inferencePhysicalDevice;
inferenceVulkanParams.device = inferenceDevice;
inferenceVulkanParams.instance = vkInstance;
inferenceVulkanParams.queueCompute = inferenceComputeQueue;

// Create TTS instance parameters
nvigi::TTSCreationParameters ttsParams{};
ttsParams.common = &commonParams;
ttsParams.common->numThreads = 4;
ttsParams.common->vramBudgetMB = 4096;

// Chain the Vulkan parameters
if(NVIGI_FAILED(ttsParams.chain(inferenceVulkanParams)))
{
    // Handle error
}

// Create the TTS instance - it will run Vulkan compute inference on GPU 1
nvigi::ITTS* itts = nullptr;
nvigiGetInterface(nvigi::plugin::tts::asqflow_ggml::vulkan::kId, &itts);
itts->createInstance(ttsParams, &ttsInstance);
```

## Important Considerations

### Device Compatibility
- When running inference on a different GPU, ensure that the inference GPU meets the minimum requirements for the plugin you're using.
- CUDA-based plugins require NVIDIA GPUs with the appropriate compute capability.
- Vulkan plugins may work more efficiently when particular Vulkan extensions are available on the inference GPU. See the documentation for each plugin for details.
- Check the plugin requirements using the `getRequirements()` API before creating instances.

### Memory Management
- Each GPU has its own dedicated memory. VRAM budgets specified in creation parameters apply to the inference GPU.
- Data transfers between GPUs may be required depending on your use case, which can add latency.

### CUDA Context Management
- When specifying the GPU to run CUDA inference on, NVIGI will manage the CUDA context for you.
- CUDA-in-Graphics (CiG) optimizations are only available when inference and graphics run on the same GPU.
- If using CiG on a multiple GPU system, note that the inference device is determined by the D3D or Vulkan queue passed in D3dParameters or VulkanParameters, not the device passed in CudaParameters.

### Vulkan Device Management
- NvIgi plugins may require Vulkan extensions, and these must be specified at device creation time. See the documentation for each plugin for details.
- Ensure proper extension selection for compute workloads on the inference device.
- Ensure proper queue family selection for compute workloads on the inference device.

### Performance Considerations
- Running inference on a separate GPU can provide the best of both worlds: maximum graphics FPS and low AI inference latency.
- However, if you need to transfer data between GPUs (e.g., textures, blend weights), consider the overhead of GPU-to-GPU transfers.
- For optimal performance when inference and graphics are on the same GPU, refer to [GpuSchedulingForAI.md](GpuSchedulingForAI.md).

### Driver Requirements
- Some plugins require up-to-date NVIDIA drivers for correct Multi-GPU operation.
- Ensure you have the minimum driver version listed in [GpuSchedulingForAI.md](GpuSchedulingForAI.md).


