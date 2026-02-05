// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Modern AI Plugin Template - Unit Tests
// =======================================
// This file contains example unit tests for the modern AI plugin template.
// Copy and modify these tests for your own plugin.

#include "source/plugins/nvigi.template.inference/nvigi_template_infer.h"
#include "source/utils/nvigi.ai/ai_data_helpers.h"

namespace nvigi
{
namespace template_ai
{

//! Make sure to apply appropriate tags for your tests:
//! - [gpu], [cpu] - hardware requirements
//! - [cuda], [d3d12], [vulkan] - specific backends
//! - [inference] - AI inference tests
//! 
//! This allows easy unit test filtering as described here:
//! https://github.com/catchorg/Catch2/blob/devel/docs/command-line.md#specifying-which-tests-to-run
//! 
TEST_CASE("template_ai_basic", "[template],[inference],[cpu]")
{
    //! Use global params as needed (see source/tests/ai/main.cpp for details)

    // Get the plugin interface
    nvigi::ITemplateAI* itemplate{};
    auto loadResult = nvigiGetInterfaceDynamic(
        plugin::template_ai::kId, 
        &itemplate, 
        params.nvigiLoadInterface
    );
    
    REQUIRE(loadResult == nvigi::kResultOk);
    REQUIRE(itemplate != nullptr);

    // Test 1: Query capabilities without creating an instance
    NVIGI_LOG_TEST_INFO("Testing getCapsAndRequirements...");
    {
        // Setup creation parameters
        CommonCreationParameters common{};
        common.modelGUID = "{01234567-0123-0123-0123-0123456789AB}";
        common.utf8PathToModels = params.modelDir.c_str();
        common.numThreads = 1;

        nvigi::NVIGIParameter* capsInfo = nullptr;
        auto result = itemplate->getCapsAndRequirements(&capsInfo, common);
        REQUIRE(result == nvigi::kResultOk);
        REQUIRE(capsInfo != nullptr);
        
        auto caps = nvigi::findStruct<CommonCapabilitiesAndRequirements>(capsInfo);
        REQUIRE(caps != nullptr);
        NVIGI_LOG_TEST_INFO("Capabilities query successful");
    }

    // Test 2: Create and destroy an instance
    // NOTE: This will fail if model files are not available, which is expected for a template
    // The test validates that the creation/destruction flow works correctly
    NVIGI_LOG_TEST_INFO("Testing instance creation and destruction...");
    {
        // Setup creation parameters
        CommonCreationParameters common{};
        common.modelGUID = "{01234567-0123-0123-0123-0123456789AB}";
        common.utf8PathToModels = params.modelDir.c_str();
        common.numThreads = 1;
        
        TemplateAICreationParameters templateParams{};
        templateParams.chain(common);  // Chain common parameters
        
        nvigi::InferenceInstance* instance = nullptr;
        auto createResult = itemplate->createInstance(templateParams, &instance);
        
        if (createResult == nvigi::kResultOk && instance != nullptr)
        {
            NVIGI_LOG_TEST_INFO("Instance created successfully");
            
            // Verify the instance has the expected function pointers
            REQUIRE(instance->evaluate != nullptr);
            REQUIRE(instance->getInputSignature != nullptr);
            REQUIRE(instance->getOutputSignature != nullptr);
            
            // Test 3: Verify input/output signatures
            auto inputSig = instance->getInputSignature(instance->data);
            REQUIRE(inputSig != nullptr);
            REQUIRE(inputSig->count > 0);
            NVIGI_LOG_TEST_INFO("Input signature has %u slots", inputSig->count);
            
            auto outputSig = instance->getOutputSignature(instance->data);
            REQUIRE(outputSig != nullptr);
            REQUIRE(outputSig->count > 0);
            NVIGI_LOG_TEST_INFO("Output signature has %u slots", outputSig->count);
            
            // Test 4: Run a simple inference
            NVIGI_LOG_TEST_INFO("Testing inference execution...");
            {
                // Prepare input using STL helper - much simpler!
                ai::InferenceDataTextHelper promptHelper("Hello, template!");
                InferenceDataSlot inputSlots[] = {
                    {kTemplateAIInputPrompt, promptHelper}
                };
                InferenceDataSlotArray inputs = {1, inputSlots};
                
                // Plugin allocates output buffers internally, no need to provide them
                
                // Setup execution context with callback
                // NOTE: Callback must be a plain function pointer, not a lambda with captures
                static bool s_callbackTriggered = false;
                static std::string s_response;
                s_callbackTriggered = false;
                s_response.clear();
                
                auto callback = [](const InferenceExecutionContext* ctx, InferenceExecutionState state, void* userData) -> InferenceExecutionState {
                    s_callbackTriggered = true;
                    
                    // Read response from output if provided
                    if (ctx->outputs) {
                        const InferenceDataText* output{};
                        if (ctx->outputs->findAndValidateSlot(kTemplateAIOutputResponse, &output)) {
                            s_response = output->getUTF8Text();
                        }
                    }
                    
                    return kInferenceExecutionStateDone;
                };
                
                InferenceExecutionContext execCtx{};
                execCtx.instance = instance;
                execCtx.inputs = &inputs;
                execCtx.outputs = nullptr;  // Plugin allocates outputs
                execCtx.callback = callback;
                execCtx.callbackUserData = nullptr;
                
                // Execute (synchronous)
                auto evalResult = instance->evaluate(&execCtx);
                REQUIRE(evalResult == nvigi::kResultOk);
                REQUIRE(s_callbackTriggered);
                
                // Validate output
                REQUIRE(!s_response.empty());
                NVIGI_LOG_TEST_INFO("Response: %s", s_response.c_str());
            }
            
            // Clean up instance
            auto destroyResult = itemplate->destroyInstance(instance);
            REQUIRE(destroyResult == nvigi::kResultOk);
            NVIGI_LOG_TEST_INFO("Instance destroyed successfully");
        }
        else
        {
            // Expected to fail without real model files - this is OK for a template
            NVIGI_LOG_TEST_WARN("Instance creation failed (expected without model files): error code 0x%x", createResult);
        }
    }

    // Unload interface
    auto unloadResult = params.nvigiUnloadInterface(plugin::template_ai::kId, itemplate);
    REQUIRE(unloadResult == nvigi::kResultOk);
    NVIGI_LOG_TEST_INFO("Template AI basic test completed");
}

//! Test async execution (if your plugin supports it)
TEST_CASE("template_ai_async", "[template],[inference],[cpu],[async]")
{
    nvigi::ITemplateAI* itemplate{};
    auto loadResult = nvigiGetInterfaceDynamic(
        plugin::template_ai::kId, 
        &itemplate, 
        params.nvigiLoadInterface
    );
    
    REQUIRE(loadResult == nvigi::kResultOk);
    REQUIRE(itemplate != nullptr);

    NVIGI_LOG_TEST_INFO("Testing async execution with polled results...");
    
    // Get polled inference interface for async operations
    nvigi::IPolledInferenceInterface* ipolled{};
    auto polledResult = nvigiGetInterfaceDynamic(
        plugin::template_ai::kId,
        &ipolled,
        params.nvigiLoadInterface
    );
    REQUIRE(polledResult == nvigi::kResultOk);
    REQUIRE(ipolled != nullptr);
    
    // Setup creation parameters
    CommonCreationParameters common{};
    common.modelGUID = "{01234567-0123-0123-0123-0123456789AB}";  // Dummy GUID for testing
    common.utf8PathToModels = params.modelDir.c_str();
    common.numThreads = 1;
    
    TemplateAICreationParameters templateParams{};
    templateParams.chain(common);
    
    nvigi::InferenceInstance* instance = nullptr;
    auto createResult = itemplate->createInstance(templateParams, &instance);
    
        if (createResult == nvigi::kResultOk && instance != nullptr)
        {
            NVIGI_LOG_TEST_INFO("Instance created, testing async evaluation...");
            
            // Prepare input using STL helper
            ai::InferenceDataTextHelper promptHelper("Async test");
            InferenceDataSlot inputSlots[] = {{kTemplateAIInputPrompt, promptHelper}};
            InferenceDataSlotArray inputs = {1, inputSlots};
            
            // Plugin allocates outputs internally
            
            InferenceExecutionContext execCtx{};
            execCtx.instance = instance;
            execCtx.inputs = &inputs;
            execCtx.outputs = nullptr;  // Plugin allocates outputs
            execCtx.callback = nullptr;  // No callback = polled mode
        
        // Start async evaluation
        if (instance->evaluateAsync)
        {
            auto evalResult = instance->evaluateAsync(&execCtx);
            REQUIRE(evalResult == nvigi::kResultOk);
            NVIGI_LOG_TEST_INFO("Async evaluation started");
            
            // Poll for results using IPolledInferenceInterface
            InferenceExecutionState state = kInferenceExecutionStateInvalid;
            bool done = false;
            int pollCount = 0;
            
            while (!done && pollCount < 100)  // Max 100 polls
            {
                auto getResult = ipolled->getResults(&execCtx, false, &state);
                if (getResult == nvigi::kResultOk)
                {
                    if (state == kInferenceExecutionStateDone)
                    {
                        done = true;
                        
                        // Read response from outputs
                        std::string response;
                        if (execCtx.outputs) {
                            const InferenceDataText* output{};
                            if (execCtx.outputs->findAndValidateSlot(kTemplateAIOutputResponse, &output)) {
                                response = output->getUTF8Text();
                            }
                        }
                        
                        NVIGI_LOG_TEST_INFO("Async evaluation completed, response: %s", response.c_str());
                        
                        // Release results
                        auto releaseResult = ipolled->releaseResults(&execCtx, state);
                        REQUIRE(releaseResult == nvigi::kResultOk);
                    }
                }
                pollCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            REQUIRE(done);
        }
        
        itemplate->destroyInstance(instance);
        NVIGI_LOG_TEST_INFO("Async test completed");
    }
    else
    {
        NVIGI_LOG_TEST_WARN("Async test skipped (no model files available)");
    }

    // Unload interfaces
    auto unloadPolled = params.nvigiUnloadInterface(plugin::template_ai::kId, ipolled);
    REQUIRE(unloadPolled == nvigi::kResultOk);
    
    auto unloadResult = params.nvigiUnloadInterface(plugin::template_ai::kId, itemplate);
    REQUIRE(unloadResult == nvigi::kResultOk);
}

//! Test cancellation (if your plugin supports it)
TEST_CASE("template_ai_cancel", "[template],[inference],[cpu],[cancel]")
{
    nvigi::ITemplateAI* itemplate{};
    auto loadResult = nvigiGetInterfaceDynamic(
        plugin::template_ai::kId, 
        &itemplate, 
        params.nvigiLoadInterface
    );
    
    REQUIRE(loadResult == nvigi::kResultOk);
    REQUIRE(itemplate != nullptr);

    NVIGI_LOG_TEST_INFO("Testing cancellation...");
    
    // Get polled inference interface for async operations
    nvigi::IPolledInferenceInterface* ipolled{};
    auto polledResult = nvigiGetInterfaceDynamic(
        plugin::template_ai::kId,
        &ipolled,
        params.nvigiLoadInterface
    );
    REQUIRE(polledResult == nvigi::kResultOk);
    REQUIRE(ipolled != nullptr);
    
    // Setup creation parameters
    CommonCreationParameters common{};
    common.modelGUID = "{01234567-0123-0123-0123-0123456789AB}";  // Dummy GUID for testing
    common.utf8PathToModels = params.modelDir.c_str();
    common.numThreads = 1;
    
    TemplateAICreationParameters templateParams{};
    templateParams.chain(common);
    
    nvigi::InferenceInstance* instance = nullptr;
    auto createResult = itemplate->createInstance(templateParams, &instance);
    
        if (createResult == nvigi::kResultOk && instance != nullptr)
        {
            NVIGI_LOG_TEST_INFO("Instance created, testing cancellation...");
            
            // Prepare input using STL helper
            ai::InferenceDataTextHelper promptHelper("Cancel test");
            InferenceDataSlot inputSlots[] = {{kTemplateAIInputPrompt, promptHelper}};
            InferenceDataSlotArray inputs = {1, inputSlots};
            
            // Plugin allocates outputs internally
            
            InferenceExecutionContext execCtx{};
            execCtx.instance = instance;
            execCtx.inputs = &inputs;
            execCtx.outputs = nullptr;  // Plugin allocates outputs
            execCtx.callback = nullptr;
        
        // Start async evaluation
        if (instance->evaluateAsync && instance->cancelAsyncEvaluation)
        {
            auto evalResult = instance->evaluateAsync(&execCtx);
            REQUIRE(evalResult == nvigi::kResultOk);
            NVIGI_LOG_TEST_INFO("Async evaluation started, requesting cancellation...");
            
            // Cancel immediately
            auto cancelResult = instance->cancelAsyncEvaluation(&execCtx);
            NVIGI_LOG_TEST_INFO("Cancel result: 0x%x", cancelResult);
            
            // Try to get results (should be cancelled or done)
            InferenceExecutionState state = kInferenceExecutionStateInvalid;
            auto getResult = ipolled->getResults(&execCtx, true, &state);  // Wait
            NVIGI_LOG_TEST_INFO("Final state after cancel: %d", (int)state);
            
            if (getResult == nvigi::kResultOk)
            {
                ipolled->releaseResults(&execCtx, state);
            }
        }
        
        itemplate->destroyInstance(instance);
        NVIGI_LOG_TEST_INFO("Cancellation test completed");
    }
    else
    {
        NVIGI_LOG_TEST_WARN("Cancellation test skipped (no model files available)");
    }

    // Unload interfaces
    auto unloadPolled = params.nvigiUnloadInterface(plugin::template_ai::kId, ipolled);
    REQUIRE(unloadPolled == nvigi::kResultOk);
    
    auto unloadResult = params.nvigiUnloadInterface(plugin::template_ai::kId, itemplate);
    REQUIRE(unloadResult == nvigi::kResultOk);
}

//! Add more test cases as needed
//! Examples:
//! - Test with different input types (images, tensors, etc.)
//! - Test error handling (invalid inputs, missing models, etc.)
//! - Test performance benchmarks
//! - Test resource limits (memory, GPU usage, etc.)
//! - Test multi-instance scenarios

} // namespace template_ai
} // namespace nvigi
