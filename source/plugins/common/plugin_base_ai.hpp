// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once

#include <expected>
#include <span>
#include <any>
#include <optional>
#include <string_view>
#include <functional>
#include <future>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <concepts>
#include <unordered_map>
#include <chrono>

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.exception/exception.h"
#include "source/core/nvigi.plugin/plugin.h"
#include "source/utils/nvigi.poll/poll.h"
#include "source/utils/nvigi.ai/ai.h"
#include "external/json/source/nlohmann/json.hpp"

using json = nlohmann::json;

namespace nvigi::plugin::modern {

// ============================================================================
// Error Type with Context
// ============================================================================

struct Error {
    Result code;
    std::string message;

    Error(Result c, std::string msg = "") : code(c), message(std::move(msg)) {}

    operator Result() const { return code; }
};

template<typename T>
using Expected = std::expected<T, Error>;

// ============================================================================
// Helper Macros for Cleaner Code
// ============================================================================

#define NVIGI_PLUGIN_ID(a, b, c, d, e, f, g, h, i, j, k) \
    PluginID{a, b, c, {d, e, f, g, h, i, j, k}}

// Slot definition macros
#define NVIGI_TEXT_SLOT(name, optional) \
    InferenceDataDescriptor{name, InferenceDataText::s_type, optional}

#define NVIGI_SCALAR_SLOT(name, optional) \
    InferenceDataDescriptor{name, InferenceDataScalar::s_type, optional}

#define NVIGI_IMAGE_SLOT(name, optional) \
    InferenceDataDescriptor{name, InferenceDataImage::s_type, optional}

#define NVIGI_AUDIO_SLOT(name, optional) \
    InferenceDataDescriptor{name, InferenceDataAudio::s_type, optional}

#define NVIGI_TENSOR_SLOT(name, optional) \
    InferenceDataDescriptor{name, InferenceDataTensor::s_type, optional}

#define NVIGI_SIGNATURE(...) \
    []() -> std::span<const InferenceDataDescriptor> { \
        static const InferenceDataDescriptor descriptors[] = {__VA_ARGS__}; \
        return std::span(descriptors); \
    }()

// ============================================================================
// Plugin Context - Ergonomic API for Plugin Authors
// ============================================================================

class PluginContext {
public:
    // Construction
    PluginContext(InferenceExecutionContext* execCtx,
        const NVIGIParameter* creationParams,
        std::any& pluginDataParam,
        poll::PollContext<InferenceExecutionState>* pollCtxPtr = nullptr)
        : m_execCtx(execCtx)
        , m_creationParams(creationParams)
        , pluginData(pluginDataParam)      // Initialize public reference directly from parameter
        , m_pluginData(pluginDataParam)    // Initialize private reference from same parameter
        , m_pollCtx(pollCtxPtr)
    {
        NVIGI_LOG_INFO("PluginContext constructor: pluginData param address=%p, public ref address=%p, private ref address=%p",
                      &pluginDataParam, &pluginData, &m_pluginData);
    }

    // ========================================================================
    // Get Creation Parameters (Type-Safe)
    // ========================================================================

    template<typename T>
    std::optional<const T*> getCreationParam() const {
        if (auto param = findStruct<T>(m_creationParams)) {
            return param;
        }
        return std::nullopt;
    }

    // ========================================================================
    // Get Runtime Parameters (Type-Safe)
    // ========================================================================

    template<typename T>
    std::optional<const T*> getRuntimeParam() const {
        if (m_execCtx && m_execCtx->runtimeParameters) {
            if (auto param = findStruct<T>(m_execCtx->runtimeParameters)) {
                return param;
            }
        }
        return std::nullopt;
    }

    // ========================================================================
    // Get Inputs (Type-Safe and Ergonomic)
    // ========================================================================

