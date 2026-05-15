// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Modern AI Plugin Template - Implementation
// ==========================================
// This file demonstrates how to implement an AI inference plugin using the
// modern C++ plugin_base_ai.hpp framework. The framework handles all the
// boilerplate code for async execution, cancellation, result polling, etc.
//
// Key features:
// - Type-safe parameter access
// - Automatic async/sync execution handling
// - Built-in cancellation support
// - Ergonomic input/output handling
// - Platform-specific GPU context management (CUDA/D3D12/Vulkan)
//

// ============================================================================
// Platform-Specific Includes
// ============================================================================
// Configure these defines in your premake.lua based on your platform needs

#if defined(PLUGIN_USES_CUDA)
    // CUDA support - Required for NVIDIA GPU acceleration via CUDA
    #include "source/core/nvigi.api/nvigi_cuda.h"
    #include "source/utils/nvigi.hwi/cuda/push_poppable_cuda_context.h"
    #include "source/utils/nvigi.hwi/cuda/runtime_context_scope.h"
    #include <cuda_runtime.h>
#endif

#if defined(PLUGIN_USES_D3D12)
    // D3D12 support - Required for DirectX 12 GPU acceleration (Windows)
    #include "source/core/nvigi.api/nvigi_d3d12.h"
    #include "source/utils/nvigi.d3d12/d3d12_helpers.h"
#endif

#if defined(PLUGIN_USES_VULKAN)
    // Vulkan support - Cross-platform GPU acceleration
    // 
    // TODO: Add Vulkan-specific includes when available
    #include "source/core/nvigi.api/nvigi_vulkan.h"
#endif

// ============================================================================
// Core Includes
// ============================================================================
#include "nvigi.h"
#include "nvigi_io.h"
#include "nvigi.api/internal.h"
#include "nvigi.log/log.h"
#include "nvigi.exception/exception.h"
#include "nvigi.plugin/plugin.h"
#include "versions.h"
#include "nvigi_template_infer.h"
#include "source/utils/nvigi.ai/ai.h"
#include "_artifacts/gitVersion.h"

// Modern plugin framework - provides all the base functionality
#include "source/plugins/common/plugin_base_ai.hpp"

// Import modern plugin types for convenience
using namespace nvigi::plugin::modern;

namespace nvigi
{

// ============================================================================
// Instance Context
// ============================================================================
// This structure holds per-instance context/state for your plugin.
// Each instance created by createInstance() gets its own InstanceContext.
//
// IMPORTANT: Use std::shared_ptr to store this in PluginContext::pluginData
// to ensure proper lifetime management.
//
struct InstanceContext
{
    // Model configuration
    std::string modelPath;
    json modelConfig;
    bool isInitialized = false;

    // Add your model-specific state here
    // Example:
    // void* model = nullptr;  // Your model handle
    // std::vector<float> embeddings;

#if defined(PLUGIN_USES_CUDA)
    // CUDA context management
    // PushPoppableCudaContext handles automatic context push/pop for CUDA-in-Graphics (CiG)
    // It ensures the correct CUDA context is active when this plugin runs
    PushPoppableCudaContext cudaContext;
    
    // CUDA-specific resources
    // Example: Store CUDA streams for async operations
    // std::vector<cudaStream_t> cudaStreams;
    // void* deviceBuffer = nullptr;
    
    // Constructor that initializes CUDA context from creation parameters
    InstanceContext(const NVIGIParameter* params) : cudaContext(params) {}
#else
    // Dummy CUDA context for non-CUDA builds (allows same interface)
    struct PushPoppableCudaContext {
        bool constructorSucceeded = true;
        PushPoppableCudaContext(const NVIGIParameter*) {}
        void pushRuntimeContext() {}
        void popRuntimeContext() {}
    };
    PushPoppableCudaContext cudaContext;
    
