# GPU Scheduling for AI

Games typically optimize graphics to maximize GPU utilization, so when you add AI compute to a game it is essential to configure the GPU scheduler to minimize the effect on the game's frame rate. This document describes how to do this.

## Supported GPUs
Optimized GPU scheduling for AI has been tested on the following GPUs.
* RTX 4080
* RTX 4090 
* RTX 5080
* RTX 5090

Note that optimized GPU scheduling is only tested on the AI models that ship with NvIGI. Other models may exercise codepaths that are not currently compatible with optimized GPU scheduling. General support is deferred to a later release.

## Supported plugins
Optimized GPU scheduling is currently utilized by the following CUDA plugins.
* `plugins/sdk` in current SDK
  * Automatic Speech Recognition (nvigi::plugin::asr::ggml::cuda)
  * Embed (nvigi::plugin::embed::ggml::cuda)
  * Generative Pre-Trained Transformer (nvigi::plugin::gpt::ggml::cuda)
* ACE SDK plugins - not available in all packs
  * Audio To Emotion (nvigi::plugin::a2e::trt::cuda)
  * Audio To Face (nvigi::plugin::a2f::trt::cuda)
  * Audio To X Pipeline (nvigi::plugin::a2x::pipeline)

## Prerequisites
NVIGI's GPU scheduling uses CUDA in Graphics (CIG) which has the following requirements.
* NVIDIA display driver 555.85 or higher.
* Hardware scheduling must be enabled in Windows 10 and 11. Windows 11 has hardware scheduling enabled by default.

## A note on performance measurement
It is important not to use GPU begin/end event queries (such as D3D12_QUERY_TYPE_TIMESTAMP or CUDA events) to measure the cost of AI features. The reason is that such measurements do not take into account how much graphics work is running in parallel with the AI compute during the measured interval. For example, if AI is using 10% of the execution units of the GPU for 1ms, and graphics is running in parallel using the other 90%, then queries would report the AI cost as 1ms, which is not correct. Instead its better to add a way to toggle the AI feature (for example a command line parameter), run a benchmark twice with the feature enabled and disabled, and report the difference in total frame time.

## How to use

In order to schedule graphics and compute efficiently, NVIGI needs to know the D3D direct queue that your game is using for graphics. To do this, fill in a D3D12Parameters struct and chain it to the parameters of all NVIGI instances that you create. The following example shows how to do this for the ASR plugin. 

    nvigi::ASRCreationParameters asrParams{};
    asrParams.common = &common;
    asrParams.common->numThreads = myNumCPUThreads;
    asrParams.common->vramBudgetMB = myVRAMBudget;
    <etc>
    
    D3D12Parameters d3d12Parameters{};
    d3d12Parameters.queue = myGraphicsQueue;

	asrParams.chain(d3d12Parameters); // Tell ASR to run in parallel with
                                      // graphics

	iasr->createInstance(asrParams, &asrInstance);

This ensures that compute work submitted by NVIGI will run efficiently with graphics running in parallel. 

## CiG and D3D Wrappers (e.g. Streamline)

Care should be taken when integrating NVIGI into an existing application that is also using a D3D object wrapper like Streamline.  The queue/device parameters passed to NVIGI must be the **native** objects, not the app-level wrappers.  In the case of Streamline, this means using `slGetNativeInterface` to retrieve the base interface object before passing it to NVIGI.

### Limitations
Note that CUDA Compute In Graphics contexts are not compatible with compute-only CUDA contexts. In particular, accessing CUDA pointers outside of the CUDA context that created them may cause CUDA API errors, which if not handled can causes crashes. So using multiple CUDA contexts in a game is not recommended. CUDA virtual memory is also not supported.

## UnrealEngine 5 example code

The following example shows how to initialize NVIGI's GPU scheduling in UnrealEngine5 (UE5). It uses UE5's global dynamic RHI to get the D3D device and command queue that the game uses for graphics.

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