    // Required input - returns error if missing
    template<typename T>
    Expected<T> getInput(std::string_view name) const {
        if (!m_execCtx || !m_execCtx->inputs) {
            return std::unexpected(Error{ kResultInvalidParameter, "No inputs provided" });
        }

        if constexpr (std::is_same_v<T, std::string>) {
            const InferenceDataText* data{};
            if (!m_execCtx->inputs->findAndValidateSlot(name.data(), &data)) {
                return std::unexpected(Error{
                    kResultInvalidParameter,
                    std::string("Missing required input: ") + std::string(name)
                    });
            }
            return std::string(data->getUTF8Text());
        }
        //else if constexpr (std::is_floating_point_v<T> || std::is_integral_v<T>) {
        //    const InferenceDataScalar* data{};
        //    if (!m_execCtx->inputs->findAndValidateSlot(name.data(), &data)) {
        //        return std::unexpected(Error{
        //            kResultInvalidParameter,
        //            std::string("Missing required input: ") + std::string(name)
        //        });
        //    }
        //    if constexpr (std::is_floating_point_v<T>) {
        //        return static_cast<T>(data->f64);
        //    } else {
        //        return static_cast<T>(data->i64);
        //    }
        //}
        //else if constexpr (std::is_same_v<T, InferenceDataImage>) {
        //    const InferenceDataImage* data{};
        //    if (!m_execCtx->inputs->findAndValidateSlot(name.data(), &data)) {
        //        return std::unexpected(Error{
        //            kResultInvalidParameter,
        //            std::string("Missing required input: ") + std::string(name)
        //        });
        //    }
        //    return *data;
        //}
        //else if constexpr (std::is_same_v<T, std::vector<float>>) {
        //    const InferenceDataTensor* data{};
        //    if (!m_execCtx->inputs->findAndValidateSlot(name.data(), &data)) {
        //        return std::unexpected(Error{
        //            kResultInvalidParameter,
        //            std::string("Missing required input: ") + std::string(name)
        //        });
        //    }
        //    // Convert tensor to vector
        //    std::vector<float> result;
        //    // TODO: Implement tensor extraction
        //    return result;
        //}
        else {
            static_assert(always_false<T>, "Unsupported input type");
        }
    }

    // Optional input - returns nullopt if missing
    template<typename T>
    std::optional<T> getOptionalInput(std::string_view name) const {
        auto result = getInput<T>(name);
        if (result) {
            return *result;
        }
        return std::nullopt;
    }

    // ========================================================================
    // Set Outputs (Type-Safe and Ergonomic)
    // ========================================================================

    template<typename T>
    Expected<void> setOutput(std::string_view name, const T& value) {
        if (!m_execCtx) {
            return std::unexpected(Error{ kResultInvalidParameter, "No execution context" });
        }

        // Store output for later
        m_pendingOutputs[std::string(name)] = value;
        return {};
    }

    // ========================================================================
    // Fluent Output Builder
    // ========================================================================

    class OutputBuilder {
    public:
        OutputBuilder(PluginContext& ctx) : m_ctx(ctx) {}

        template<typename T>
        OutputBuilder& set(std::string_view name, const T& value) {
            [[maybe_unused]] auto result = m_ctx.setOutput(name, value);
            return *this;
        }

        Expected<void> build() {
            return m_ctx.flushOutputs();
        }

    private:
        PluginContext& m_ctx;
    };

    OutputBuilder buildOutput() {
        return OutputBuilder(*this);
    }

    // ========================================================================
    // Plugin-Specific State (Type-Safe with std::any)
    // ========================================================================

    std::any& pluginData;

    // Get plugin data stored in std::any
    // 
    // IMPORTANT: std::any_cast behavior:
    //   - If std::any contains type T, std::any_cast<T>(&any) returns T*
    //   - If std::any contains std::shared_ptr<T>, std::any_cast<std::shared_ptr<T>>(&any) 
    //     returns std::shared_ptr<T>*, then call ->get() to get T*
    // 
    // Recommended: Use std::shared_ptr for plugin state (handles lifetime automatically)
    // 
    // Example usage:
    //   // Store state
    //   pluginData = std::shared_ptr<MyState>(new MyState());
    //   
    //   // Retrieve state
    //   auto statePtr = getPluginData<std::shared_ptr<MyState>>();
    //   if (statePtr && *statePtr) {
    //       auto state = statePtr->get();  // Get raw pointer
    //       state->doSomething();
    //   }
    template<typename T>
    T* getPluginData() {
        return std::any_cast<T>(&pluginData);
    }

    template<typename T>
    void setPluginData(T&& data) {
        pluginData = std::forward<T>(data);
    }

    // ========================================================================
    // Check Cancellation
    // ========================================================================