    InstanceContext(const NVIGIParameter* params) : cudaContext(params) {}
    InstanceContext() : cudaContext(nullptr) {}
#endif

#if defined(PLUGIN_USES_D3D12)
    // D3D12-specific resources
    // Example: Store D3D12 command queues, heaps, etc.
    // ID3D12CommandQueue* commandQueue = nullptr;
#endif

#if defined(PLUGIN_USES_VULKAN)
    // Vulkan-specific resources
    // Example: Store Vulkan command buffers, descriptor sets, etc.
    // VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
#endif

    ~InstanceContext() {
        // Clean up your model resources here
        // Example:
        // if (model) {
        //     your_model_free(model);
        //     model = nullptr;
        // }
    }
};

// ============================================================================
// Plugin Implementation
// ============================================================================
// This class implements the modern PluginConcept interface.
// All methods are static - instance state is stored in PluginState.
//
class TemplateAIPlugin
{
public:
    // ========================================================================
    // Plugin Identity
    // ========================================================================

    static PluginID getPluginID()
    {
        return plugin::template_ai::kId;
    }

    static plugin::PluginInfo getPluginInfo()
    {
        plugin::PluginInfo info{};
        info.description = "Modern AI Plugin Template - demonstrating plugin_base_ai.hpp framework";
        info.author = "NVIDIA";
        info.build = GIT_BRANCH_AND_LAST_COMMIT;
        info.interfaces = { plugin::getInterfaceInfo<ITemplateAI>() };

        // Specify minimum requirements for OS, driver, and GPU architecture
        // Leave default (empty) for no restrictions
        info.minOS = {};
        info.minDriver = {};
        info.minGPUArch = {};

#if defined(PLUGIN_USES_CUDA)
        // CUDA plugins typically require specific GPU architectures
        // Example: Require compute capability 7.0+
        // info.minGPUArch = GPUArchitecture::kSM70;
        info.requiredVendor = GPUVendor::kNVIDIA;
#endif

        return info;
    }

    // ========================================================================
    // Input/Output Signatures
    // ========================================================================
    // Define what data your plugin expects as input and produces as output.
    // Use the NVIGI_SIGNATURE macro for clean, declarative definitions.

    static std::span<const InferenceDataDescriptor> getPluginInputSignature()
    {
        // Example: Single text input (prompt)
        return NVIGI_SIGNATURE(
            NVIGI_TEXT_SLOT(kTemplateAIInputPrompt, false)  // required
            // Add more inputs as needed:
            // NVIGI_SCALAR_SLOT("temperature", true),       // optional
            // NVIGI_IMAGE_SLOT("image", true),              // optional
            // NVIGI_TENSOR_SLOT("embeddings", false)        // required
        );
    }

    static std::span<const InferenceDataDescriptor> getPluginOutputSignature()
    {
        // Example: Single text output (response)
        return NVIGI_SIGNATURE(
            NVIGI_TEXT_SLOT(kTemplateAIOutputResponse, false)  // required
            // Add more outputs as needed:
            // NVIGI_SCALAR_SLOT("confidence", false),
            // NVIGI_TENSOR_SLOT("logits", true)
        );
    }

    // ========================================================================
    // Capabilities and Requirements
    // ========================================================================
    // Report what your plugin can do and what it needs to run.

    static Expected<CommonCapabilitiesAndRequirements> 
    getPluginCapsAndRequirements(const NVIGIParameter* params)
    {
        auto common = findStruct<CommonCreationParameters>(params);
        if (!common) {
            return std::unexpected(Error{
                kResultInvalidParameter,
                "Missing CommonCreationParameters"
            });
        }

        CommonCapabilitiesAndRequirements caps{};
        
        // Set which backends your plugin supports
#if defined(PLUGIN_USES_CUDA) || defined(PLUGIN_USES_VULKAN) || defined(PLUGIN_USES_D3D12)
        caps.supportedBackends = InferenceBackendLocations::eGPU | InferenceBackendLocations::eCPU;
#else
        caps.supportedBackends = InferenceBackendLocations::eCPU;
#endif

        // Optional: Populate model information dynamically
        // This queries available models and their requirements
        // auto& ctx = ModernPluginBase<TemplateAIPlugin, ITemplateAI>::getContext();
        // if (!ai::populateCommonCapsAndRequirements(common, {"gguf", "bin"}, caps, ctx.capsData, ctx.modelInfo)) {
        //     return std::unexpected(Error{kResultInvalidParameter, "Failed to populate capabilities"});
        // }

        return caps;
    }

