// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "cuda_scg.h"
#include <dxgi.h>
#include <d3d12.h>
#include <cuda.h>
#include <cuda_runtime.h>

#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.file/file.h"

namespace nvigi
{
	namespace cudaScg
	{
		nvigi::Result CreateSharedCUDAContext(ID3D12Device* device, ID3D12CommandQueue* queue, CUcontext& cuContext)
		{
			//! Use special GUID to obtain the underlying native interface if SL proxy is used, returns null otherwise
			IID riid;
			IIDFromString(L"{ADEC44E2-61F0-45C3-AD9F-1B37379284FF}", &riid);

			ID3D12Device* nativeD3D12Device = nullptr;
			device->QueryInterface(riid, reinterpret_cast<void**>(&nativeD3D12Device));
			if (nativeD3D12Device)
			{
				// SL proxy is passed from the host, this is not good ...
				NVIGI_LOG_WARN("Provided D3D12 device 0x%llu is a Streamline (SL) proxy which is NOT supposed to be used with 3rd party libraries like NVIGI, using native interface 0x%llu instead", device, nativeD3D12Device);
				// Swap to using native interface to prevent crashes
				device = nativeD3D12Device;
				// Release extra reference added by QueryInterface
				nativeD3D12Device->Release();
			}
			ID3D12CommandQueue* nativeD3D12Queue = nullptr;
			queue->QueryInterface(riid, reinterpret_cast<void**>(&nativeD3D12Queue));
			if (nativeD3D12Queue)
			{
				// SL proxy is passed from the host, this is not good ...
				NVIGI_LOG_WARN("Provided D3D12 queue 0x%llu is a Streamline (SL) proxy which is NOT supposed to be used with 3rd party libraries like NVIGI, using native interface 0x%llu instead", queue, nativeD3D12Queue);
				// Swap to using native interface to prevent crashes
				queue = nativeD3D12Queue;
				// Release extra reference added by QueryInterface
				nativeD3D12Queue->Release();
			}

			// Ensure we load CUDA that is in system32
			HMODULE cudaModule = LoadLibraryExA("nvcuda.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (cudaModule)
			{
				using PFun_cuCtxCreate_v4 = CUresult(CUcontext* pctx, CUctxCreateParams* ctxCreateParams, unsigned int flags, CUdevice dev);
				PFun_cuCtxCreate_v4* ptr_cuCtxCreate_v4 = (PFun_cuCtxCreate_v4*)GetProcAddress(cudaModule, "cuCtxCreate_v4");
				if (ptr_cuCtxCreate_v4)
				{
					CUctxCigParam ctxCigParam;
					ctxCigParam.sharedDataType = CIG_DATA_TYPE_D3D12_COMMAND_QUEUE;
					ctxCigParam.sharedData = queue;

					CUctxCreateParams ctxCreateParams;
					ctxCreateParams.execAffinityParams = nullptr;
					ctxCreateParams.numExecAffinityParams = 0;
					ctxCreateParams.cigParams = &ctxCigParam;

					::LUID dx12deviceluid = device->GetAdapterLuid();

					// Need to find the CUDA device that represents the same adapter as the D3D12 device
					int devIndex{};
					int numCudaDevices = 0;
					cudaError_t cudaErr = cudaGetDeviceCount(&numCudaDevices);
					if (cudaErr != cudaSuccess)
					{
						NVIGI_LOG_ERROR("CiG could not locate devices!");
						return nvigi::kResultDriverOutOfDate;
					}
					for (int32_t devId = 0; devId < numCudaDevices; devId++) {
						cudaDeviceProp devProp;
						cudaErr = cudaGetDeviceProperties(&devProp, devId);
						if (cudaErr != cudaSuccess)
						{
							NVIGI_LOG_ERROR("CiG could not locate get device properties!");
							return nvigi::kResultDriverOutOfDate;
						}

						if ((memcmp(&dx12deviceluid.LowPart, devProp.luid,
							sizeof(dx12deviceluid.LowPart)) == 0) &&
							(memcmp(&dx12deviceluid.HighPart,
								devProp.luid + sizeof(dx12deviceluid.LowPart),
								sizeof(dx12deviceluid.HighPart)) == 0)) {
							devIndex = devId;
							break;
						}
					}

					// cuCtxCreate_v4 has the side effect of changing the 
					// current context. We don't want this though, we 
					// want users to explicitly pass the CIG context to 
					// plugins they want to run with CIG. For this reason we
					// save and restore the existing context around the create.
					CUcontext existingCtx{};
					CUresult err = cuCtxGetCurrent(&existingCtx);
					if (err != CUDA_SUCCESS)
					{
						NVIGI_LOG_WARN("CiG could not get previous context!");
						return nvigi::kResultInvalidState;
					}

					CUdevice dev{};
					cuDeviceGet(&dev, devIndex);
					err = (*ptr_cuCtxCreate_v4)(&cuContext, &ctxCreateParams, 0, dev);
					if (err != CUDA_SUCCESS)
					{
						NVIGI_LOG_WARN("CiG could not create context!");
						return nvigi::kResultDriverOutOfDate;
					}

					err = cuCtxSetCurrent(existingCtx);
					if (err != CUDA_SUCCESS)
					{
						NVIGI_LOG_WARN("CiG could not restore previous context!");
						return nvigi::kResultInvalidState;
					}
				}
				else
				{
					NVIGI_LOG_WARN("CiG could not find CUDA create function!");
					return nvigi::kResultDriverOutOfDate;
				}
			}
			else
			{
				NVIGI_LOG_WARN("CiG could not find CUDA DLL!");
				return nvigi::kResultDriverOutOfDate;
			}

			return nvigi::kResultOk;
		}
	}
}