    // Check if cancellation was requested
    // Plugins should call this periodically during long-running operations
    // to allow early termination
    bool isCancelled() const {
        return m_cancelled && m_cancelled->load();
    }

    void setCancelledFlag(std::atomic<bool>* flag) {
        m_cancelled = flag;
    }

    // Get execution context (for advanced usage)
    InferenceExecutionContext* getExecutionContext() const {
        return m_execCtx;
    }

private:
    template<typename T>
    static constexpr bool always_false = false;

    Expected<void> flushOutputs() {
        if (!m_execCtx) {
            return std::unexpected(Error{kResultInvalidParameter, "No execution context"});
        }

        // Create temporary output slots if host didn't provide them
        std::vector<InferenceDataSlot> tempSlots;
        std::vector<CpuData> tempBuffers;  // Keep alive for callback duration
        std::vector< InferenceDataText> tempTextData;  // Keep alive for callback duration
        InferenceDataSlotArray tempOutputs{};
        InferenceDataSlotArray* originalOutputs = m_execCtx->outputs;
        
        bool usingTempOutputs = false;
        
        if (!m_execCtx->outputs) {
            // Host didn't provide output slots, create temporary ones
            NVIGI_LOG_VERBOSE("Creating temporary output slots");
            usingTempOutputs = true;
            
            for (const auto& [slotName, value] : m_pendingOutputs) {
                if (auto strValue = std::any_cast<std::string>(&value)) {
                    tempBuffers.emplace_back(strValue->length() + 1, (const void*)strValue->c_str());
                    tempTextData.emplace_back(InferenceDataText(tempBuffers.back()));
                    tempSlots.push_back({slotName.c_str(), tempTextData.back()});
                }
            }
            
            tempOutputs = { static_cast<uint32_t>(tempSlots.size()), tempSlots.data() };
            m_execCtx->outputs = &tempOutputs;
        }

        // Write all pending outputs to the execution context
        for (const auto& [slotName, value] : m_pendingOutputs) {
            // For now, only support string outputs (text slots)
            if (auto strValue = std::any_cast<std::string>(&value)) {
                const InferenceDataText* outputSlot{};
                if (m_execCtx->outputs->findAndValidateSlot(slotName.c_str(), &outputSlot)) {
                    auto cpuBuffer = castTo<CpuData>(outputSlot->utf8Text);
                    if (cpuBuffer->buffer && cpuBuffer->sizeInBytes >= strValue->size() + 1) {
                        strcpy_s((char*)cpuBuffer->buffer, cpuBuffer->sizeInBytes, strValue->c_str());
                        NVIGI_LOG_VERBOSE("Wrote output '%s': %zu bytes", slotName.c_str(), strValue->size());
                    } else {
                        NVIGI_LOG_ERROR("Output buffer too small for slot '%s'", slotName.c_str());
                        if (usingTempOutputs) {
                            m_execCtx->outputs = originalOutputs;
                        }
                        return std::unexpected(Error{kResultInsufficientResources, "Output buffer too small"});
                    }
                }
            }
        }
        
        // Trigger callback with the outputs
        if (m_execCtx->callback) {
            // Callback mode: Invoke callback directly
            m_execCtx->callback(m_execCtx, kInferenceExecutionStateDone, m_execCtx->callbackUserData);
        } else if (m_pollCtx) {
            // Polled mode: Signal poll context to unblock getResults()
            m_pollCtx->triggerCallback(kInferenceExecutionStateDone);
        }
        
        // Restore original outputs pointer if we used temp
        if (usingTempOutputs) {
            m_execCtx->outputs = originalOutputs;
        }
        
        m_pendingOutputs.clear();
        return {};
    }

    InferenceExecutionContext* m_execCtx;
    const NVIGIParameter* m_creationParams;
    std::any& m_pluginData;
    std::atomic<bool>* m_cancelled = nullptr;
    poll::PollContext<InferenceExecutionState>* m_pollCtx = nullptr;