    // ========================================================================
    // Lifecycle Callbacks
    // ========================================================================

    //! Called when plugin is loaded
    //! Do global initialization here (NOT heavy resource allocation)
    static Result onPluginRegister(framework::IFramework* framework)
    {
        NVIGI_LOG_INFO("Template AI plugin registered");

        // Example: Acquire hardware interface for CUDA
#if defined(PLUGIN_USES_CUDA)
        auto& ctx = ModernPluginBase<TemplateAIPlugin, ITemplateAI>::getContext();
        if (!framework::getInterface(plugin::getContext()->framework, plugin::hwi::cuda::kId, &ctx.icig)) {
            NVIGI_LOG_ERROR("Failed to obtain CUDA interface");
            return kResultMissingInterface;
        }
#endif

        // Example: Acquire system interface for D3D12
#if defined(PLUGIN_USES_D3D12)
        auto& ctx = ModernPluginBase<TemplateAIPlugin, ITemplateAI>::getContext();
        if (!framework::getInterface(framework, nvigi::core::framework::kId, &ctx.isystem)) {
            NVIGI_LOG_ERROR("Failed to obtain system interface");
            return kResultMissingInterface;
        }
#endif

        return kResultOk;
    }

    //! Called when plugin is unloaded
    //! Do global cleanup here
    static Result onPluginDeregister()
    {
        NVIGI_LOG_INFO("Template AI plugin deregistered");
        
        // Example: Clean up backend resources if needed
#if defined(PLUGIN_USES_VULKAN) || defined(PLUGIN_USES_D3D12)
        // Some backends need explicit cleanup
        // extern void ggml_vk_backend_free();
        // ggml_vk_backend_free();
#endif

        // Example: Wait for background threads (if using multi-threading)
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return kResultOk;
    }

