# HYBRID AI

This document contains information about the Hybrid AI inference API powered by In-Game Inferencing (NVIGI). 

For detailed explanation on how to integrate AI features in your application please see programming guides for [OpenAI ASR](./ProgrammingGuideASR.md) and [Meta GPT](./ProgrammingGuideGPT.md)

> NOTE:
> Dark theme is recommended for the best reading experience.

## Architecture

```{image} media/hybridai.svg
:alt: hybridai
:align: center
```

* NVIGI plugins implement inference for various AI models (ASR, GPT, Riva, Nemo, Audio2Face etc.) using variety of backends like for example TRT, GGML, cuGfx etc.
* Each plugin supports hybrid execution model by implementing any combination of backend "locations": CPU, GPU or CLOUD
  * Considering that 97% of gamers have <=8GB of VRAM the open-sourced library GGML is very important part of this solution since it allows execution on CPU, GPU, mix between CPU/GPU or even mix between multiple GPUs if gamers keep their older cards
* Same code base and interchangeable API is used for Windows and Linux
* Games use the same NVIGI SDK C++ API directly since it provides lowest possible latency
* Other Windows applications can use:
  * NVIGI SDK C++ directly if lowest latency is desired
  * Optional C++ wrapper for the REST API, if desired
* CLOUD API can be specific to the cloud provider, for example, NVCF for GFN etc.

## API

Developer queries feature requirements and capabilities then creates instance(s) as shown below:

```{image} media/hybridai_api.svg
:alt: hybridai_api
:align: center
```

Creation properties contain, but are not limited to, the following:

* CPU threads, VRAM budget, model GUID, model location (if any), force identical cloud model or not (i.e. no OTA) etc.​
* Various other properties which are feature/model specific​
* Feature requirements allow developer to determine if feature can even run locally etc.​

> **IMPORTANT**:
> Developer has full control over inference backend type, location (cpu, gpu, cloud) and behavior via properties.

### Instance Data Descriptors

Each instance reports input and output signature with names and data type so it is possible to create correct inputs and outputs and also connect different instances which form a pipeline.

```{image} media/hybridai_api_instance.svg
:alt: hybridai_api_instance
:align: center
```

> NOTE:
> Some inputs can be optional while others are mandatory, each input slot in the signature contains flag indicating which is which

### Instance Evaluation

Once instance is created and all inputs for it have been generated and provided, that instance can be evaluated to produce inferred results which is returned to the host via callback.

```{image} media/hybridai_api_instance_eval.svg
:alt: hybridai_api_instance_eval
:align: center
```

* Runtime properties are optional and depend on model
* Input data is mandatory and must match instance's input signature
* Output data slots are also optional, if not provided NVIGI will allocate them
* Callback is mandatory and it is used to retrieve data and cancel inference if needed
* **It is important to note that the host application should not make any assumptions about the `instance->eval` execution flow**:
  * Evaluation can be asynchronous and there is no guarantee that when `instance->eval` returns inference is done
  * All input data must be valid until inference is done (callback reports inference state)

### Pipelining

```{image} media/hybridai_api_instance_pipeline.svg
:alt: hybridai_api_instance_pipeline
:align: center
```

* Multiple instances can be queued up in a "pipeline"
* Evaluation is performed by the `nvigi.ai` parent plugin which must be requested by the host
* First instance must provide valid inputs
* Last instance must provide callback to receive inference results
* Callbacks and outputs for each other stage are optional and can be used to retrieve intermediate data (if needed)
* NVIGI will automatically connect inputs/outputs between stages and allocate memory if data slots are left as null
* NVIGI might optimize or fuse some stages in the pipeline if possible but host should not make any assumptions about this
* **There is no generic magical solution for handling all the potential custom pre/post processing between pipeline stages**

> NOTE:
> Pipelining is just a wrapper/helper potentially making integrations easier, it does not add any new functionality

Here is an example of a potential optimization in case of the `ACE` pipeline:

* Host can create local (CPU/GPU) instances for the `ASR`, `GPT`, `textToSpeech` for example
* If latency is too high, there is not enough VRAM NVIGI can decide to offload certain stages to the cloud as show in the diagram below

```{image} media/hybridai_ace_pipeline.svg
:alt: hybridai_ace_pipeline
:align: center
```