    std::unordered_map<std::string, std::any> m_pendingOutputs;
};

// ============================================================================
// Plugin Concept - Compile-Time Interface Validation
// ============================================================================

template<typename T>
concept PluginConcept = requires(T t, PluginContext & ctx, const NVIGIParameter * params, std::any & pluginData) {
    // Required static methods
    { T::getPluginID() } -> std::same_as<PluginID>;
    { T::getPluginInfo() } -> std::convertible_to<plugin::PluginInfo>;
    { T::getPluginInputSignature() } -> std::convertible_to<std::span<const InferenceDataDescriptor>>;
    { T::getPluginOutputSignature() } -> std::convertible_to<std::span<const InferenceDataDescriptor>>;
    { T::getPluginCapsAndRequirements(params) } -> std::same_as<Expected<CommonCapabilitiesAndRequirements>>;
    { T::onPluginRegister(std::declval<framework::IFramework*>()) } -> std::same_as<Result>;
    { T::onPluginDeregister() } -> std::same_as<Result>;
    { T::onCreateInstance(params, pluginData) } -> std::same_as<Expected<void>>;
    { T::onDestroyInstance(pluginData) } -> std::same_as<Expected<void>>;
    { T::onEvaluate(ctx) } -> std::same_as<Expected<void>>;
    { T::onCancel(ctx) } -> std::same_as<Expected<void>>;
};

// ============================================================================
// Modern Plugin Base Template
// ============================================================================

template<PluginConcept PluginImpl, typename APIType>
class ModernPluginBase {
public:
    // ========================================================================
    // Instance Data
    // ========================================================================

    struct InstanceData {
#if GGML_USE_CUBLAS
        InstanceData(const NVIGIParameter* params) : cudaContext(params) {}
        PushPoppableCudaContext cudaContext;
#else
        InstanceData(const NVIGIParameter*) {}
#endif

        poll::PollContext<InferenceExecutionState> pollCtx;
        json modelInfo;

        std::mutex mtx;
        std::future<Result> job;
        std::atomic<bool> running{ true };     // Controls the async evaluation loop
        std::atomic<bool> cancelled{ false };  // Signals cancellation request to plugin

        std::any pluginData;
        const NVIGIParameter* creationParams = nullptr;
    };

    // ========================================================================
    // Plugin Context
    // ========================================================================

    struct PluginContextImpl {
        // Use default constructor instead of NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY
        // to allow static instance in getContext()
        PluginContextImpl() = default;

        void onCreateContext() {}
        void onDestroyContext() {}

        APIType api{};
        IPolledInferenceInterface polledApi{};
        PluginID feature{};

        ai::CommonCapsData capsData;
        json modelInfo;

#ifdef GGML_USE_CUBLAS
        nvigi::IHWICuda* icig{};
#elif defined(GGML_USE_D3D12)
        nvigi::IHWID3D12* iscg{};
        nvigi::system::ISystem* isystem{};
#endif
    };

    // ========================================================================
    // Public API
    // ========================================================================

    static Result createInstance(const NVIGIParameter* params, InferenceInstance** outInstance) {
        NVIGI_CATCH_EXCEPTION(createInstanceImpl(params, outInstance));
    }

    static Result destroyInstance(const InferenceInstance* instance) {
        NVIGI_CATCH_EXCEPTION(destroyInstanceImpl(instance));
    }

    static Result evaluate(InferenceExecutionContext* execCtx) {
        NVIGI_CATCH_EXCEPTION(evaluateInternal(execCtx, false));
    }

    static Result evaluateAsync(InferenceExecutionContext* execCtx) {
        NVIGI_CATCH_EXCEPTION(evaluateInternal(execCtx, true));
    }

    static Result getResults(InferenceExecutionContext* execCtx, bool wait, InferenceExecutionState* state) {
        NVIGI_CATCH_EXCEPTION(getResultsImpl(execCtx, wait, state));
    }

    static Result releaseResults(InferenceExecutionContext* execCtx, InferenceExecutionState state) {
        NVIGI_CATCH_EXCEPTION(releaseResultsImpl(execCtx, state));
    }

    static Result cancelAsyncEvaluation(InferenceExecutionContext* execCtx) {
        NVIGI_CATCH_EXCEPTION(cancelAsyncEvaluationImpl(execCtx));
    }

    // ========================================================================
    // Plugin Registration
    // ========================================================================