    //! Called when a new instance is created
    //! Initialize per-instance state here
    static Expected<void> onCreateInstance(
        const NVIGIParameter* params,
        std::any& pluginData)
    {
        NVIGI_LOG_INFO("Creating Template AI plugin instance");

        // Get required common parameters
        auto common = findStruct<CommonCreationParameters>(params);
        if (!common)
        {
            return std::unexpected(Error{
                kResultInvalidParameter,
                "CommonCreationParameters must be provided in parameter chain"
            });
        }

        // Get optional custom parameters
        auto customParams = findStruct<TemplateAICreationParameters>(params);
        if (customParams)
        {
            NVIGI_LOG_VERBOSE("Custom creation parameters provided");
            // Example: Access custom parameters
            // float temperature = customParams->temperature;
            // int maxTokens = customParams->maxTokens;
        }

        // Get extended parameters (if chained via _next)
        auto extendedParams = findStruct<TemplateAICreationParametersEx>(params);
        if (extendedParams)
        {
            NVIGI_LOG_VERBOSE("Extended creation parameters provided");
            // Example: Access extended parameters
            // bool enableOptimization = extendedParams->enableOptimization;
        }

        // Unified model discovery - handles ALL three approaches automatically!
        // Just pass FileIOCallbacks and ai::findModels() does the rest
        auto ioCallbacks = findStruct<FileIOCallbacks>(params);
        auto& ctx = ModernPluginBase<TemplateAIPlugin, ITemplateAI>::getContext();
        
        // Call unified findModels - it detects which approach to use based on parameters
        // - Approach 3: ioCallbacks provided → uses modelCardJSON directly
        // - Approach 2: no callbacks, JSON has local_files → uses those file names
        // - Approach 1: no callbacks, JSON lacks local_files → scans by extension
        json modelInfo;
        {
            // Replace "my_ext1", "my_ext2" with your model's file extensions (e.g., "gguf", "bin", "safetensors", etc.)
            if (!ai::findModels(common, {"my_ext1", "my_ext2"}, modelInfo, ioCallbacks)) {
                return std::unexpected(Error{
                    kResultInvalidParameter,
                    "Failed to find model files - check logs for details"
                });
            }
        }

        // Extract model file paths and card
        // For Approach 3 (callbacks), the key is "ai::kSyntheticKey"
        // For Approach 1 & 2 (filesystem), the key is the GUID
        std::string modelKey = ioCallbacks ? ai::kSyntheticKey : (common->modelGUID ? common->modelGUID : "");
        if(modelKey.empty()) {
            return std::unexpected(Error{
                kResultInvalidParameter,
                "Model GUID must be provided in CommonCreationParameters for filesystem-based discovery"
            });
        }

        std::vector<std::string> files;
        json modelCard;
        
        try {
            // Get model card
            if (!modelInfo.contains(modelKey)) {
                return std::unexpected(Error{
                    kResultInvalidParameter,
                    "Model not found in discovered models"
                });
            }
            
            modelCard = modelInfo[modelKey];
            
            // Get file list (check first extension that has files)
            for (const auto& ext : {"my_ext1", "my_ext2"}) {
                if (modelInfo[modelKey].contains(ext) && 
                    !modelInfo[modelKey][ext].empty()) {
                    files = modelInfo[modelKey][ext];
                    break;
                }
            }
        } catch (const std::exception& e) {
            return std::unexpected(Error{
                kResultJSONException,
                std::string("Model info error: ") + e.what()
            });
        }

        if (files.empty()) {
            return std::unexpected(Error{
                kResultInvalidParameter,
                "No model files found"
            });
        }

        std::string pathToModel = files[0];
        NVIGI_LOG_INFO("Loading model from '%s'", pathToModel.c_str());

        // Create instance context (use shared_ptr for automatic lifetime management)
        // IMPORTANT: Pass params to constructor for CUDA context initialization
        std::unique_ptr<InstanceContext> state;
        
#if defined(PLUGIN_USES_CUDA)
        // Create instance context with CUDA context - constructor initializes PushPoppableCudaContext
        state = std::make_unique<InstanceContext>(params);
        if (!state->cudaContext.constructorSucceeded) {
            return std::unexpected(Error{
                kResultInvalidState,
                "Failed to initialize CUDA context"
            });
        }
#else
        state = std::make_unique<InstanceContext>();
#endif

        state->modelPath = pathToModel;

        // Initialize your model here
        // Replace this with your actual model loading code

#if defined(PLUGIN_USES_CUDA)
        // Example: Set up CUDA device from parameters
        // auto cudaParams = findStruct<CudaParameters>(params);
        // int deviceId = cudaParams ? cudaParams->device : 0;
        // 
        // Example model initialization with CUDA:
        // your_model_context_params modelParams = your_model_default_params();
        // modelParams.use_gpu = true;
        // modelParams.gpu_device = deviceId;
        // state->model = your_model_init_from_file_with_params(pathToModel.c_str(), modelParams);
        // if (!state->model) {
        //     return std::unexpected(Error{kResultInvalidState, "Failed to initialize model"});
        // }
        //
        // Example: Get CUDA streams from model for async operations
        // size_t streamCount = your_model_get_stream_count(state->model);
        // state->cudaStreams.resize(streamCount);
        // your_model_get_streams(state->model, (void**)state->cudaStreams.data(), streamCount);
        // NVIGI_LOG_INFO("Initialized %zu CUDA streams", streamCount);
#else
        // CPU-only model initialization
        // state->model = your_model_init_from_file(pathToModel.c_str());
        // if (!state->model) {
        //     return std::unexpected(Error{kResultInvalidState, "Failed to initialize model"});
        // }
#endif

        // If using IO callbacks, load model files through callbacks instead of standard file I/O
        // This allows host to provide custom file handling (encryption, compression, virtual filesystems, etc.)
        if (ioCallbacks) {
            NVIGI_LOG_INFO("Loading model '%s' using FileIOCallbacks", pathToModel.c_str());
            
            // Example: Load model file(s) using callbacks
            auto handle = ioCallbacks->open(ioCallbacks->userData, pathToModel.c_str(), "rb");
            if (!handle) {
                 return std::unexpected(Error{kResultInvalidParameter, "Failed to open model file via callbacks"});
            }
            size_t size = ioCallbacks->size(ioCallbacks->userData, handle);
            std::vector<char> buffer(size);
            size_t bytesRead = ioCallbacks->read(ioCallbacks->userData, handle, buffer.data(), size);
            ioCallbacks->close(ioCallbacks->userData, handle);
             
            if (bytesRead != size) {
                return std::unexpected(Error{kResultInvalidParameter, "Failed to read complete model file"});
            }
             
            // // Load model from buffer
            // state->model = your_model_init_from_buffer(buffer.data(), buffer.size());
            // if (!state->model) {
            //     return std::unexpected(Error{kResultInvalidState, "Failed to initialize model from buffer"});
            // }
        }
        else {
            // No callbacks - use standard file I/O from disk path
            NVIGI_LOG_INFO("Loading model '%s' using standard file I/O", pathToModel.c_str());
            
            // Example: Load model from disk
            // state->model = your_model_init_from_file(pathToModel.c_str());
            // if (!state->model) {
            //     return std::unexpected(Error{kResultInvalidState, "Failed to initialize model from file"});
            // }
        }

        state->isInitialized = true;

        // Store instance context in pluginData (MUST use std::shared_ptr for proper lifetime management)
        // Transfer ownership from unique_ptr to shared_ptr
        auto sharedState = std::shared_ptr<InstanceContext>(state.release());
        pluginData = sharedState;

        NVIGI_LOG_INFO("Template AI plugin instance created successfully");
        return {};
    }

