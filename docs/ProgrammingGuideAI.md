
# NVIGI - Programming Guide For Local And Cloud Inference

This guide primarily focuses on the general use of the AI plugins performing local or cloud inference. Before reading this please read [the general programming guide](ProgrammingGuide.md)

> **IMPORTANT**: This guide might contain pseudo code, for the up to date implementation and source code which can be copy pasted please see the [basic sample](../source/samples/nvigi.basic/basic.cpp)

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
  - [Canceling Asynchronous Evaluation](#canceling-asynchronous-evaluation)
  
## INTRODUCTION

NVIGI AI plugins provide unified API for both local and cloud inference. This ensures easy transition between local and cloud services and full flexibility. The AI API is located in `nvigi_ai.h` header.

## Key Concepts

* Each AI plugin implements certain features with the specific backend and underlying API, here are some examples:
    *  nvigi.plugin.gpt.ggml.cuda -> implements GPT feature using **GGML backend and CUDA API for local execution**
    *  nvigi.plugin.gpt.cloud.rest -> implements GPT feature using **CLOUD backend and REST API for remote execution**
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

As mentioned in the above section, each model configuration JSON contains a model card with instructions on how to obtain each model.

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

Each LLM requirs a correct prompt template. These templates are stored in the above mentioned model configuration JSON and look something like this:
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

If using [NVIDIA NIM APIs](https://build.nvidia.com) search and navigate to the model you want to use then copy/paste the request into the above mentioned JSON. For example, when selecting [llama-3_1-70b-instruct](https://build.nvidia.com/meta/llama-3_1-70b-instruct) the completion code in Python looks like this:

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

> **IMPORTANT NOTE**: This section covers a scenario where the host application can instantiate models that were not included when the application was packaged and shipped. If the models and their capabilities are predefined and there is no need for dynamically downloaded models, you can skip to the next section.

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

If specific feature has custom capabilities and requirements the common ones will always be either chained together or returned as a pointer within the custom caps. In addition, if feature is a pipeline (parent plugin encapsulating two or more plugins) it will return caps and requirements for ALL enclosed plugins.  These can be queried from what is returned by the parent plugin's `getCapsAndRequirements` using `nvigi::findStruct<structtype>(rootstruct)`.

> IMPORTANT: Always have a look at the plugin's public header `nvigi_$feature.h` to find out if the plugin has custom caps etc.

## Creation Parameters

### Common and Custom

Before creating an instance, host application must specify certain properties like which model should be used, how much VRAM is available, how many CPU threads etc. Similar to the previous section, plugins can have custom creation parameters but they always have to provide common ones. Here is how common creation parameters look like:

```cpp
//! Generic creation parameters - apply to all plugins
//! 
//! {CC8CAD78-95F0-41B0-AD9C-5D6995988B23}
struct alignas(8) CommonCreationParameters {
    CommonCreationParameters() {};
    NVIGI_UID(UID({ 0xcc8cad78, 0x95f0, 0x41b0,{ 0xad, 0x9c, 0x5d, 0x69, 0x95, 0x98, 0x8b, 0x23 } }), kStructVersion2);
    int32_t numThreads = 1;
    size_t vramBudgetMB = SIZE_MAX;
    const char* modelGUID{};
    const char* utf8PathToModels{};
    //! Optional - path to additional models downloaded on the system (if any)
    const char* utf8PathToAdditionalModels{};

    //! v2
    
    //! JSON model card if model GUID is not used and custom model loading is preferred
    const char* modelCardJSON{};

    //! v3+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};
```

### Model Loading Approaches

NVIGI supports three approaches for loading models, allowing flexibility based on your application's requirements:

#### Approach 1: Legacy (GUID-Based Discovery with Extension Search)

The traditional method uses filesystem-based model discovery:

**Requirements:**
- `modelGUID` must be provided (registry format: `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`)
- `utf8PathToModels` must point to valid model repository root
- `FileIOCallbacks` should NOT be provided (or NULL)
- Model config JSON does NOT contain `model.local_files` array

**How it works:**
1. Plugin scans `utf8PathToModels` directory for GUID subdirectory
2. Finds `nvigi.model.config.json` inside GUID folder
3. Discovers model files by scanning directory for required extensions
4. Loads files using standard file I/O

**Example:**
```cpp
CommonCreationParameters common{};
common.utf8PathToModels = "D:/models";
common.modelGUID = "{175C5C5D-E978-41AF-8F11-880D0517C524}";
common.numThreads = 4;
common.vramBudgetMB = 8000;

nvigi::InferenceInstance* instance{};
interface->createInstance(common, &instance);
```

**Model directory structure:**
```
D:/models/
└── nvigi.plugin.gpt.ggml/
    └── {175C5C5D-E978-41AF-8F11-880D0517C524}/
        ├── nvigi.model.config.json  (no local_files array)
        ├── model.gguf
        └── tokenizer.bin
```

#### Approach 2: Hybrid (GUID-Based Discovery with local_files)

Optimized filesystem approach that explicitly lists required files in the model config JSON.

**Requirements:**
- `modelGUID` must be provided (registry format: `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`)
- `utf8PathToModels` must point to valid model repository root
- `FileIOCallbacks` should NOT be provided (or NULL)
- Model config JSON MUST contain `model.local_files` array

**How it works:**
1. Plugin scans `utf8PathToModels` directory for GUID subdirectory
2. Finds `nvigi.model.config.json` inside GUID folder
3. Reads `model.local_files` array from JSON
4. Builds full paths: `{utf8PathToModels}/{pluginDir}/{GUID}/{fileName}`
5. Loads files using standard file I/O (no directory scanning for extensions)

**Benefits:**
- Faster than extension search (no directory scanning)
- Explicitly lists all required files
- Still uses standard filesystem (no callbacks needed)
- Better for models with multiple files or unusual extensions

**Example:**
```cpp
CommonCreationParameters common{};
common.utf8PathToModels = "D:/models";
common.modelGUID = "{175C5C5D-E978-41AF-8F11-880D0517C524}";
common.numThreads = 4;
common.vramBudgetMB = 8000;

nvigi::InferenceInstance* instance{};
interface->createInstance(common, &instance);
```

**Model config JSON with local_files:**
```json
{
    "name": "llama-3.1-8b-instruct",
    "vram": 5124,
    "model": {
        "ext": "gguf",
        "notes": "Model files explicitly listed",
        "local_files": [
            "llama-3.1-8b-instruct.gguf",
            "tokenizer.model",
            "config.json"
        ]
    }
}
```

**Model directory structure:**
```
D:/models/
└── nvigi.plugin.gpt.ggml/
    └── {175C5C5D-E978-41AF-8F11-880D0517C524}/
        ├── nvigi.model.config.json  (contains local_files)
        ├── llama-3.1-8b-instruct.gguf
        ├── tokenizer.model
        └── config.json
```

#### Approach 3: Modern (Direct JSON Card with IO Callbacks)

Modern method bypasses filesystem entirely, allowing virtual file systems, encryption, compression, or remote storage:

**Requirements:**
- `modelCardJSON` must be provided with model configuration
- `FileIOCallbacks` must be provided via parameter chaining
- `modelGUID` and `utf8PathToModels` can be NULL/empty

**How it works:**
1. Host provides complete model card JSON directly via `modelCardJSON`
2. Plugin parses JSON and reads `model.local_files` array for file names
3. Plugin uses `FileIOCallbacks` to load files (no filesystem access)
4. Skips directory scanning entirely (`ai::findModels()` not called)

**Example:**
```cpp
// Prepare model card JSON
const char* modelJSON = R"({
    "name": "my-custom-model",
    "vram": 4096,
    "model": {
        "ext": "gguf",
        "notes": "Custom model with IO callbacks",
        "local_files": [
            "model.gguf",
            "tokenizer.bin"
        ]
    }
})";

// Setup IO callbacks
FileIOCallbacks ioCallbacks{};
ioCallbacks.userData = myCustomFileSystem;
ioCallbacks.open = [](void* userData, const char* path, const char* mode) -> void* {
    auto fs = (MyFileSystem*)userData;
    return fs->openFile(path, mode);
};
ioCallbacks.read = [](void* userData, void* handle, void* buffer, size_t size) -> size_t {
    auto fs = (MyFileSystem*)userData;
    return fs->readFile(handle, buffer, size);
};
ioCallbacks.size = [](void* userData, void* handle) -> size_t {
    auto fs = (MyFileSystem*)userData;
    return fs->getFileSize(handle);
};
ioCallbacks.close = [](void* userData, void* handle) {
    auto fs = (MyFileSystem*)userData;
    fs->closeFile(handle);
};

// Create common parameters with JSON card
CommonCreationParameters common{};
common.modelCardJSON = modelJSON;  // Provide JSON directly
common.modelGUID = nullptr;        // Not needed with callbacks
common.utf8PathToModels = nullptr; // Not needed with callbacks
common.numThreads = 4;
common.vramBudgetMB = 8000;

// Chain IO callbacks
MyPluginCreationParameters params{};
if (NVIGI_FAILED(params.chain(common))) {
    // Handle error
}
if (NVIGI_FAILED(params.chain(ioCallbacks))) {
    // Handle error
}

nvigi::InferenceInstance* instance{};
interface->createInstance(params, &instance);
```

**JSON Card Format:**

The `modelCardJSON` must contain a `model.local_files` array listing all files the plugin needs:

```json
{
    "name": "my-model-name",
    "vram": 4096,
    "model": {
        "ext": "gguf",
        "notes": "Model description",
        "local_files": [
            "my_model.gguf",
            "tokenizer.bin",
            "config.json"
        ]
    },
    "prompt_template": ["optional", "for", "LLM", "models"],
    "turn_template": ["optional", "for", "chat", "models"]
}
```

**Benefits of New Approach:**
- No filesystem access required
- Works with encrypted/compressed storage
- Supports virtual file systems
- Enables cloud/network storage
- Allows custom security/access control
- Model files don't need to be on disk

### Implementing Plugins Supporting All Three Approaches

The beauty of the unified API is that plugins don't need to manually check which approach is being used. Simply call `ai::findModels()` with the `FileIOCallbacks` parameter, and it automatically detects and handles all three approaches!

Here's the simplified implementation pattern in your plugin's `onCreateInstance` method:

```cpp
static Expected<void> onCreateInstance(
    const NVIGIParameter* params,
    std::any& pluginData)
{
    auto common = findStruct<CommonCreationParameters>(params);
    if (!common) {
        return std::unexpected(Error{
            kResultInvalidParameter,
            "CommonCreationParameters required"
        });
    }

    // Get FileIOCallbacks (nullptr if not using Approach 3)
    auto ioCallbacks = findStruct<FileIOCallbacks>(params);
    
    // Unified model discovery - handles ALL three approaches automatically!
    auto& ctx = ModernPluginBase<MyPlugin, IMyAPI>::getContext();
    if (ctx.modelInfo.empty()) {
        // Just pass ioCallbacks and ai::findModels() does the rest:
        // - Approach 3: ioCallbacks provided → uses modelCardJSON directly
        // - Approach 2: no callbacks, JSON has local_files → uses file names from JSON
        // - Approach 1: no callbacks, JSON lacks local_files → scans by extension
        if (!ai::findModels(common, {"gguf", "bin"}, ctx.modelInfo, ioCallbacks)) {
            return std::unexpected(Error{
                kResultInvalidParameter,
                "Failed to find models - check logs for details"
            });
        }
    }
    
    // Extract model information
    // For Approach 3 (callbacks), the key is "callback-model"
    // For Approach 1 & 2 (filesystem), the key is the GUID
    std::string modelKey = ioCallbacks ? "callback-model" : common->modelGUID;
    
    std::vector<std::string> modelFiles;
    json modelCard;
    
    try {
        modelCard = ctx.modelInfo[modelKey];
        
        // Get files for your extension (e.g., "gguf")
        if (ctx.modelInfo[modelKey].contains("gguf")) {
            modelFiles = ctx.modelInfo[modelKey]["gguf"];
        }
    } catch (const std::exception& e) {
        return std::unexpected(Error{
            kResultJSONException,
            "Model info error: " + std::string(e.what())
        });
    }
    
    if (modelFiles.empty()) {
        return std::unexpected(Error{
            kResultInvalidParameter,
            "No model files found"
        });
    }
    
    // Now load the files
    if (ioCallbacks) {
        // Approach 3: Load using callbacks
        for (const auto& fileName : modelFiles) {
            auto handle = ioCallbacks->open(
                ioCallbacks->userData, fileName.c_str(), "rb");
            if (!handle) {
                return std::unexpected(Error{
                    kResultInvalidParameter,
                    "Failed to open: " + fileName
                });
            }
            
            size_t size = ioCallbacks->size(ioCallbacks->userData, handle);
            std::vector<char> buffer(size);
            ioCallbacks->read(ioCallbacks->userData, handle, buffer.data(), size);
            ioCallbacks->close(ioCallbacks->userData, handle);
            
            // Process buffer...
        }
    } else {
        // Approach 1 or 2: Load using standard file I/O
        for (const auto& filePath : modelFiles) {
            // Load from disk using standard I/O
            // filePath is already a full path from ai::findModels()
            // ... load from disk ...
        }
    }
    
    // Continue with model initialization using loaded data...
    return {};
}
```

**That's it!** No need to manually check parameters, validate approaches, or duplicate logic. The `ai::findModels()` function handles everything.

**Key Points for Plugin Developers:**

1. **Always call `ai::findModels()` with `ioCallbacks` parameter** - it handles everything automatically
2. **Don't manually validate which approach** - the function does it for you with detailed error messages
3. **Use correct model key** to access results:
   - With callbacks: key is `"callback-model"`
   - Without callbacks: key is the `modelGUID`
4. **After `ai::findModels()` returns**, file loading differs:
   - With callbacks: use callbacks to load files (file names are virtual paths)
   - Without callbacks: use standard file I/O (file names are full disk paths)
5. **Check logs** - `ai::findModels()` logs which approach was detected and used

**Comparison of Approaches:**

| Feature | Approach 1 (Legacy) | Approach 2 (Hybrid) | Approach 3 (Modern) |
|---------|---------------------|---------------------|---------------------|
| GUID Required | Yes | Yes | No |
| Path Required | Yes | Yes | No |
| JSON Card Provided | No | No | Yes |
| IO Callbacks | No | No | Yes |
| File Discovery | Extension scan | local_files list | local_files list |
| Filesystem Access | Yes | Yes | No |
| Best For | Simple models | Complex models on disk | Virtual/encrypted storage |

### FileIOCallbacks Structure

The `FileIOCallbacks` structure provides a complete file I/O abstraction:

```cpp
//! File I/O callbacks for custom file handling
//! 
//! {E9A8C7B4-2F6D-4A1E-9B3C-5D8E7F6A4B2C}
struct alignas(8) FileIOCallbacks {
    FileIOCallbacks() {};
    NVIGI_UID(UID({ 0xe9a8c7b4, 0x2f6d, 0x4a1e,
        { 0x9b, 0x3c, 0x5d, 0x8e, 0x7f, 0x6a, 0x4b, 0x2c } }), kStructVersion1);
    
    //! User-defined context passed to all callbacks
    void* userData{};
    
    //! Open file for reading/writing
    //! @param userData User context
    //! @param path File path (virtual or physical)
    //! @param mode Open mode ("r", "rb", "w", "wb", etc.)
    //! @return File handle or nullptr on error
    void* (*open)(void* userData, const char* path, const char* mode){};
    
    //! Read from file
    //! @param userData User context
    //! @param handle File handle from open()
    //! @param buffer Destination buffer
    //! @param size Number of bytes to read
    //! @return Number of bytes actually read
    size_t (*read)(void* userData, void* handle, void* buffer, size_t size){};
    
    //! Get file size
    //! @param userData User context
    //! @param handle File handle from open()
    //! @return File size in bytes
    size_t (*size)(void* userData, void* handle){};
    
    //! Close file
    //! @param userData User context
    //! @param handle File handle to close
    void (*close)(void* userData, void* handle){};
    
    //! NEW MEMBERS GO HERE, BUMP THE VERSION!
};
```

> NOTE:
> Same model GUID can be used by different plugins if they are implementing different backends, for example Whisper GGUF model can be loaded by the `nvigi.plugin.asr.ggml.cuda` and `nvigi.plugin.asr.ggml.cpu` plugins

### Cloud Plugins

When it comes to cloud plugins, they can use two different protocols, REST and gRPC. In addition to the above mentioned common creation parameters, cloud plugins require either `RESTParameters` or `RPCParameters` to be chained together with the common ones. Here is an example:

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
if(NVIGI_FAILED(gptCreationParams.chain(common)))
{
  // Handle error
}

//! Cloud parameters
RESTParameters cloudParams{};
std::string token;
getEnvVar("OPENAI_TOKEN", token);
cloudParams.url = "https://api.openai.com/v1/chat/completions";
cloudParams.authenticationToken = token.c_str();
cloudParams.verboseMode = true;

if(NVIGI_FAILED(gptCreationParams.chain(cloudParams))) // Chaining cloud parameters!
{
  // Handle error
}
nvigi::InferenceInstance* instance{};
igpt->createInstance(gptCreationParams, &instance);
```

### Local Plugins

With local plugins there are few key points to consider when selecting which plugin to use:

* Selecting backend and API
* How much VRAM can be used?
* What is the expected latency?

For example fully GPU bottlenecked application or application which does not have enough VRAM left could do the following and run GPT inference completely on the CPU:

```cpp
//! Obtain GPT GGML CPU interface
nvigi::IGeneralPurposeTransformer* igpt{};
nvigiGetInterface(plugin::gpt::ggml::cpu::kId, &igpt);
```

On the other hand, CPU bottlenecked application could do the following and run GPT inference completely on the GPU:

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
if(NVIGI_FAILED(gptCreationParams.chain(d3d12Parameters)))
{
  // Handle error
}
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

For example, this is how one would setup some audio data located on the CPU using the STL helpers from `nvigi_stl_helpers.h`

```cpp
std::vector<int16> my_audio = recordMyMonoAudio();
// Auto convert to the underlying `InferenceDataAudio`, single channel, PCM16
nvigi::InferenceDataAudioSTLHelper audioData(my_audio, 1);
```
Another example, this time setting up a prompt for the GPT plugin:
```cpp
std::string text = "Hello World!";
nvigi::InferenceDataTextSTLHelper userPrompt(text);
```

## Input Slots

Once we have our instance we need to provide input data slots that match the input signature for the given instance. The `InferenceInstance` provides an API to obtain input and output signatures at runtime but they can also be obtained from the plugin's headers and source code. In this guide we will use the Automated Speech Recognition (ASR) as an example.

```cpp
//! Audio data slot is coming from our previous step, note that we are using operator to convert audioData to InferenceDataAudio*
std::vector<nvigi::InferenceDataSlot> slots = { {nvigi::kASRDataSlotAudio, audioData} };
nvigi::InferenceDataSlotArray inputs = { slots.size(), slots.data() }; // Input slots
```
> NOTE: STL helpers provide an operator which automatically converts data to the underlying low level type used by NVIGI

## Execution Context

Before the instance can be evaluated the `InferenceExecutionContext` must be created and populated with all the necessary information. This context contains:

* pointer to the instance to use
* pointer to the optional callback to receive results
* pointer to the optional context for the above callback
* pointer to input data slots
* pointer to the output data slots (optional and normally provided by plugins)
* pointer to any runtime parameters (again can be chained together as needed)

The following sections contain examples showing how to utilize execution context.

### Blocking Vs Asynchronous Evaluation

Each plugin can opt to implement blocking and/or non-blocking APIs used to evaluate the instance (essentially run an inference pass). For example:

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

* By providing callback in the `InferenceExecutionContext` and receiving results either on host's or NVIGI's thread
* By NOT providing a callback and forcing `evaluateAsync` path, which results in requiring host app to poll for result.

### Understanding Execution States

When receiving results through callbacks or polling, the execution state indicates the status of the data:

* **`InferenceExecutionStateDone`** - All processing is complete, no more data will be provided
* **`InferenceExecutionStateCancel`** - The inference was canceled by the host
* **`InferenceExecutionStateDataPending`** - The provided data is **final and will not change**, but more data is expected. This data should be committed/saved.
* **`InferenceExecutionStateDataPartial`** - The provided data is **tentative and may change**. The plugin may replace or correct this data in subsequent callbacks as more context becomes available.

#### Partial Data Behavior

`InferenceExecutionStateDataPartial` is particularly important for streaming scenarios like Automated Speech Recognition (ASR). As more audio is processed, the model gains additional context which can cause it to "correct" previously transcribed text.

For example, an ASR plugin processing the phrase "Hello world, how are you?" might produce:

```
Callback 1: "Hell" (Partial)
Callback 2: "Hello" (Partial - corrected from "Hell")  
Callback 3: "Hello" (Pending - confirmed, won't change)
Callback 4: "Hello world" (Pending - "world" is confirmed)
Callback 5: "Hello world how" (Partial - "how" is tentative)
Callback 6: "Hello world hi" (Partial - corrected to "hi" with more context)
Callback 7: "Hello world hi" (Pending - "hi" is confirmed)
Callback 8: "Hello world hi there" (Done - complete transcription)
```

Applications should handle partial data appropriately:
* Display partial data to users with visual indication (e.g., dimmed/italicized text) that it may change
* Do not commit partial data to permanent storage or trigger actions based on it
* Only process/commit data marked as Pending or Done

### Callback Approach

This is the simplest and easiest way to obtain results. Callback function of the following type must be provided via `InferenceExecutionContext` before calling evaluate:

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
       
       //! Handle different states appropriately
       if (state == nvigi::InferenceExecutionStateDataPartial)
       {
           //! Partial data - may change in subsequent callbacks
           //! Display to user with indication it's tentative (e.g., dimmed/italicized)
           //! Do NOT commit to permanent storage or trigger actions
           userCtx->displayPartialResult(transcribedText);
       }
       else if (state == nvigi::InferenceExecutionStateDataPending || state == nvigi::InferenceExecutionStateDone)
       {
           //! Pending/Done - this data is final and won't change
           //! Safe to commit to storage, trigger actions, etc.
           userCtx->commitFinalResult(transcribedText);
       }
    } 
    if (state == nvigi::InferenceExecutionStateDone) 
    { 
        //! This is all the data we can expect to receive 
        userCtx->onComplete();
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

Before proceeding any further, it is necessary to obtain the polling interface from the plugin, assuming it is actually implemented:

```cpp
nvigi::IPolledInferenceInterface* ipolled{};
nvigiGetInterface(feature, &ipolled);
```

Upon successful retrieval of the polling interface, the next step is to skip providing a callback to the execution context. This will automatically make evaluate call async (plugin will generate and manage a thread) and host application will need to check if results are ready before consuming them.

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

### Canceling Asynchronous Evaluation

> IMPORTANT: This API is only available for plugins that implement version 3 or higher of the `InferenceInstance` interface. Not all plugins may support cancellation, in which case the API will return `nvigi::ResultNoImplementation`.

When using asynchronous evaluation with polling (i.e., `evaluateAsync` with a null callback), you can explicitly cancel an in-progress inference operation by calling the `cancelAsyncEvaluation` method. This is particularly useful when you need to abort a long-running inference task due to user input or application requirements.

Here's how to use `cancelAsyncEvaluation`:

```cpp
// Start async evaluation with polling (no callback)
nvigi::InferenceExecutionContext asrContext{};
asrContext.instance = asrInstanceLocal;
asrContext.callback = nullptr;          // NO CALLBACK - using polling mode
asrContext.callbackUserData = nullptr;
asrContext.inputs = &inputs;

// Start async evaluation
if(NVIGI_FAILED(res, asrContext.instance->evaluateAsync(&asrContext)))
{
    LOG("NVIGI call failed, code %d", res);
}

// ... Later, if you need to cancel the operation ...

// Check if the plugin supports cancellation
if (asrContext.instance->cancelAsyncEvaluation != nullptr)
{
    // Request cancellation of the async evaluation
    nvigi::Result cancelResult = asrContext.instance->cancelAsyncEvaluation(&asrContext);
    
    if (cancelResult == nvigi::kResultOk)
    {
        // Cancellation request successful
        // Continue polling to receive the final state
        nvigi::InferenceExecutionState state;
        while (state != nvigi::InferenceExecutionStateDone)
        {
            if (ipolled->getResults(&asrContext, true, &state) == nvigi::kResultOk)
            {
                // Process any remaining results
                if (state == nvigi::InferenceExecutionStateCancel)
                {
                    LOG("Inference was successfully canceled");
                }
                ipolled->releaseResults(&asrContext, state);
            }
        }
    }
    else if (cancelResult == nvigi::kResultNoImplementation)
    {
        LOG("Plugin does not support cancellation");
    }
}
```

**Key Points:**

* `cancelAsyncEvaluation` should only be called after `evaluateAsync` has been invoked with a null callback pointer
* The method can be `nullptr` or return `nvigi::ResultNoImplementation` if the plugin doesn't support cancellation
* After calling `cancelAsyncEvaluation`, you should continue polling with `getResults` to properly clean up the evaluation context
* The final state received will be `InferenceExecutionStateCancel` when cancellation is successful
* This method is NOT thread safe and should be called from the same thread that manages the evaluation context

**Alternative: Canceling via Callback**

When using the callback approach (instead of polling), inference can be canceled by returning `InferenceExecutionStateCancel` from the callback function itself, as shown in the earlier callback examples. The `cancelAsyncEvaluation` API is specifically designed for the polling workflow where no callback is provided.