    static Result pluginGetInfo(framework::IFramework* framework, plugin::PluginInfo** outInfo) {
        if (!plugin::internalPluginSetup(framework))
            return kResultInvalidState;

        auto& info = plugin::getContext()->info;
        *outInfo = &info;

        auto pluginInfo = PluginImpl::getPluginInfo();
        info.id = PluginImpl::getPluginID();
        info.description = pluginInfo.description;
        info.author = pluginInfo.author;
        info.build = pluginInfo.build;
        info.interfaces = pluginInfo.interfaces;
        info.minOS = pluginInfo.minOS;
        info.minGPUArch = pluginInfo.minGPUArch;
        info.minDriver = pluginInfo.minDriver;
        info.requiredVendor = pluginInfo.requiredVendor;

        return kResultOk;
    }

    static Result pluginRegister(framework::IFramework* framework) {
        if (!plugin::internalPluginSetup(framework))
            return kResultInvalidState;

        auto& ctx = getContext();
        ctx.feature = PluginImpl::getPluginID();

        ctx.api.createInstance = createInstance;
        ctx.api.destroyInstance = destroyInstance;
        ctx.api.getCapsAndRequirements = getCapsAndRequirements;

        framework->addInterface(ctx.feature, &ctx.api, 0);

        ctx.polledApi.getResults = getResults;
        ctx.polledApi.releaseResults = releaseResults;
        framework->addInterface(ctx.feature, &ctx.polledApi, 0);

        return PluginImpl::onPluginRegister(framework);
    }

    static Result pluginDeregister() {
        auto& ctx = getContext();
        ai::freeCommonCapsAndRequirements(ctx.capsData);

#if GGML_USE_CUBLAS
        if (ctx.icig) {
            framework::releaseInterface(plugin::getContext()->framework, plugin::hwi::cuda::kId, ctx.icig);
            ctx.icig = nullptr;
        }
#elif defined(GGML_USE_D3D12)
        if (ctx.iscg) {
            framework::releaseInterface(plugin::getContext()->framework, plugin::hwi::d3d12::kId, ctx.iscg);
            ctx.iscg = nullptr;
        }
        if (ctx.isystem) {
            framework::releaseInterface(plugin::getContext()->framework, nvigi::core::framework::kId, ctx.isystem);
            ctx.isystem = nullptr;
        }
#endif

        return PluginImpl::onPluginDeregister();
    }

    static PluginContextImpl& getContext() {
        static PluginContextImpl s_context;
        return s_context;
    }

private:
    // ========================================================================
    // Internal Implementation (for exception wrapper)
    // ========================================================================

    static Result createInstanceImpl(const NVIGIParameter* params, InferenceInstance** outInstance) {
        auto common = findStruct<CommonCreationParameters>(params);
        if (!common || !params || !outInstance)
            return kResultInvalidParameter;

        *outInstance = nullptr;

        auto instance = new InstanceData(params);
        instance->creationParams = params;

#if GGML_USE_CUBLAS
        if (!instance->cudaContext.constructorSucceeded) {
            delete instance;
            return kResultInvalidState;
        }
#endif

        // Call plugin's onCreateInstance callback
        auto createResult = PluginImpl::onCreateInstance(params, instance->pluginData);
        if (!createResult) {
            NVIGI_LOG_ERROR("onCreateInstance failed: %s", createResult.error().message.c_str());
            delete instance;
            return createResult.error().code;
        }

        auto wrapper = new InferenceInstance();
        wrapper->data = instance;
        wrapper->getFeatureId = getFeatureId;
        wrapper->getInputSignature = getInputSignature;
        wrapper->getOutputSignature = getOutputSignature;
        wrapper->evaluate = evaluate;
        wrapper->evaluateAsync = evaluateAsync;
        wrapper->cancelAsyncEvaluation = cancelAsyncEvaluation;

        *outInstance = wrapper;
        return kResultOk;
    }

    static Result destroyInstanceImpl(const InferenceInstance* instance) {
        if (instance) {
            auto ctx = static_cast<InstanceData*>(instance->data);
            flushAndTerminate(ctx);
            
            // Call plugin's onDestroyInstance callback
            auto destroyResult = PluginImpl::onDestroyInstance(ctx->pluginData);
            if (!destroyResult) {
                NVIGI_LOG_ERROR("onDestroyInstance failed: %s", destroyResult.error().message.c_str());
                // Continue cleanup even if callback failed
            }
            
            delete ctx;
            delete instance;
        }
        return kResultOk;
    }