    //! Called when an instance is destroyed
    //! Clean up per-instance resources here
    static Expected<void> onDestroyInstance(std::any& pluginData)
    {
        NVIGI_LOG_INFO("Destroying Template AI plugin instance");

        // The shared_ptr will automatically clean up when std::any is destroyed
        // But we can reset it explicitly to be clear about cleanup order
        if (auto statePtr = std::any_cast<std::shared_ptr<InstanceContext>>(&pluginData)) {
            if (*statePtr) {
                auto state = statePtr->get();
                
                // Your model cleanup happens in ~PluginState() destructor
                // But you can do explicit cleanup here if needed
                
#if defined(PLUGIN_USES_CUDA)
                // Example: Destroy CUDA streams
                // for (auto stream : state->cudaStreams) {
                //     cudaStreamDestroy(stream);
                // }
                // state->cudaStreams.clear();
                //
                // Example: Free CUDA memory
                // if (state->deviceBuffer) {
                //     cudaFree(state->deviceBuffer);
                //     state->deviceBuffer = nullptr;
                // }
#endif

#if defined(PLUGIN_USES_D3D12)
                // Example: Release D3D12 resources
                // if (state->commandQueue) {
                //     state->commandQueue->Release();
                //     state->commandQueue = nullptr;
                // }
#endif
            }
            
            // Reset the shared_ptr to trigger cleanup
            statePtr->reset();
        }

        NVIGI_LOG_INFO("Template AI plugin instance destroyed successfully");
        return {};
    }

    // ========================================================================
    // Inference Execution
    // ========================================================================

