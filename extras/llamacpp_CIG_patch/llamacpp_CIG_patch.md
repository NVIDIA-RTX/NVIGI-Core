# llama.cpp patch for enabling CIG without using IGI

## Purpose
Games typically optimize graphics to maximize GPU utilization, so when you add AI compute to a game it is essential to configure the GPU scheduler to minimize the effect on the game's frame rate.

The official version of llama.cpp on github does not configure the GPU scheduler for game use. In particular it doesn't enable CIG (CUDA in Graphics), which is necessary to achieve fine grained load balancing between CUDA inference and game graphics running in parallel on the GPU.

NVIGI uses modified versions of llama.cpp, whisper.cpp etc. that do enable CIG. However, you might want to use CIG without using NvIGI, for example to use a newer version of llama.cpp. The extras/llamacpp_CIG_patch directory contains a patch to enable CIG in llama.cpp without using NvIGI. For more information on GPU scheduling, please see GpuSchedulingForAI.md.

The setup.bat script:
- Clones the TOT (at the time of writing) of llama.cpp from the public github.
- Applies a patch that implements CIG in the library, and adds CIG verification to llama-bench
- Configures cmake with `GGML_CUDA_NO_VMM=ON` because CIG does not support CUDA virtual memory.
- Builds llama-bench

The patch:

- Modifies llama.cpp to select kernels that respect the shared memory limits of CIG.
- Adds API `ggml_backend_get_cuda_streams()` to allow the app to get a list of CUDA streams used by llama.cpp. This is necessary to allow the application to set CIG priorities.
- Adds code to llama-bench to initialize CIG from a D3D device.
- Adds code to llama-bench to set CIG workload type, which determines scheduling priority.
- Instruments llama-bench with a **CIG compatibility checker** (CUPTI + D3D12 probe + shared-memory limits, CUDA stream workload tagging via `cig_scheduler_settings.dll`) to verify that CIG is working correctly.

## Prerequisites

- CUDA toolkit 12.8 (needed for CIG-compatible cublas)
- Microsoft Visual Studio 2022
- git installed and in PATH
- cmake installed and in PATH

## Run `setup.bat`

1. `cd` to the patch directory, for example:  
   `extras\llamacpp_CIG_patch`
2. Run:

   ```bat
   setup.bat
   ```

## Run `llama-bench`

Run the executable like normal **llama-bench** (model path, batch sizes, etc.), for example:

```bat
cd llama.cpp\build\bin\RelWithDebInfo
llama-bench.exe -m \\path\\to\\model.gguf ...
```

Consult upstream **llama.cpp** docs for full CLI options. Ensure **`cig_scheduler_settings.dll`** (and **`cupti64_*.dll`** if not already on `PATH`) sit next to **`llama-bench.exe`**; the CMake post-build steps from the patch try to copy them when paths resolve.

## Example output
```
Llama.cpp\build\bin\RelWithDebInfo\llama-bench.exe -m Qwen3-4B-Q4_1.gguf
CIG Info: Created CIG context: context=0000024B867341E0, contextId=1, returned context=0000024B867341E0
CIG Info: max shared memory bytes for CIG = 84992 (CTX is SUPPORTED)
ggml_cuda_init: found 1 CUDA devices (Total VRAM: 12226 MiB):
  Device 0: NVIDIA GeForce RTX 5070, compute capability 12.0, VMM: no, VRAM: 12226 MiB
```  
| model                          |       size |     params | backend    | ngl |            test |                  t/s |
| ------------------------------ | ---------: | ---------: | ---------- | --: | --------------: | -------------------: |
| qwen3 4B Q4_1                  |   2.41 GiB |     4.02 B | CUDA       |  99 |           pp512 |     4992.39 ± 448.37 |
| qwen3 4B Q4_1                  |   2.41 GiB |     4.02 B | CUDA       |  99 |           tg128 |        150.19 ± 0.74 |
```
build: 44dbe8c52 (9067)
CIG Info: CIG contexts used: 0000024B867341E0 (device 0: NVIDIA GeForce RTX 5070)
CIG Info: Launches of each workload type:
                    foreground: 0
                    background: 5615
      background with throttle: 0
```

## What to look for in the output

During the run you may see **stdout** lines from the checker, for example:

- **`CIG Info: max shared memory bytes for CIG = …`** the maximum amount of shared memory a kernel can use when running in parallel with the graphics pipeline. This value is used to check that llama.cpp is selecting CIG compatible kernels.
- **`CIG Info: Launches of each workload type:`** — counts launches per **`CigWorkloadType`**.
- **`CIG Info: CIG contexts used:`** / **`CIG Compatibility Error: the following non-CIG contexts were used:`** — whether CUDA work stayed on a CIG context vs. leaked to a non-CIG context.
- **`CIG Compatibility Error: Kernel … uses … bytes of shared memory`** — a kernel exceeded the CIG shared-memory limit reported by the checker.
- **`Skipping CUPTI due to errors, …`** — CUPTI could not be enabled (newer hardware / tooling quirks); compatibility checks that depend on it may be degraded.
- **`Error loading cig_scheduler_settings.dll`** — DLL not next to the exe or wrong architecture; stream tagging will fail.

The usual **llama-bench** timing tables still print as upstream; the lines above are **diagnostics** layered on top for CIG validation.