    static Result getResultsImpl(InferenceExecutionContext* execCtx, bool wait, InferenceExecutionState* state) {
        if (!execCtx || !execCtx->instance)
            return kResultInvalidParameter;

        auto instance = static_cast<InstanceData*>(execCtx->instance->data);
        return instance->pollCtx.getResults(wait, state);
    }

    static Result releaseResultsImpl(InferenceExecutionContext* execCtx, InferenceExecutionState state) {
        if (!execCtx || !execCtx->instance)
            return kResultInvalidParameter;

        auto instance = static_cast<InstanceData*>(execCtx->instance->data);
        return instance->pollCtx.releaseResults(state);
    }

    static Result cancelAsyncEvaluationImpl(InferenceExecutionContext* execCtx) {
        if (!execCtx || !execCtx->instance)
            return kResultInvalidParameter;

        auto instance = static_cast<InstanceData*>(execCtx->instance->data);

        // Check if async job is actually running
        if (!instance->job.valid()) {
            NVIGI_LOG_WARN("cancelAsyncEvaluation called but no async evaluation is running");
            return kResultNoImplementation;
        }

        // Set cancellation flag to interrupt the evaluation loop as early as possible
        instance->cancelled.store(true);

        // Let plugin handle cancellation
        PluginContext ctx(execCtx, instance->creationParams, instance->pluginData, &instance->pollCtx);
        auto result = PluginImpl::onCancel(ctx);
        if (!result) {
            NVIGI_LOG_ERROR("Cancel failed: %s", result.error().message.c_str());
        }

        return flushAndTerminate(instance);
    }

    // ========================================================================
    // Internal Helpers
    // ========================================================================

    // Lambda factory for async evaluation jobs
    // This creates the worker function that runs in the background thread.
    // It checks both 'running' and 'cancelled' flags to allow graceful termination.
    //
    // IMPORTANT: The plugin's onEvaluate() may trigger callbacks or use polled results:
    //   - WITH callback: Calls execCtx->callback(), returns immediately
    //   - WITHOUT callback (polled): Calls pollCtx.triggerCallback(), which blocks
    //     until host calls getResults() and releaseResults()
    static auto createEvaluationJob(InstanceData* instance, InferenceExecutionContext* execCtx) {
        return [instance, execCtx]() -> Result {
            PluginContext ctx(execCtx, instance->creationParams, instance->pluginData, &instance->pollCtx);
            ctx.setCancelledFlag(&instance->cancelled);

            auto res = kResultOk;
            while (instance->running.load() && !instance->cancelled.load() && res == kResultOk) {
                auto result = PluginImpl::onEvaluate(ctx);
                if (!result) {
                    NVIGI_LOG_ERROR("Evaluation failed: %s", result.error().message.c_str());
                    res = result.error().code;
                    break;
                }

                // For most plugins, one evaluation is enough
                break;
            }

            // If cancelled, exit cleanly
            if (instance->cancelled.load()) {
                NVIGI_LOG_VERBOSE("Evaluation cancelled by user request");
                return kResultOk;
            }

            return res;
        };
    }

    static PluginID getFeatureId(InferenceInstanceData* data) {
        return PluginImpl::getPluginID();
    }

    static const InferenceDataDescriptorArray* getInputSignature(InferenceInstanceData* data) {
        static auto span = PluginImpl::getPluginInputSignature();
        static InferenceDataDescriptorArray s_desc = {
            static_cast<uint32_t>(span.size()),
            const_cast<InferenceDataDescriptor*>(span.data())
        };
        return &s_desc;
    }

    static const InferenceDataDescriptorArray* getOutputSignature(InferenceInstanceData* data) {
        static auto span = PluginImpl::getPluginOutputSignature();
        static InferenceDataDescriptorArray s_desc = {
            static_cast<uint32_t>(span.size()),
            const_cast<InferenceDataDescriptor*>(span.data())
        };
        return &s_desc;
    }

