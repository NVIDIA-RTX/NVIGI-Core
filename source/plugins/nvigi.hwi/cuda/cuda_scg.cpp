// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "cuda_scg.h"
#include <dxgi.h>
#include <d3d12.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <vulkan/vulkan.h>
#include <vector>

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
						NVIGI_LOG_WARN("CiG could not create context! cuCtxCreate_v4 returned %d", err);
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
				NVIGI_LOG_WARN("Failed to load nvcuda.dll");
				return nvigi::kResultDriverOutOfDate;
			}

			return nvigi::kResultOk;
		}

		nvigi::Result CreateSharedCUDAContextVulkan(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, CUcontext& cuContext)
		{
			// First, check if the physical device supports the external compute queue extension
			uint32_t extensionCount = 0;
			vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
			
			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());
			
			bool externalComputeQueueSupported = false;
			for (const auto& extension : availableExtensions) {
				if (strcmp(extension.extensionName, "VK_NV_external_compute_queue") == 0) {
					externalComputeQueueSupported = true;
					break;
				}
			}
			
			if (!externalComputeQueueSupported) {
				NVIGI_LOG_WARN("VK_NV_external_compute_queue extension not supported by physical device!");
				return nvigi::kResultDriverOutOfDate;
			}

			// Ensure we load CUDA that is in system32
			HMODULE cudaModule = LoadLibraryExA("nvcuda.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (!cudaModule)
			{
				NVIGI_LOG_WARN("Failed to load nvcuda.dll");
				return nvigi::kResultDriverOutOfDate;
			}

			using PFun_cuCtxCreate_v4 = CUresult(CUcontext* pctx, CUctxCreateParams* ctxCreateParams, unsigned int flags, CUdevice dev);
			PFun_cuCtxCreate_v4* ptr_cuCtxCreate_v4 = (PFun_cuCtxCreate_v4*)GetProcAddress(cudaModule, "cuCtxCreate_v4");
			if (!ptr_cuCtxCreate_v4)
			{
				NVIGI_LOG_WARN("CiG could not find CUDA create function!");
				return nvigi::kResultDriverOutOfDate;
			}

			// Get function pointers for the extensions we use
			PFN_vkCreateExternalComputeQueueNV ptr_vkCreateExternalComputeQueueNV =
				(PFN_vkCreateExternalComputeQueueNV)vkGetDeviceProcAddr(device, "vkCreateExternalComputeQueueNV");
						
			if (!ptr_vkCreateExternalComputeQueueNV)
			{
				NVIGI_LOG_WARN("CiG could not find vkCreateExternalComputeQueueNV!");
				return nvigi::kResultDriverOutOfDate;
			}

			PFN_vkGetExternalComputeQueueDataNV ptr_vkGetExternalComputeQueueDataNV =
				(PFN_vkGetExternalComputeQueueDataNV)vkGetDeviceProcAddr(device, "vkGetExternalComputeQueueDataNV");
						
			if (!ptr_vkGetExternalComputeQueueDataNV)
			{
				NVIGI_LOG_WARN("CiG could not find vkGetExternalComputeQueueDataNV!");
				return nvigi::kResultDriverOutOfDate;
			}

			// Get Vulkan external compute queue properties
			VkPhysicalDeviceExternalComputeQueuePropertiesNV externalComputeProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_COMPUTE_QUEUE_PROPERTIES_NV };
			VkPhysicalDeviceProperties2 physDevProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
			physDevProps.pNext = &externalComputeProperties;
			vkGetPhysicalDeviceProperties2(physicalDevice, &physDevProps);

			// Allocate an external queue
			VkExternalComputeQueueNV vkExternalQueue;
			VkExternalComputeQueueCreateInfoNV createInfo = { VK_STRUCTURE_TYPE_EXTERNAL_COMPUTE_QUEUE_CREATE_INFO_NV };
			ptr_vkCreateExternalComputeQueueNV(device, &createInfo, nullptr, &vkExternalQueue);

			// Allocate memory for the shared data blob
			void* vkSharedDataBlob = malloc(externalComputeProperties.externalDataSize);
			if (!vkSharedDataBlob)
			{
				NVIGI_LOG_ERROR("Failed to allocate memory for Vulkan shared data blob");
				return nvigi::kResultInsufficientResources;
			}

			// Get the external compute queue data
			int physicalDeviceIdx = 0;
			VkExternalComputeQueueDataParamsNV vkExternalComputeQueueDataParams = { VK_STRUCTURE_TYPE_EXTERNAL_COMPUTE_QUEUE_DATA_PARAMS_NV };
			vkExternalComputeQueueDataParams.deviceIndex = physicalDeviceIdx;
			ptr_vkGetExternalComputeQueueDataNV(vkExternalQueue, &vkExternalComputeQueueDataParams, vkSharedDataBlob);