    //! Main inference callback - implement your AI model logic here
    //! 
    //! This is called by the framework for both sync and async execution.
    //! The framework handles all the complexity of threading, cancellation,
    //! result polling, etc.
    //!
    //! @param ctx PluginContext providing access to inputs, outputs, parameters, and state
    //! @return Expected<void> - success or error with message
    static Expected<void> onEvaluate(PluginContext& ctx)
    {
        NVIGI_LOG_VERBOSE("Template AI plugin evaluate called");

        // Retrieve instance context
        auto statePtr = ctx.getPluginData<std::shared_ptr<InstanceContext>>();
        if (!statePtr || !*statePtr)
        {
            return std::unexpected(Error{
                kResultInvalidState,
                "Instance context not initialized"
            });
        }

        auto state = statePtr->get();

        if (!state->isInitialized)
        {
            return std::unexpected(Error{
                kResultInvalidState,
                "Instance not properly initialized"
            });
        }

        // ====================================================================
        // GPU Context Management
        // ====================================================================
        // For CUDA plugins, we need to ensure the correct CUDA context is active
        // RuntimeContextScope handles automatic push/pop of the CUDA context

#if defined(PLUGIN_USES_CUDA)
        // RAII scope guard that pushes CUDA context on construction, pops on destruction
        // This ensures all CUDA API calls in this function use the correct context
        RuntimeContextScope scope(*state);  // Pass PluginState reference
        
        // Now you can safely use CUDA APIs
        // Example:
        // cudaMemcpyAsync(dst, src, size, cudaMemcpyHostToDevice, state->cudaStreams[0]);
        // cudaStreamSynchronize(state->cudaStreams[0]);
        
        // The context will be automatically popped when scope goes out of scope
#endif

#if defined(PLUGIN_USES_D3D12)
        // D3D12 context management (if needed)
        // Example:
        // ID3D12CommandQueue* queue = state->commandQueue;
#endif

#if defined(PLUGIN_USES_VULKAN)
        // Vulkan context management (if needed)
        // Example:
        // VkCommandBuffer cmd = state->commandBuffer;
#endif

        // ====================================================================
        // Get Inputs (Type-Safe)
        // ====================================================================

        // Get required input
        auto promptResult = ctx.getInput<std::string>(kTemplateAIInputPrompt);
        if (!promptResult)
        {
            return std::unexpected(promptResult.error());
        }
        std::string prompt = *promptResult;

        NVIGI_LOG_INFO("Processing prompt: %s", prompt.c_str());

        // Get optional inputs (if your plugin supports them)
        // auto temperature = ctx.getOptionalInput<float>("temperature");
        // if (temperature) {
        //     NVIGI_LOG_VERBOSE("Using temperature: %f", *temperature);
        // }

        // ====================================================================
        // Get Runtime Parameters (Optional)
        // ====================================================================
        // Runtime parameters are provided per-evaluation (not per-instance)
        // Use these for parameters that can change between evaluations

        // Example: Get runtime configuration
        // if (auto runtime = ctx.getRuntimeParam<TemplateAIRuntimeParameters>()) {
        //     float temperature = (*runtime)->temperature;
        //     int maxTokens = (*runtime)->maxTokens;
        //     NVIGI_LOG_VERBOSE("Runtime config: temp=%.2f, maxTokens=%d", temperature, maxTokens);
        // }

        // ====================================================================
        // Check Cancellation
        // ====================================================================
        // For long-running operations, periodically check if cancellation was requested

        if (ctx.isCancelled())
        {
            NVIGI_LOG_VERBOSE("Evaluation cancelled before processing");
            return std::unexpected(Error{
                kResultCanceled,
                "Evaluation cancelled by user"
            });
        }

        // ====================================================================
        // Perform Inference
        // ====================================================================
        // Replace this with your actual model inference code

        // Simple echo example (replace with your model)
        std::string response = "Echo: " + prompt;

        // Example: Run inference with your model
        // std::string response = your_model_generate(state->model, prompt.c_str());
        // if (response.empty()) {
        //     return std::unexpected(Error{kResultInvalidState, "Model generation failed"});
        // }

        // Example: For long operations, check cancellation periodically
        // This allows the host to interrupt processing if needed
        // const int maxSteps = 100;
        // for (int step = 0; step < maxSteps; ++step) {
        //     if (ctx.isCancelled()) {
        //         NVIGI_LOG_VERBOSE("Inference cancelled at step %d", step);
        //         return std::unexpected(Error{
        //             kResultCanceled,
        //             "Evaluation cancelled during processing"
        //         });
        //     }
        //     
        //     // Perform one step of computation
        //     // your_model_step(state->model);
        // }

        // ====================================================================
        // Set Outputs (Type-Safe and Ergonomic)
        // ====================================================================
        // IMPORTANT: You MUST use the fluent builder pattern with .build() to trigger results!
        //
        // How it works:
        // 1. .set() stores outputs in m_pendingOutputs (no callback yet)
        // 2. .build() calls flushOutputs() which:
        //    a) Writes outputs to execCtx->outputs slots (or creates temp slots)
        //    b) WITH callback: Invokes execCtx->callback() immediately
        //    c) WITHOUT callback (polled): Signals pollCtx to unblock getResults()
        //
        // Your code is IDENTICAL for both sync/async and callback/polled modes!
        // The framework handles the differences automatically.
        //
        // DO NOT just use ctx.setOutput() without .build() - results won't be delivered!

        NVIGI_LOG_VERBOSE("Template AI plugin evaluate completed successfully");
        return ctx.buildOutput()
            .set(kTemplateAIOutputResponse, response)
            .build();

        // For multiple outputs, chain more .set() calls:
        // return ctx.buildOutput()
        //     .set(kTemplateAIOutputResponse, response)
        //     .set("confidence", 0.95f)
        //     .set("tokens", tokenCount)
        //     .build();
    }