    static Result getCapsAndRequirements(NVIGIParameter** outInfo, const NVIGIParameter* params) {
        auto result = PluginImpl::getPluginCapsAndRequirements(params);
        if (!result) {
            NVIGI_LOG_ERROR("getCapsAndRequirements failed: %s", result.error().message.c_str());
            return result.error().code;
        }

        static CommonCapabilitiesAndRequirements s_caps;
        s_caps = *result;
        // CommonCapabilitiesAndRequirements inherits from NVIGIParameter via NVIGI_UID macro
        *outInfo = s_caps;
        return kResultOk;
    }

    // ========================================================================
    // Evaluation Implementation
    // ========================================================================
    //
    // ASYNC MODE INPUT HANDLING AND DEADLOCK PREVENTION:
    // ---------------------------------------------------
    // When evaluateAsync() is called while a job is already running:
    //
    // Case 1: WITH CALLBACK (execCtx->callback != nullptr)
    //   - Background thread calls the callback and continues
    //   - No blocking, no deadlock risk
    //   - BUT: Still can't process new inputs without buffering/queueing
    //   - Solution: Return kResultNotReady to signal inputs would be lost
    //
    // Case 2: WITHOUT CALLBACK (execCtx->callback == nullptr) - POLLED RESULTS
    //   - Background thread blocks waiting for host to call getResults()
    //   - If host calls evaluateAsync() again while blocked -> DEADLOCK!
    //   - We MUST NOT wait for the job, as host may need to consume results first
    //   - Solution: Check with very short timeout, return kResultNotReady if busy
    //
    // Deadlock scenario (polled results):
    //   Host -> evaluateAsync() -> job starts -> produces results -> blocks waiting
    //                ^                                                       |
    //                |                                                       |
    //                +----------- Host calls evaluateAsync() again <---------+
    //                             (would deadlock if we wait here!)
    //
    // Implementation: Check job status with 10μs timeout
    //   - If job is done: Get result, start new job with new inputs
    //   - If job is busy: Return kResultNotReady (prevents deadlock + signals loss)
    //
    // kResultNotReady tells the host:
    //   1. Current evaluation still in progress
    //   2. New inputs NOT processed (would be lost)
    //   3. Retry after consuming results (especially critical for polled mode)
    //
    // NOTE: Plugins needing input buffering (like ASR streaming) should buffer
    // BEFORE calling base evaluate, allowing rapid calls without data loss.
    //
    static Result evaluateInternal(InferenceExecutionContext* execCtx, bool async) {
        if (!execCtx) {
            NVIGI_LOG_ERROR("No execution context");
            return kResultInvalidParameter;
        }

        if (!execCtx->callback && !async) {
            NVIGI_LOG_ERROR("Callback not provided for sync evaluation");
            return kResultInvalidParameter;
        }

        if (!execCtx->instance) {
            NVIGI_LOG_ERROR("No inference instance");
            return kResultInvalidParameter;
        }

        auto instance = static_cast<InstanceData*>(execCtx->instance->data);

        if (async) {
            // Async execution
            if (!instance->job.valid()) {
                // No job running, start a new one
                instance->running.store(true);
                instance->cancelled.store(false);  // Reset cancellation flag for new evaluation
                instance->job = std::async(std::launch::async, createEvaluationJob(instance, execCtx));
            }
            else {
                // Job already running - check if it's done with a very short timeout
                // IMPORTANT: We use a short timeout (10 microseconds) to avoid deadlock.
                // The job might be blocked waiting for the host to consume polled results,
                // so we must NOT block here waiting for the job to complete.
                if (instance->job.wait_for(std::chrono::microseconds(10)) == std::future_status::ready) {
                    // Job completed, get the result and check for errors
                    if (NVIGI_FAILED(result, instance->job.get())) {
                        NVIGI_LOG_ERROR("Previous async evaluation returned error: %u", result);
                        return result;
                    }

                    // Start a new job
                    instance->running.store(true);
                    instance->cancelled.store(false);  // Reset cancellation flag for new evaluation
                    instance->job = std::async(std::launch::async, createEvaluationJob(instance, execCtx));
                }
                else {
                    // Job still running - return kResultNotReady
                    //
                    // This applies to BOTH callback and polled result modes:
                    //
                    // WITH callback (execCtx->callback != nullptr):
                    //   - Job won't block, but we still can't accept new inputs
                    //   - Returning kResultNotReady signals inputs would be lost
                    //   - Host should wait for current job to complete
                    //
                    // WITHOUT callback (polled results):
                    //   - Job may be blocked waiting for getResults()
                    //   - CRITICAL: We cannot wait here or we'd cause deadlock
                    //   - Host MUST consume results before calling evaluateAsync() again
                    //
                    // NOTE: Plugins with input buffering (like ASR streaming audio) can
                    // buffer inputs BEFORE calling base evaluate to avoid this error.
                    return kResultNotReady;
                }
            }
        }
        else {
            // Sync execution
            
            // First make sure any async jobs are done
            if (instance->job.valid()) {
                NVIGI_LOG_WARN("'evaluateAsync' task not finished, interrupting before running blocking 'evaluate' ...");
                instance->running.store(false);
                instance->job.get();
            }

            // Run synchronously
            NVIGI_LOG_INFO("Creating PluginContext for sync eval, instance=%p, pluginData address=%p", 
                          instance, &instance->pluginData);
            PluginContext ctx(execCtx, instance->creationParams, instance->pluginData, nullptr);
            NVIGI_LOG_INFO("PluginContext created, checking pluginData reference address=%p", 
                          &ctx.pluginData);
            ctx.setCancelledFlag(&instance->cancelled);

            auto result = PluginImpl::onEvaluate(ctx);
            if (!result) {
                NVIGI_LOG_ERROR("Evaluation failed: %s", result.error().message.c_str());
                return result.error().code;
            }
        }

        return kResultOk;
    }

