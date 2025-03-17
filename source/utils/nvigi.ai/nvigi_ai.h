// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <type_traits>
#include "nvigi_cpu.h"

namespace nvigi
{

//! The current state of an inference execution
//! 
//! IMPORTANT: Interfaces should define custom states (if any) in their respective headers
//! 
//! The following formatting should be used:
//! 
//!     32bit STATE VALUE
//! | 31 ... 24  | 23 ....  0 |
//! | STATE CODE | FEATURE ID |
//! 
//! constexpr uint32_t kInferenceExecutionStateMyState = (myState << 24) | nvigi::plugin::$NAME::$BACKEND::$API::kId.crc24; // crc24 is auto generated and part of the PluginID
//! 
//! Up to 256 different states, these are the common ones
using InferenceExecutionState = uint32_t;
constexpr InferenceExecutionState kInferenceExecutionStateInvalid = 0 << 24;     // Inference in an invalid state (internal error, provided invalid output slots etc)
constexpr InferenceExecutionState kInferenceExecutionStateDone = 1 << 24;        // All done, no more data expected
constexpr InferenceExecutionState kInferenceExecutionStateCancel = 2 << 24;      // Canceled by the host
constexpr InferenceExecutionState kInferenceExecutionStateDataPending = 3 << 24; // Final data but more data expected
constexpr InferenceExecutionState kInferenceExecutionStateDataPartial = 4 << 24; // Partial data, subject to change, more partial or final data is expected, see below for more details

//! NOTE: Careful consideration should be taken when receiving partial data, for example here is one possible sequence:
//! 
//! [] A1 : Partial
//! [] A2 : Pending
//! [A2] B1 : Pending
//! [A2 B1] C1 : Partial
//! [A2 B1] C2 : Partial
//! [A2 B1] C3 : Pending
//! [A2 B1 C3] D1 : Done

//! Available backends, features could support only one or any combination
enum class InferenceBackendLocations : uint32_t
{
    eCPU = 0x01,
    eGPU = 0x02,
    eCloud = 0x04
};

NVIGI_ENUM_OPERATORS_32(InferenceBackendLocations)

//! Indicates where the actual data resides
enum class InferenceDataAllocator : uint32_t
{
    eCPU = 0x01,
    eCuda = 0x02,
    eDirectX = 0x03,
    eVulkan = 0x04
};

//! Used only if inference instance is specifically created for local execution on GPU
using CommandList = void;

//! Descriptor for inference data types
//! 
//! {A3C2792B-8EA3-4079-B6D8-EC2591332C2E}
struct alignas(8) InferenceDataDescriptor {
    InferenceDataDescriptor() {};
    NVIGI_UID(UID({ 0xa3c2792b, 0x8ea3, 0x4079,{ 0xb6, 0xd8, 0xec, 0x25, 0x91, 0x33, 0x2c, 0x2e } }), kStructVersion1)
    InferenceDataDescriptor(const char* _key, UID _dataType, bool _optional) : key(_key), dataType(_dataType), optional(_optional) {};
    //! The key identifying the data slot
    const char* key{};
    //! Maps directly to one of the *Data structures with the same GUID
    UID dataType{};
    //! Indicates if this slot is optional or not, defaults to false
    bool optional{};
    //! Specifies where data is actually allocated, defaults to CPU
    InferenceDataAllocator dataAllocator = InferenceDataAllocator::eCPU;

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(InferenceDataDescriptor)

// {30116404-792F-4099-B248-FD82B7AC67AE}
struct alignas(8) InferenceDataDescriptorArray {
    InferenceDataDescriptorArray() {};
    NVIGI_UID(UID({ 0x30116404, 0x792f, 0x4099,{ 0xb2, 0x48, 0xfd, 0x82, 0xb7, 0xac, 0x67, 0xae } }), kStructVersion1)
    InferenceDataDescriptorArray(size_t _count, InferenceDataDescriptor* _items) : count(_count), items(_items) {};
    //! Number of items in the list
    size_t count{};
    //! Data slot items
    const InferenceDataDescriptor* items{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(InferenceDataDescriptorArray)

// {A3560575-F9F7-4FBF-A382-22D6448C9D52}
struct alignas(8) InferenceDataSlot {
    InferenceDataSlot() {};
    NVIGI_UID(UID({ 0xa3560575, 0xf9f7, 0x4fbf,{ 0xa3, 0x82, 0x22, 0xd6, 0x44, 0x8c, 0x9d, 0x52 } }), kStructVersion1)

#ifndef __GNUC__
    NVIGI_DEPRECATED(InferenceDataSlot(const char* _key, void* _data) { key = _key; data = (BaseStructure*)_data; }, "Please use the explicit InferenceDataSlot constructor!")
#endif
    InferenceDataSlot(const char* _key, NVIGIParameter* _data) : key(_key),data(_data) {}

    //! The key identifying the data slot
    const char* key{};
    //! The InferenceData* structure containing the actual data.
    //! 
    //! Note that this can be constant or not, depending if data slot is input or output
    NVIGIParameter* data{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(InferenceDataSlot)

// {10FD9ADB-794E-4873-ACCD-AD8BD2AC00BF}
struct alignas(8) InferenceDataSlotArray {
    InferenceDataSlotArray() {};
    NVIGI_UID(UID({ 0x10fd9adb, 0x794e, 0x4873,{ 0xac, 0xcd, 0xad, 0x8b, 0xd2, 0xac, 0x0, 0xbf } }), kStructVersion1)
    InferenceDataSlotArray(size_t _count, InferenceDataSlot* _items) : count(_count), items(_items) {};
    //! Number of items in the list
    size_t count{};
    //! Data slot items
    const InferenceDataSlot* items{};
    //! Looks for the data slot with the given key and validates data type
    //! 
    //! Returns false if not found or if data type is incorrect, true otherwise.
    template<typename T>
    inline bool findAndValidateSlot(const char* key, const T** data) const
    {
        for (size_t i = 0; i < count && key && data; i++)
        {
            if (items[i].data != nullptr && !strcmp(key, items[i].key) && ((const BaseStructure*)items[i].data)->type == T::s_type)
            {
                *data = reinterpret_cast<const T*>(items[i].data);
                return true;
            }
        }
        return false;
    }
    template<typename T>
    inline bool findAndValidateSlot(const char* key, T** data)
    {
        for (size_t i = 0; i < count && key && data; i++)
        {
            if (items[i].data != nullptr && !strcmp(key, items[i].key) && ((const BaseStructure*)items[i].data)->type == T::s_type)
            {
                *data = reinterpret_cast<T*>(items[i].data);
                return true;
            }
        }
        return false;
    }

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(InferenceDataSlotArray)

//! Inference input and output data types
//! 
//! Can be expanded with new typed structures as needed (video, images etc.) at any point in time.

//! {15F34B49-63D3-4AE2-AF75-3DD37772DCB9}
struct alignas(8) InferenceDataText {
    InferenceDataText() {};
    NVIGI_UID(UID({ 0x15f34b49, 0x63d3, 0x4ae2,{ 0xaf, 0x75, 0x3d, 0xd3, 0x77, 0x72, 0xdc, 0xb9 } }), kStructVersion1)
    InferenceDataText(NVIGIParameter* _text) : utf8Text(_text) {};
    // Actual data, can point to CPUData, D3D12Data etc.
    NVIGIParameter* utf8Text{}; // UTF8 string
    inline const char* getUTF8Text() const
    {
        const CpuData* data{ castTo<CpuData>(utf8Text) };
        if (data) return static_cast<const char*>(data->buffer);
        return nullptr;
    }

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(InferenceDataText)

//! Audio Data Type
//!
enum AudioDataType : uint32_t
{
    ePCM,
    eRawFP32
};

//! {AEE09772-32FD-4E28-BE90-833175D12E12}
struct alignas(8) InferenceDataAudio {
    InferenceDataAudio() {};
    NVIGI_UID(UID({ 0xaee09772, 0x32fd, 0x4e28,{ 0xbe, 0x90, 0x83, 0x31, 0x75, 0xd1, 0x2e, 0x12 } }), kStructVersion1)
    InferenceDataAudio(NVIGIParameter* _audio) : audio(_audio) {};
    // Actual data, can point to CPUData, D3D12Data etc.
    NVIGIParameter* audio{};
    //! Note that most models are trained on 16000 samples mono audio
    //! hence that is our default. Most, if not all, inference instances
    //! will FAIL and return an error if audio is not using default sample rate.
    int bitsPerSample = 16;
    int samplingRate = 16000;
    int channels = 1;
    AudioDataType dataType{}; // PCM by default

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(InferenceDataAudio)


//! Interface InferenceDataByteArray
//!
//! {1A213DB7-568D-4BE3-BAD2-8EEBCF9AD44D}
struct alignas(8) InferenceDataByteArray {
    InferenceDataByteArray() {};
    NVIGI_UID(UID({ 0x1a213db7, 0x568d, 0x4be3,{ 0xba, 0xd2, 0x8e, 0xeb, 0xcf, 0x9a, 0xd4, 0x4d } }), kStructVersion1)
    InferenceDataByteArray(NVIGIParameter* _bytes) : bytes(_bytes) {};
    // Actual data, can point to CPUData, D3D12Data etc.
    NVIGIParameter* bytes{};
    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(InferenceDataByteArray)


//! Interface InferenceDataImage
//!
//! {05F87A5D-6F13-4DDF-B011-3B14CD35ECC9}
struct alignas(8) InferenceDataImage {
    InferenceDataImage() {};
    NVIGI_UID(UID({ 0x05f87a5d, 0x6f13, 0x4ddf,{ 0xb0, 0x11, 0x3b, 0x14, 0xcd, 0x35, 0xec, 0xc9 } }), kStructVersion1)
        InferenceDataImage(NVIGIParameter* _bytes, int _h, int _w, int _c) : bytes(_bytes), h(_h), w(_w), c(_c) {};
    // Actual data, can point to CPUData.
    NVIGIParameter* bytes{};
    int h;
    int w;
    int c;
    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(InferenceDataImage)
//! TODO: ADD NEW INFERENCE DATA TYPES HERE

struct InferenceExecutionContext;

//! Inference callback
//! 
//! Provides an array of InferenceData* structures containing the output(s)
//! 
//! @param context A pointer to an execution context containing input/output data for the inference pass
//! @param state Current execution state
//! @param userData User data provided in the execution context (if any, can be null)
//! 
//! To interrupt inference execution callback can return InferenceExecutionState::eCancel, otherwise provided state should be returned.
//! 
//! IMPORTANT: Provided data is ONLY VALID WITHIN THE CALLBACK EXECUTION TIME FRAME.
//! 
//! This method is NOT thread safe.
using PFun_nvigiInferenceCallback = nvigi::InferenceExecutionState(const InferenceExecutionContext* context, nvigi::InferenceExecutionState state, void* userData);

struct InferenceInstance;

//! Inference execution context
//! 
//! Combines inputs/outputs and other needed items to run inference
//! 
//! IMPORTANT: Must be valid until inference is done (normally signalled by the 'kInferenceExecutionStateDone' state in the callback)
//!
// {75B12C0B-5D88-48B0-8E52-D2E8B4684EDA}
struct alignas(8) InferenceExecutionContext {
    InferenceExecutionContext() {};
    NVIGI_UID(UID({ 0x75b12c0b, 0x5d88, 0x48b0,{ 0x8e, 0x52, 0xd2, 0xe8, 0xb4, 0x68, 0x4e, 0xda } }), kStructVersion1)
    //! Instance we are using to run the inference
    InferenceInstance* instance{};
    //! Expected inputs, mandatory inputs must be included and must match instance's input signature
    InferenceDataSlotArray* inputs{};
    //! Callback to receive inference outputs
    PFun_nvigiInferenceCallback* callback{};
    //! OPTIONAL - any runtime parameters this instance might need (can chain parameters as needed)
    NVIGIParameter* runtimeParameters{};
    //! OPTIONAL - user callback data
    void* callbackUserData{};
    //! OPTIONAL - Expected outputs, outputs must match instance's output signature.
    //! 
    //! IMPORTANT: If a specific data slot is left as null the backend will be responsible for allocating it.
    InferenceDataSlotArray* outputs{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(InferenceExecutionContext)

using InferenceInstanceData = void;

//! Inference instance 
//! 
//! Contains in/out signatures and the inference execution method
//! 
// {AD9DC29C-0A89-4A4E-B900-A7183B48336E}
struct alignas(8) InferenceInstance {
    //! Allow existing code to downgrade version as needed if not planning to implement V2+
    InferenceInstance(uint32_t version = kStructVersion2) { _base.version = version; };
    NVIGI_UID(UID({ 0xad9dc29c, 0xa89, 0x4a4e,{ 0xb9, 0x0, 0xa7, 0x18, 0x3b, 0x48, 0x33, 0x6e } }), kStructVersion2)

    //! Instance data, must be passed as input to all functions below
    InferenceInstanceData* data{};

    //! Returns feature id e.g. nvigi::plugin::gpt::ggml.cuda etc.
    //! 
    //! This method is thread safe.
    PluginID(*getFeatureId)(InferenceInstanceData* data){};

    //! Returns an array of descriptors for the input data expected by this instance
    //! 
    //! This method is thread safe.
    const InferenceDataDescriptorArray* (*getInputSignature)(InferenceInstanceData* data){};

    //! Returns an array of descriptors for the output data provided by this instance
    //! 
    //! This method is thread safe.
    const InferenceDataDescriptorArray* (*getOutputSignature)(InferenceInstanceData* data){};

    //! Evaluates provided execution context synchronously
    //!
    //! * Callback MUST be provided to retrieve results
    //! * Execution context MUST be valid until the 'InferenceExecutionStateDone' state is received
    //! * This method can return nvigi::ResultNoImplementation
    //! 
    //! This method is NOT thread safe.
    nvigi::Result(*evaluate)(nvigi::InferenceExecutionContext* execCtx){};

    //! V2

    //! Evaluates provided execution context asynchronously
    //!
    //! * If callback is provided it will be called from another thread managed by NVIGI
    //! * If callback is NOT provided the 'IPolledInferenceInterface' must be used to retrieve results
    //! * Execution context MUST be valid until the 'InferenceExecutionStateDone' state is received
    //! * This method can return nvigi::ResultNoImplementation
    //! 
    //! This method is NOT thread safe.
    nvigi::Result(*evaluateAsync)(nvigi::InferenceExecutionContext* execCtx){};

    //! NEW MEMBERS GO HERE, BUMP THE VERSION IN NVIGI_UID AND CONSTRUCTOR!
};

NVIGI_VALIDATE_STRUCT(InferenceInstance)

//! Inference interface
//! 
//! {F0038A35-EEC2-4230-811D-58C9498671BC}
struct alignas(8) InferenceInterface {
    InferenceInterface() {};
    NVIGI_UID(UID({ 0xf0038a35, 0xeec2, 0x4230,{ 0x81, 0x1d, 0x58, 0xc9, 0x49, 0x86, 0x71, 0xbc } }), kStructVersion1)

    //! Creates new instance
    //!
    //! Call this method to create instance for the ASR model
    //! 
    //! @param params Pointer to the creation parameters
    //! @param instance Returned new instance (null on error)
    //! @return nvigi::kResultOk if successful, error code otherwise (see NVIGI_result.h for details)
    //!
    //! Example:
    //! 
    //! nvigi::XXXCreationParameters params{};
    //! ... fill in params
    //! 
    //! nvigi::InferenceInstance* instance{}; 
    //! interface->createInstance(params /* leveraging helper operators (translates to &params._base) */, &instance)
    //! 
    //! This method is NOT thread safe.
    nvigi::Result(*createInstance)(const nvigi::NVIGIParameter* params, nvigi::InferenceInstance** instance);

    //! Destroys existing instance
    //!
    //! Call this method to destroy an existing ASR instance
    //! 
    //! @param instance Instance to destroy (ok to destroy null instance)
    //! @return nvigi::kResultOk if successful, error code otherwise (see NVIGI_result.h for details)
    //!
    //! This method is NOT thread safe.
    nvigi::Result(*destroyInstance)(const nvigi::InferenceInstance* instance);

    //! Returns model information
    //!
    //! Call this method to find out about the available models and their capabilities and requirements.
    //! 
    //! @param modelInfo Pointer to a structure containing supported model information
    //! @param params Optional pointer to the setup parameters (can be null)
    //! @return nvigi::kResultOk if successful, error code otherwise (see NVIGI_result.h for details)
    //!
    //! NOTE: It is recommended to use the templated 'getCapsAndRequirements' helper (see below in this header).
    //! 
    //! Here is an example:
    //! 
    //! nvigi::IXXXInterface* ixxx; // obtained via 'nvigiGetInterface'
    //!
    //! nvigi::CommonCreationParameters common{};
    //! nvigi::XXXCreationParameters params{};
    //! params.common = &common;
    //! common.utf8PathToModels = myPathTomodelDir;
    //! 
    //! nvigi::XXXCapabilitiesAndRequirements* caps{};
    //! nvigi::getCapsAndRequirements(ixxx, params, &caps);
    //! 
    //! This method is NOT thread safe.
    nvigi::Result(*getCapsAndRequirements)(nvigi::NVIGIParameter** modelInfo, const nvigi::NVIGIParameter* params);

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(InferenceInterface)

//! Interface 'IPolledInferenceInterface'
//!
//! {203A2E67-9EA2-47FC-B932-7A3965E608D4}
struct alignas(8) IPolledInferenceInterface
{
    IPolledInferenceInterface() { };
    NVIGI_UID(UID({ 0x203a2e67, 0x9ea2, 0x47fc,{0xb9, 0x32, 0x7a, 0x39, 0x65, 0xe6, 0x08, 0xd4} }), kStructVersion1)

    //! Polls (or blocks) waiting for results to be available
    //!
    //! This method should only be called after calling evaluate with a null callback ptr.
    //! 
    //! @param execCtx The execution context passed to evaluate
    //! @param wait Whether or not to wait until data is available.  If true, then we will block until there
    //! is data or there is an error
    //! @param state Pointer to a state variable to be filled in with the inference state that would normally be
    //! passed to the callback; indicates whether more data is expected (i.e. the app should query for more)
    //! @return nvigi::kResultOk if successful and there is data available.  If wait is false and no data is pending,
    //! will return kResultNotReady.  This is success, but indicates that the host must call again to get data.
    //! Error code otherwise (see NVIGI_result.h for details)
    //!
    //! This method is thread safe.
    nvigi::Result(*getResults)(nvigi::InferenceExecutionContext* execCtx, bool wait, nvigi::InferenceExecutionState* state);

    //! Indicates that the host is done using the pending data in the InferenceExecutionContext
    //! 
    //! The host must call this before receiving additional data.  Once this call is made, the host must not
    //! continue using the output data in the InferenceExecutionContext (until the next getResults)
    //! 
    //! @param execCtx The execution context passed to evaluate
    //! @param state The state of inference following data processing.  This should either be the state returned
    //! to the host by getResults or else eCancel if the host wishes to prematurely halt this evaluate seqence
    //! @return nvigi::kResultOk if successful.  Error code otherwise (see NVIGI_result.h for details)
    //!
    //! This method is thread safe.
    nvigi::Result(*releaseResults)(nvigi::InferenceExecutionContext* execCtx, nvigi::InferenceExecutionState state);
};

NVIGI_VALIDATE_STRUCT(IPolledInferenceInterface)


//! Generic creation parameters - apply to all plugins
//! 
//! NOTE: All allocations are managed by the plugin in question and are valid until that plugin is unloaded
//! 
//! {CC8CAD78-95F0-41B0-AD9C-5D6995988B23}
struct alignas(8) CommonCreationParameters {
    CommonCreationParameters() {};
    NVIGI_UID(UID({ 0xcc8cad78, 0x95f0, 0x41b0,{ 0xad, 0x9c, 0x5d, 0x69, 0x95, 0x98, 0x8b, 0x23 } }), kStructVersion1)
    //! Relevant only for CPU backends, should be set to 1 for all GPU based backends
    int32_t numThreads = 1;
    //! Now much VRAM is allowed to use
    size_t vramBudgetMB = SIZE_MAX;
    //! Must be in the registry format "{XXX}", for example "{175C5C5D-E978-41AF-8F11-880D0517C524}"
    const char* modelGUID{};
    //! Path to the model repository
    const char* utf8PathToModels{};
    //! Optional - path to the additional models downloaded on the system (if any)
    const char* utf8PathToAdditionalModels{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(CommonCreationParameters)

//! Model flags
//! 
//1 NOTE: Can be custom and declared in plugin headers, please see nvigi::Result to find out how to make custom/unique per plugin flags
using ModelFlags = uint32_t;

constexpr ModelFlags kModelFlagRequiresDownload = 1; // Model needs files downloaded in order to run inference

//! Generic caps and requirements - apply to all plugins
//! 
//! NOTE: All allocations are managed by the plugin in question and are valid until that plugin is unloaded
//! 
//! {1213844E-E53B-4C46-A303-741789060B3C}
struct alignas(8) CommonCapabilitiesAndRequirements {
    CommonCapabilitiesAndRequirements() {};
    NVIGI_UID(UID({ 0x1213844e, 0xe53b, 0x4c46,{ 0xa3, 0x3, 0x74, 0x17, 0x89, 0x6, 0xb, 0x3c } }), kStructVersion2)
    size_t numSupportedModels{};
    const char** supportedModelGUIDs{};
    const char** supportedModelNames{};
    const size_t* modelMemoryBudgetMB{}; //! IMPORTANT: Provided if known, can be 0 if fully dynamic and depends on inputs or if model is cloud only
    InferenceBackendLocations supportedBackends{};

    //! V2
    const ModelFlags* modelFlags{}; // Various flags, can be custom per model and defined in plugin headers

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(CommonCapabilitiesAndRequirements)

//! Interface 'CloudCapabilities'
//!
//! NOTE: All allocations are managed by the plugin in question and are valid until that plugin is unloaded
//! 
//! {4AD74E46-3272-4FE3-813E-D22C8BBAF0D5}
struct alignas(8) CloudCapabilities
{
    CloudCapabilities() { };
    NVIGI_UID(UID({ 0x4ad74e46, 0x3272, 0x4fe3,{0x81, 0x3e, 0xd2, 0x2c, 0x8b, 0xba, 0xf0, 0xd5} }), kStructVersion1)

    //! URL to connect to
    const char* url{};
    //! JSON request template which is sent to the server after appropriate input slots are applied
    const char* jsonRequestBody{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(CloudCapabilities)

template<typename T>
nvigi::Result getCapsAndRequirements(nvigi::InferenceInterface* _interf, const nvigi::NVIGIParameter* params, T** capsAndReqs)
{
    nvigi::NVIGIParameter* _info;
    if (NVIGI_FAILED(error, _interf->getCapsAndRequirements(&_info, params))) return error;
    // Caps can be chained so search and check
    *capsAndReqs = findStruct<T>(_info);
    return *capsAndReqs == nullptr ? nvigi::kResultInvalidParameter : nvigi::kResultOk;
}

//! StreamingMode controls the various streaming modes in which the plugin can operate.
enum class StreamingMode
{
    //! The plugin expects input in streaming mode
    //! and also returns output in streaming mode.
    eStreamingModeInputOutput = 0,

    //! The plugin expects input in streaming mode,
    //! however, the output is provided only after the input stream has concluded.
    eStreamingModeInputOnly,

    //! The plugin expects a single input (non-streaming)
    //! and returns output in streaming mode.
    eStreamingModeOutputOnly
};

//! StreamSignal indicates the different stages of streaming inputs.
enum class StreamSignal
{
    //! Signals a plugin to initiate new input stream.
    //! Should be called only once per stream
    eStreamSignalStart = 0,

    //! Signals a plugin that a data buffer is sent for an initiated stream.
    //! Can be called multiple times for a stream.
    eStreamSignalData,

    //! Signals a plugin to stop an input stream and return the final output.
    //! Should be called only once per stream
    eStreamSignalStop
};

//! Interface 'StreamingParameters'
//!
//! {1D2B9BB6-3F78-417E-9B26-265096FF865F}
struct alignas(8) StreamingParameters
{
    StreamingParameters() { };
    NVIGI_UID(UID({ 0x1d2b9bb6, 0x3f78, 0x417e,{0x9b, 0x26, 0x26, 0x50, 0x96, 0xff, 0x86, 0x5f} }), kStructVersion1)

    //! Indicates the streaming configuration that a plugin should operate in.
    //! Should be chained with the nvigi::InferenceExecutionContext while calling 
    //! evaluate()/evaluateAsync().

    //! v1 members go here

    //! StreamingMode defines the type of streaming user wants to enable for the plugin.
    //! Default is streaming both input and output.
    StreamingMode mode {};

    //! StreamSignal defines the type of buffer input being sent along with the evaluate call.
    StreamSignal signal {};
    
    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(StreamingParameters)

}