    //! Cancellation callback - called when host requests cancellation
    //! 
    //! This is called when the host wants to cancel an ongoing async evaluation.
    //! The framework sets the cancellation flag automatically, but you can
    //! do additional cleanup here if needed.
    //!
    //! @param ctx PluginContext
    //! @return Expected<void> - success or error with message
    static Expected<void> onCancel(PluginContext& ctx)
    {
        NVIGI_LOG_INFO("Template AI plugin cancellation requested");

        // The framework already set the cancellation flag (ctx.isCancelled() will return true)
        // Your onEvaluate() should check isCancelled() periodically and return early

        // Optional: Clear any buffered state or interrupt model processing
        auto statePtr = ctx.getPluginData<std::shared_ptr<InstanceContext>>();
        if (statePtr && *statePtr) {
            auto state = statePtr->get();
            
            // Example: Clear any buffered data
            // state->inputBuffer.clear();
            
            // Example: Interrupt model processing if your model supports it
            // your_model_interrupt(state->model);
        }

        return {};
    }
};

} // namespace nvigi

// ============================================================================
// Plugin Context
// ============================================================================
// Define a simple context structure for the plugin.
// This holds global plugin state (not per-instance state).
//
// IMPORTANT: This must be defined inside the plugin's namespace
// (template_ai in this case) so the macro can find it.
//
namespace nvigi::template_ai {

struct TemplateAIPluginContext {
    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(TemplateAIPluginContext);
    
    void onCreateContext() {
        // Called when plugin DLL is loaded
        // Add any global initialization here
    }
    
    void onDestroyContext() {
        // Called when plugin DLL is unloaded
        // Add any global cleanup here
    }
};

} // namespace nvigi::template_ai

// ============================================================================
// Plugin Export
// ============================================================================
// This macro generates all the plugin entry points and registration code.
// It connects your TemplateAIPlugin implementation to the framework.
//
// IMPORTANT: This macro must be called at global scope (outside all namespaces)
//
NVIGI_MODERN_PLUGIN(
    nvigi::TemplateAIPlugin,                          // Plugin class (fully qualified)
    nvigi::ITemplateAI,                               // API interface (fully qualified)
    "nvigi.plugin.template.ai",                       // Plugin name
    Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH),  // Plugin version (Version is imported)
    Version(API_MAJOR, API_MINOR, API_PATCH),         // API version (Version is imported)
    template_ai,                                      // Namespace (context is in nvigi::template_ai)
    TemplateAIPluginContext                           // Plugin context type (will be found in template_ai namespace)
)