    static Result flushAndTerminate(InstanceData* instance) {
        auto result = kResultOk;
        if (instance->job.valid()) {
            // Signal the async job to stop
            instance->running.store(false);
            instance->cancelled.store(true);

            // Keep draining results while waiting for job to complete
            auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (std::chrono::steady_clock::now() < timeout) {
                // Release any pending results to unblock the background thread
                if (instance->pollCtx.checkResultPending()) {
                    instance->pollCtx.releaseResults(kInferenceExecutionStateDone);
                }

                // Check if job is done
                auto futureResult = instance->job.wait_for(std::chrono::milliseconds(10));
                if (futureResult == std::future_status::ready) {
                    // Job completed, get the result
                    if (NVIGI_FAILED(result, instance->job.get())) {
                        return result;
                    }
                    return kResultOk;
                }
            }

            // Timeout occurred
            NVIGI_LOG_WARN("Async job timed out after 10 seconds");
            return kResultTimedOut;
        }
        return kResultOk;
    }
};

} // namespace nvigi::plugin::modern

// ============================================================================
// Simplified Export Macro
// ============================================================================

#define NVIGI_MODERN_PLUGIN(PluginClass, APIType, NAME, V1, V2, PLUGIN_NAMESPACE, PLUGIN_CTX) \
    namespace nvigi { \
    namespace plugin { \
        static Context* s_ctx{}; \
        Context* getContext() { return s_ctx; } \
    } \
    NVIGI_PLUGIN_CONTEXT_DEFINE(PLUGIN_NAMESPACE, PLUGIN_CTX) \
    Result nvigiPluginGetInfo(framework::IFramework* framework, plugin::PluginInfo** info) \
    { \
        return plugin::modern::ModernPluginBase<PluginClass, APIType>::pluginGetInfo(framework, info); \
    } \
    Result nvigiPluginRegister(framework::IFramework* framework) \
    { \
        return plugin::modern::ModernPluginBase<PluginClass, APIType>::pluginRegister(framework); \
    } \
    Result nvigiPluginDeregister() \
    { \
        return plugin::modern::ModernPluginBase<PluginClass, APIType>::pluginDeregister(); \
    } \
    NVIGI_EXPORT void* nvigiPluginGetFunction(const char* functionName) \
    { \
        NVIGI_EXPORT_FUNCTION(nvigiPluginGetInfo); \
        NVIGI_EXPORT_FUNCTION(nvigiPluginRegister); \
        NVIGI_EXPORT_FUNCTION(nvigiPluginDeregister); \
        return nullptr; \
    } \
    } \
    NVIGI_ENTRY_POINT_BODY(NAME, V1, V2, PLUGIN_NAMESPACE, PLUGIN_CTX)

