
# NVIGI - Programming Guide For Local And Cloud Inference

This guide primarily focuses on the general use of the AI plugins performing local or cloud inference. Before reading this please read [the general programming guide](ProgrammingGuide.md)

> **IMPORTANT**: This guide might contain pseudo code, for the up to date implementation and source code which can be copy pasted please see the [basic sample](../source/samples/nvigi.basic/basic.cpp)

## Version 1.0.0 Release

## Table of Contents
- [Introduction](#introduction)
- [Key Concepts](#key-concepts)
- [Model Repository](#model-repository)
- [Obtaining Models](#obtaining-models)
  - [Local Execution](#local-execution)
  - [Remote Execution](#remote-execution)
- [Obtaining Plugin Capabilities and Requirements](#common-and-custom-capabilities-and-requirements)
- [Creation Parameters](#creation-parameters)
  - [Common and Custom](#common-and-custom)
  - [Cloud Plugins](#cloud-plugins)
  - [Local Plugins](#local-plugins)
  - [Compute In Graphics (CIG)](#compute-in-graphics-cig)
- [Data Types](#data-types)
- [Inputs Slots](#input-slots)
- [Execution Context](#execution-context)
  - [Synchronous vs Asynchronous Execution](#blocking-vs-asynchronous-evaluation)
- [Obtaining Results](#obtaining-results)
  - [Callback Approach](#callback-approach)
  - [Polling Approach](#polling-approach)
  
## INTRODUCTION

NVIGI AI plugins provide unified API for both local and cloud inference. This ensures easy transition between local and cloud services and full flexibility. The AI API is located in `nvigi_ai.h` header.

## Key Concepts

* Each AI plugin implements certain feature with the specific backend and underlying API, here are some examples:
    *  nvigi.plugin.gpt.ggml.cuda -> implements GPT feature using **GGML backend and CUDA API for local execution**
    *  nvigi.plugin.gpt.cloud.rest -> implements GPT feature using **CLOUD backed and REST API for remote execution**
* Models used by the AI **local** plugins are stored in a specific NVIGI model repository (more details in sections below)  
* All AI plugins implement and export the same generic interface `InferenceInterface`
* AI plugins can act as "parent" plugins encapsulating multiple features but still exposing the same unified API
* `InferenceInterface` is used to obtain capabilities and requirements (VRAM etc.) and also create and destroy instance(s)
* Each created instance is represented by the `InferenceInstance` interface
* `InferenceInstance` contains generic API for running the inference given the `InferenceExecutionContext` which contains input slots, callbacks to get results, runtime parameters etc.
* All inputs and outputs use generic `data slots`, like for example `InferenceDataByteArray`, `InferenceDataText`, `InferenceDataAudio` etc.
* Host application obtains results (output slots) either via registered callback or polling mechanism

## Model Repository

For consistency, all NVIGI AI inference plugins store their models using the following directory structure:

```text
$ROOT/
├── nvigi.plugin.$name.$backend
    └── {MODEL_GUID}
        └── files
```

Here is an example structure for the existing NVIGI plugins and models:

```text
$ROOT/
├── nvigi.plugin.gpt.ggml
│   └── {175C5C5D-E978-41AF-8F11-880D0517C524}
│       ├── gpt-7b-chat-q4.gguf
│       └── nvigi.model.config.json
└── nvigi.plugin.asr.ggml
    └── {5CAD3A03-1272-4D43-9F3D-655417526170}
        ├── ggml-asr-small.gguf
        └── nvigi.model.config.json
```

> NOTE: Each plugin can have as many different models (GUIDs) as needed

Each model must provide `nvigi.model.config.json` file containing:

* model's card (name, file extension(s) and instructions on how to obtain it)
* vram consumption (if local)
* other model specific information (e.g. prompt template for LLM models)

Here is an example from `nvigi.plugin.gpt.ggml`, model information for `llama3.1 8B instruct`:

```json
{
  "name": "llama-3.1-8b-instruct",
  "vram": 5124,
  "prompt_template": [
    "<|begin_of_text|>",
    "<|start_header_id|>system<|end_header_id|>\n\n",
    "$system",
    "\n<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n",
    "$user",
    "\n<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n",
    "$assistant"
  ],
  "turn_template": [
    "<|start_header_id|>user<|end_header_id|>\n\n",
    "$user",
    "\n<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n"
  ],
  "model":
  {
      "ext" : "gguf",
      "notes": "Must use .gguf extension and format, model(s) can be obtained for free on huggingface",
      "file":
      {
      "command": "curl -L -o llama-3.1-8b-instruct.gguf 'https://huggingface.co/ArtyLLaMa/LLaMa3.1-Instruct-8b-GGUF/resolve/main/Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf?download=true'"
      }
  }
}
```
> NOTE: Some models will only be accessible via NGC and require special token for access. These models are normally licensed differently and require developers to contact NVIDIA to obtain access.

An optional `configs` subfolder, with the identical subfolder structure, can be added under `$ROOT` to provide `nvigi.model.config.json` overrides as shown below:

```text
$ROOT/
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
> This allows quick way of modifying only model config settings in JSON without having to reupload the entire model which can be several GBs in size

## Obtaining Models

### Local Execution

As mentioned in the above section, each model configuration JSON contains model card with instructions on how to obtain each model.

To add new local model to the model repository please follow these steps:

* Generate new GUID in the `registry format`, like for example `{2467C733-2936-4187-B7EE-B53C145288F3}`
* Create new folder under `nvigi.plugins.$feature.$backend` and name it the above GUID (like for example `{2467C733-2936-4187-B7EE-B53C145288F3}`)
* Copy an existing `nvigi.model.config.json` from already available models
* Modify `name` field in the JSON to match your model
* Modify `vram` field to match your model's VRAM requirements in MB (NVIGI logs VRAM consumption per instance in release/debug build configurations)
* Modify any custom model specific bits (for example, each GPT/LLM model requires specific prompt setup)
* Download model from Hugging Face or other source (NGC etc.)
* Unzip any archives and ensure correct extension(s) are used (for example if using GGML backend all models must use `.gguf` extension)

> NOTE: Please keep in mind that **some latest models might not work** with the backends provided in this version of NVIGI SDK hence plugin(s) need to be upgraded.

#### Prompt Templates for LLM Models

Each LLM required correct prompt template. These templates are stored in the above mentioned model configuration JSON and look something like this:
```json
"prompt_template": [
    "<|begin_of_text|>",
    "<|start_header_id|>system<|end_header_id|>\n\n",
    "$system",
    "\n<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n",
    "$user",
    "\n<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n",
    "$assistant"
  ],
  "turn_template": [
    "<|start_header_id|>user<|end_header_id|>\n\n",
    "$user",
    "\n<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n"
  ],
```

To make these prompt templates, one needs to find the jinja (very simplistic programming language) that the model uses to format itself (or more trivially, look for "<model name> prompt template" in google), and see what it wants for the various markers.
For example, SmolLM2, we find this: https://ollama.com/library/smollm2:360m/blobs/d502d55c1d60

Next step is to find where the system, user, and assistant start and stop markers are
```txt
{{- if .System }}<|im_start|>system
{{ .System }}<|im_end|>
{{- if eq .Role "user" }}<|im_start|>user
{{ .Content }}<|im_end|>
{{ else if eq .Role "assistant" }}<|im_start|>assistant
{{ .Content }}{{ if not $last }}<|im_end|>
```
Now we know the LLM wants something that looks like this:
```txt
<|im_start|>system
You are a helpful Ai agent...
<|im_end|>
<|im_start|>user
Can you tell me a story to help my daughter go to sleep?
<|im_end|>
<|im_start|>assistant
Sure, Once upon a time...
<|im_end|>
```
Hence we make the prompt template like this
```json
  "prompt_template": [
      "<|im_start|>system\n",
      "$system",
      "<|im_end|>\n\n",
      "<|im_start|>user\n",
      "$user",
      "<|im_end|>\n<|im_start|>assistant\n",
      "$assistant"
  ],
```
> NOTE: IGI does NOT automatically add newlines, special care must be taken to include `\n` correctly

IGI uses the "prompt_template" on either the *first* chat message or if we're using the LLM in instruct mode (no chat history). For the chat mode, the "turn_template" is required for all remaining turns.
This ensures that the last assistant message is terminated properly (if necessary, sometimes the model auto puts this in), and then replicating the user/assistant turn from the prompt template. 

So turn template for SMOLLM 2 becomes
```json
  "turn_template": [
      "<|im_start|>user\n",
      "$user",
      "<|im_end|>\n<|im_start|>assistant\n",
      "$assistant"
  ]
```

### Remote Execution

To add new remote (cloud) model to the model repository please follow these steps:

* Generate new GUID in the `registry format`, like for example `{2467C733-2936-4187-B7EE-B53C145288F3}`
* Create new folder under `nvigi.plugins.gpt.cloud` and name it the above GUID (like for example `{2467C733-2936-4187-B7EE-B53C145288F3}`)
* Copy an existing `nvigi.model.config.json` from already available models (for example from `model/llama-3.2-3b` located at `$ROOT\nvigi.plugin.gpt.cloud\{01F43B70-CE23-42CA-9606-74E80C5ED0B6}\nvigi.model.config.json`)
* Modify `name` field in the JSON to match your model
* Modify `request_body` field to match your model's JSON body for the REST request

If using [NVIDIA NIM APIs](https://build.nvidia.com) search and navigate to the model you want to use then copy paste request into the above mentioned JSON. For example, when selecting [llama-3_1-70b-instruct](https://build.nvidia.com/meta/llama-3_1-70b-instruct) the completion code in Python looks like this:

```python
completion = client.chat.completions.create(
  model="meta/llama-3.1-70b-instruct",
  messages=[{"role":"user","content":"Write a limerick about the wonders of GPU computing."}],
  temperature=0.2,
  top_p=0.7,
  max_tokens=1024,
  stream=True # NOTE: This is NOT supported by the current version of GPT cloud plugin so it must be set to false (see below)
)
```
which then translates to the `nvigi.model.config.json` file looking like this:

```json
{
  "name": "llama-3.1-70b-instruct",
  "vram": 0,
  "request_body": {
    "model": "meta/llama-3.1-70b-instruct",
    "messages": [
      {"role":"system","content":"$system"},
      {"role":"user","content":"$user"}
    ],
    "temperature": 0.2,
    "top_p": 0.7,
    "max_tokens": 1024,
    "stream": false
  }
}
```
> IMPORTANT: Current version of `nvigi.plugin.gpt.cloud.rest` does not support cloud streaming so that option needs to be set to `false`

For 3rd party cloud solutions, like for example OpenAI, please have a look at the model with GUID `{E9102ACB-8CD8-4345-BCBF-CCF6DC758E58}` which contains configuration for `gpt-3.5-turbo`. This model can be used the exact same way as any other NIMS based model provided by NVIDIA and also can be used as a template to clone other models which are based on the OpenAI API (just don't forget to generate new GUID). Here is the config file:

```json
{
  "name": "openai/gpt-3.5-turbo",
  "vram": 0,
  "request_body": {
    "model": "gpt-3.5-turbo",
    "messages": [
      {"role":"system","content":"$system"},
      {"role":"user","content":"$user"}
    ],
    "temperature": 0.2,
    "top_p": 0.7,
    "max_tokens": 1024,
    "stream": false
  }
}
```

> IMPORTANT: If your custom model does not follow the OpenAI API standard for the REST cloud requests, the `request_body` section must be modified to match specific server requirements.

## Common And Custom Capabilities and Requirements

If host application needs to find out more information about the available models in the above mentioned repository, the `InferenceInterface` provides `getCapsAndRequirements` API which returns feature specific (if any) caps and requirements and common caps and requirements shown below:

```cpp
//! Generic caps and requirements - apply to all plugins
//! 
//! {1213844E-E53B-4C46-A303-741789060B3C}
struct alignas(8) CommonCapabilitiesAndRequirements {
    CommonCapabilitiesAndRequirements() {};
    NVIGI_UID(UID({ 0x1213844e, 0xe53b, 0x4c46,{ 0xa3, 0x3, 0x74, 0x17, 0x89, 0x6, 0xb, 0x3c } }), kStructVersion1);
    size_t numSupportedModels{};
    const char** supportedModelGUIDs{};
    const char** supportedModelNames{};
    size_t* modelMemoryBudgetMB{}; //! IMPORTANT: Provided if known, can be 0 if fully dynamic and depends on inputs
    InferenceBackendLocations supportedBackends{};

    //! NEW MEMBERS GO HERE, BUMP THE VERSION!
};
```

Each AI interface provides a generic API (located in `nvigi_ai.h`) used to obtain `CommonCapabilitiesAndRequirements` and any custom capabilities and requirements (if any).

```cpp
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
//! This method is NOT thread safe.
nvigi::Result(*getCapsAndRequirements)(nvigi::NVIGIParameter** modelInfo, const nvigi::NVIGIParameter* params);
```

When obtaining information about specific model(s) host application can provide `CommonCreationParameters` (see next section for more details) as input so there are several options based on selected backend etc.

### Local Plugins

* provide specific model GUID and VRAM budget and check if that particular model can run within the budget
* provide null model GUID and VRAM budget to get a list of models that can run within the budget
* provide null model GUID and "infinite" (MAX_INT) VRAM budget to get a list of ALL models

### Cloud Plugins

* provide specific model GUID to obtain CloudCapabilities which include URL and other information for the endpoint used by the model
* provide null model GUID to get a list of ALL models (CloudCapabilities in this case will NOT provide any info)

If specific feature has custom capabilities and requirements the common ones will always be either chained together or returned as a pointer within the custom caps. In addition, if feature is a pipeline (parent plugin encapsulating two or more plugins) it will return caps and requirements for ALL enclosed plugins.  These can be queryied from what is returned by the parent plugin's `getCapsAndRequirements` using `nvigi::findStruct<structtype>(rootstruct)`.

> IMPORTANT: Always have a look at plugin's public header `nvigi_$feature.h` to find out if plugin has custom caps etc.

## Creation Parameters

### Common and Custom

Before creating an instance, host application must specify certain properties like which model should be used, how much VRAM is available, how many CPU threads etc. Similar to the previous section, plugins can have custom creation parameters but they always have to provide common ones. Here is how common creation parameters look like:

```cpp
//! Generic creation parameters - apply to all plugins
//! 
//! {CC8CAD78-95F0-41B0-AD9C-5D6995988B23}
struct alignas(8) CommonCreationParameters {
    CommonCreationParameters() {};
    NVIGI_UID(UID({ 0xcc8cad78, 0x95f0, 0x41b0,{ 0xad, 0x9c, 0x5d, 0x69, 0x95, 0x98, 0x8b, 0x23 } }), kStructVersion1);
    int32_t numThreads{};
    size_t vramBudgetMB = SIZE_MAX;
    const char* modelGUID{};
    const char* utf8PathToModels{};
    //! Optional - additional models downloaded on the system (if any)
    const char* utf8PathToAdditionalModels{};

    //! NEW MEMBERS GO HERE, BUMP THE VERSION!
};
```

Plugins cannot create an instance unless they know where NVIGI models repository is located, what model GUID to use, how much VRAM is OK to use etc. All this information is provided in common creation parameters. Each plugin can define custom ones, this obviously depends on what parameters are needed to create an instance.

> NOTE:
> Same model GUID can be used by different plugins if they are implementing different backends, for example Whisper GGUF model can be loaded by the `nvigi.plugin.asr.ggml.cuda` and `nvigi.plugin.asr.ggml.cpu` plugins

### Cloud Plugins

When it comes to cloud plugins they can use two different protocols, REST and gRPC. In addition to the above mentioned common creation parameters cloud plugin require either `RESTParameters` or `RPCParameters` to be chained together with the common ones. Here is an example:

```cpp

//! Obtain GPT CLOUD REST interface
nvigi::IGeneralPurposeTransformer* igpt{};
nvigiGetInterface(plugin::gpt::cloud::rest::kId, &igpt);

//! Common parameters
CommonCreationParameters common{};
common.utf8PathToModels = params.modelDir.c_str();
common.modelGUID = "{E9102ACB-8CD8-4345-BCBF-CCF6DC758E58}"; // gpt-3.5-turbo

//! GPT parameters
nvigi::GPTCreationParameters gptCreationParams{};
// TODO: Set some GPT specific items here
gptCreationParams.chain(common);

//! Cloud parameters
RESTParameters cloudParams{};
std::string token;
getEnvVar("OPENAI_TOKEN", token);
cloudParams.url = "https://api.openai.com/v1/chat/completions";
cloudParams.authenticationToken = token.c_str();
cloudParams.verboseMode = true;

gptCreationParams.chain(cloudParams); // Chaining cloud parameters!

nvigi::InferenceInstance* instance{};
igpt->createInstance(gptCreationParams, &instance);
```

### Local Plugins

With local plugins there are few key points to consider when selecting which plugin to use:

* Selecting backend and API
* How much VRAM can be used?
* What is the expected latency?

For example fully GPU bottle-necked application or application which does not have enough VRAM left could do the following and run GPT inference completely on the CPU:

```cpp
//! Obtain GPT GGML CPU interface
nvigi::IGeneralPurposeTransformer* igpt{};
nvigiGetInterface(plugin::gpt::ggml::cpu::kId, &igpt);
```

On the other hand, CPU bottle-necked application could do the following and run GPT inference completely on the GPU:

```cpp
//! Obtain GPT GGML CUDA interface
nvigi::IGeneralPurposeTransformer* igpt{};
nvigiGetInterface(plugin::gpt::ggml::cuda::kId, &igpt);
```

#### Compute In Graphics (CIG)

When selecting **local inference plugins which utilize CUDA API** it is **essential to enable CIG if application is using D3D12 or Vulkan rendering APIs**. This ensures optimal performance and minimizes latency for local inference execution. CIG is enabled via special interface and here are the steps:

```cpp
// Obtain special HW interface for CUDA
nvigi::IHWICuda* icig = nullptr;
nvigiGetInterface(nvigi::plugin::hwi::cuda::kId, &icig);

// Specify your device and queue information
nvigi::D3D12Parameters d3d12Params;
d3d12Params.device = <your ID3D12Device*>
d3d12Params.queue = <your (graphics) ID3D12CommandQueue*>

// Chain the D3D12 parameters to any creation parameters when generating local instance

// For example, local GPT using GGML backed and CUDA API (NOT CPU)
gptCreationParams.chain(d3d12Parameters);
```

## Data Types

Here are the common inference data types as declared in `nvigi_ai.h`:

* `InferenceDataText`
* `InferenceDataAudio`
* `InferenceDataTextByteArray`

The underlying raw data can be either on the CPU or GPU so it is represented by the types declared in `nvigi_cpu.h`, `nvigi_d3d12.h`, `nvigi_vulkan.h` etc.

* `CpuData`
* `D3D12Data`
* `VulkanData`

For example, this is how one would setup audio data located on the CPU:

```cpp
std::vector<int16> my_audio = recordMyAudio();
nvigi::CpuData _audio{my_audio.size() * sizeof(int16_t), my_audio.data()}; 
nvigi::InferenceDataAudio audioData{_audio};
```

## Input Slots

Once we have our instance we need to provide input data slots that match the input signature for the given instance. The `InferenceInstance` provides and API to obtain input and output signatures at runtime but they can also be obtained from plugin's headers and source code. In this guide we will use the Automated Speech Recognition (ASR) as an example.

```cpp
//! Audio data slot is coming from our previous step
std::vector<nvigi::InferenceDataSlot> slots = { {nvigi::kASRDataSlotAudio, &audioData} };
nvigi::InferenceDataSlotArray inputs = { slots.size(), slots.data() }; // Input slots
```

## Execution Context

Before instance can be evaluated the `InferenceExectionContext` must be created and populated with all the necessary information. This context contains:

* pointer to the instance to use
* pointer to the optional callback to receive results
* pointer to the optional context for the above callback
* pointer to input data slots
* pointer to the output data slots (optional and normally provided by plugins)
* pointer to any runtime parameters (again can be chained together as needed)

The following sections contain examples showing how to utilize execution context.

### Blocking Vs Asynchronous Evaluation

Each plugin can opt to implement blocking and/or non-blocking API used to evaluate instance (essentially run an inference pass). For example:

```cpp
nvigi::InferenceExecutionContext ctx{};
ctx.runtimeParameters = runtime;
ctx.instance = instance;
ctx.callback = myCallback; // called on a different thread
ctx.callbackUserData = &myCtx;
ctx.inputs = &inputs;
```
Async approach, returns immediately, **callback is triggered from a different thread managed by the instance**
```cpp
ctx.instance->evaluateAsync(&ctx)
```
Blocking approach, returns when done, **callback is triggered on this thread**
```cpp
ctx.instance->evaluate(&ctx)
```

## Obtaining Results

There are two ways to obtain results:

* By providing callback in the `InferenceExectionContext` and receiving results either on host's or NVIGI's thread
* By NOT providing a callback and forcing `evaluateAsync` path, which results in requiring host app to poll for result.

### Callback Approach

This is the simplest and easiest way to obtain results. Callback function of the following type must be provided via `InferenceExectionContext` before calling evaluate:

```cpp
auto inferenceCallback = [](const nvigi::InferenceExecutionContext* execCtx, nvigi::InferenceExecutionState state, void* userData)->nvigi::InferenceExecutionState 
{     
    //! Optional user context to control execution 
    auto userCtx = (HostProvidedCallbackCtx*)userData; 
    if (execCtx->outputs) 
    { 
       const nvigi::InferenceDataText* text{}; 
       execCtx->outputs->findAndValidateSlot(nvigi::kASRDataSlotTranscribedText, &text); 
       std::string transcribedText = text->getUtf8Text();
       //! Do something with the received text 
    } 
    if (state == nvigi::InferenceExecutionStateDone) 
    { 
        //! This is all the data we can expect to receive 
    } 
    else if(userCtx->needToInterruptInference) 
    { 
        //! Inform NVIGI that inference should be cancelled 
        return nvigi::InferenceExecutionStateCancel; 
    } 
    return state; 
}; 

nvigi::InferenceExecutionContext asrContext{};
asrContext.instance = asrInstanceLocal;         // The instance we created and we want to run inference on
asrContext.callback = asrCallback;              // Callback to receive transcribed text
asrContext.callbackUserData = &asrCallback;     // Optional context for the callback, can be null if not needed
asrContext.inputs = &inputs;
// BLOCKING
if(NVIGI_FAILED(res, asrContext.instance->evaluate(asrContext)))
{
    LOG("NVIGI call failed, code %d", res);
}    
```

> IMPORTANT:
> To cancel inference simply return `nvigi::InferenceExecutionStateCancel` in the callback

### Polling Approach

> IMPORTANT: This is an optional way to obtain results and each individual plugin must implement special interface `nvigi::IPolledInferenceInterface` in order to enable this functionality. In addition, when using polling, `evaluateAsync` is the ONLY viable inference model since we cannot have blocking calls.

Before proceeding any further it is necessary to obtain polling interface from the plugin, assuming it is actually implemented:

```cpp
nvigi::IPolledInferenceInterface* ipolled{};
nvigiGetInterface(feature, &ipolled);
```

Upon successful retrieval of the polling interface the next step is to skip providing callback in the execution context. This will automatically make evaluate call async (plugin will generate and manage a thread) and host application will need to check if results are ready before consuming them.

```cpp

nvigi::InferenceExecutionContext asrContext{};
asrContext.instance = asrInstanceLocal;         // The instance we created and we want to run inference on
asrContext.callback = nullptr;                  // NO CALLBACK WHEN POLLING RESULTS
asrContext.callbackUserData = nullptr;
asrContext.inputs = &inputs;
// ASYNC, note that in this mode one CANNOT use blocking evaluate call
if(NVIGI_FAILED(res, asrContext.instance->evaluateAsync(asrContext)))
{
    LOG("NVIGI call failed, code %d", res);
}    

// Poll for results on host's thread
nvigi::InferenceExecutionState state = nvigi::InferenceExecutionStateDataPending;
while (state == nvigi::InferenceExecutionStateDataPending)
{
    if (blocking)
    {
        // Block and wait for results
        ipolled->getResults(&ctx, true, &state);
        // Process results and release them
        inferenceCallback(&ctx, state, nullptr);
        ipolled->releaseResults(&ctx, state);
    }
    else
    {
        // Check if there are some results, if not move on
        if(ipolled->getResults(&ctx, false, &state) == nvigi::ResultOk)
        {
            // Process results and release them
            inferenceCallback(&ctx, state, nullptr);
            ipolled->releaseResults(&ctx, state);
        }
    }    
}
```

> NOTE: Even with polling we still ultimately use the callback function to process output slots in the execution context, simply for convenience