#define CIG_DATA_TYPE_NV_BLOB CUcigDataType(2)

			// Setup CIG parameters for Vulkan
			CUctxCigParam ctxCigParam;
			ctxCigParam.sharedDataType = CIG_DATA_TYPE_NV_BLOB;
			ctxCigParam.sharedData = vkSharedDataBlob;

			CUctxCreateParams ctxCreateParams;
			ctxCreateParams.execAffinityParams = nullptr;
			ctxCreateParams.numExecAffinityParams = 0;
			ctxCreateParams.cigParams = &ctxCigParam;

			// Find the CUDA device that corresponds to the Vulkan device
			VkPhysicalDeviceIDProperties deviceIDProps = {};
			deviceIDProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
			VkPhysicalDeviceProperties2 physicalDeviceProps2 = {};
			physicalDeviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			physicalDeviceProps2.pNext = &deviceIDProps;
			vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProps2);

			// Find matching CUDA device by UUID
			int devIndex = -1;
			int numCudaDevices = 0;
			cudaError_t cudaErr = cudaGetDeviceCount(&numCudaDevices);
			if (cudaErr != cudaSuccess)
			{
				NVIGI_LOG_ERROR("CiG could not locate CUDA devices!");
				free(vkSharedDataBlob);
				return nvigi::kResultDriverOutOfDate;
			}

			for (int32_t devId = 0; devId < numCudaDevices; devId++)
			{
				cudaDeviceProp devProp;
				cudaErr = cudaGetDeviceProperties(&devProp, devId);
				if (cudaErr != cudaSuccess)
				{
					NVIGI_LOG_ERROR("CiG could not get CUDA device properties!");
					continue;
				}

				// Compare device UUIDs
				if (memcmp(devProp.uuid.bytes, deviceIDProps.deviceUUID, VK_UUID_SIZE) == 0)
				{
					devIndex = devId;
					break;
				}
			}

			if (devIndex == -1)
			{
				NVIGI_LOG_ERROR("Could not find matching CUDA device for Vulkan device");
				free(vkSharedDataBlob);
				return nvigi::kResultItemNotFound;
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
				free(vkSharedDataBlob);
				return nvigi::kResultInvalidState;
			}

			CUdevice dev{};
			cuDeviceGet(&dev, devIndex);
			err = (*ptr_cuCtxCreate_v4)(&cuContext, &ctxCreateParams, 0, dev);
			if (err != CUDA_SUCCESS)
			{
				NVIGI_LOG_WARN("CiG could not create context! cuCtxCreate_v4 returned %d", err);
				free(vkSharedDataBlob);
				return nvigi::kResultDriverOutOfDate;
			}

			err = cuCtxSetCurrent(existingCtx);
			if (err != CUDA_SUCCESS)
			{
				NVIGI_LOG_WARN("CiG could not restore previous context!");
				free(vkSharedDataBlob);
				return nvigi::kResultInvalidState;
			}

			// Clean up the shared data blob
			free(vkSharedDataBlob);

			return nvigi::kResultOk;
		}
	}
}
