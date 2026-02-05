// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once

#include <cupti.h>
#include <unordered_set>
#include <unordered_map>
#ifdef NVIGI_WINDOWS
#include <cig_scheduler_settings.h>
#endif

#include "nvigi_cuda.h"
#include "nvigi_hwi_cuda.h"
#include <vulkan/vulkan.h>

#define ENABLE_VERBOSE_CIG_LOGGING 0

namespace CIGCompatibilityChecker
{
#ifdef NVIGI_WINDOWS
    // We're using the NVIDIA CUPTI library to logs CUDA kernel launch events 
    // to a buffer (called an activity buffer). Every time the buffer gets full
    // (or is flushed) it calls CIGCompatibilityChecker::bufferCompletedCallback().

    void bufferRequestedCallback(
        uint8_t** ppBuffer,
        size_t* pSize,
        size_t* pMaxNumRecords);
    void bufferCompletedCallback(
        CUcontext context,
        uint32_t streamId,
        uint8_t* pBuffer,
        size_t size,
        size_t validSize);
    void cudaApiCallback(
        void* pUserdata,
        CUpti_CallbackDomain domain,
        CUpti_CallbackId callbackId,
        const CUpti_CallbackData* pCallbackData);

    // The state we care about is whether the shared memory usage of each 
    // kernel seen so far is within the CIG limit determined by the hardware and 
    // CUDA, currently 47KB. As this state has to be passed to CUPTI callbacks,
    // it is global.
    struct CheckerState
    {
        // Shared memory checker
        std::atomic<bool> bytesUsedIsCigCompatible = true;
        uint32_t maxSharedMemBytesForCig;

        // Context checker
        std::mutex mutex;
        std::unordered_set<CUcontext> nonCigContextsCreated;
        std::unordered_set<CUcontext> cigContextsCreated;
        std::unordered_set<CUcontext> nonCigContextsUsed;
        std::unordered_set<CUcontext> cigContextsUsed;
        std::unordered_set<uint32_t> unhandledCudaApiFunctionIds;

        // Priority setting checker
        std::array<std::atomic<size_t>, CIG_WORKLOAD_MAX> launchesOfType = { 0,0,0 };
        bool launchesOfTypeIsValid = false;

        void reset()
        {
            // Note that we deliberately don't reset nonCigContextsCreated and cigContextsCreated
            // because we share a CIG context between the tests

            const std::lock_guard<std::mutex> lock(mutex);
            bytesUsedIsCigCompatible = true;
            nonCigContextsUsed.clear();
            cigContextsUsed.clear();
        }
    };
    CheckerState gCheckerState;
    nvigi::CudaParameters gCudaParameters{};
    CUcontext gCtxBeforeTest{};
    CUpti_SubscriberHandle gCuptiSubscriber{};
    nvigi::D3D12Parameters gD3dParameters{};
    nvigi::VulkanParameters gVulkanParameters{};
    CigSchedulerSettingsAPI sched{};

#define checkCuErrors(err)  __checkCuErrors (err, __FILE__, __LINE__)
    inline void __checkCuErrors(CUresult err, const char* file, const int line)
    {
        if (CUDA_SUCCESS != err)
        {
            const char* errorString = nullptr;
            cuGetErrorString(err, &errorString);
            printf("CUDA Error = %04d \"%s\" from file <%s>, line %i.\n",
                err, errorString, file, line);

            exit(EXIT_FAILURE);
        }
    }

#define checkCuptiErrors(err)  __checkCuptiErrors (err, __FILE__, __LINE__)
    inline void __checkCuptiErrors(CUptiResult err, const char* file, const int line)
    {
        if (CUDA_SUCCESS != err)
        {
            const char* errorString = nullptr;
            cuptiGetResultString(err, &errorString);
            printf("CUPTI Error = %04d \"%s\" from file <%s>, line %i.\n",
                err, errorString, file, line);

            exit(EXIT_FAILURE);
        }
    }

#define checkDxErrors(err)  __checkDxErrors (err, __FILE__, __LINE__)
    inline void __checkDxErrors(HRESULT err, const char* file, const int line)
    {
        if (FAILED(err))
        {
            LPVOID lpMsgBuf;

            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                err,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&lpMsgBuf,
                0, NULL);

            printf("DX Error = %04d \"%s\" from file <%s>, line %i.\n",
                err, (char*)lpMsgBuf, file, line);

            exit(EXIT_FAILURE);
        }
    }

#define checkAimErrors(err)  __checkAimErrors (err, __FILE__, __LINE__)
    inline void __checkAimErrors(nvigi::Result err, const char* file, const int line)
    {
        if (CUDA_SUCCESS != err)
        {
            printf("NvIgi Error = %04d from file <%s>, line %i.\n",
                err, file, line);

            exit(EXIT_FAILURE);
        }
    }

#define checkVkErrors(err)  __checkVkErrors (err, __FILE__, __LINE__)
    inline void __checkVkErrors(VkResult err, const char* file, const int line)
    {
        if (VK_SUCCESS != err)
        {
            printf("Vulkan Error = %04d from file <%s>, line %i.\n",
                err, file, line);

            exit(EXIT_FAILURE);
        }
    }

    nvigi::D3D12Parameters initCIG(PFun_nvigiLoadInterface* nvigiLoadInterface)
    {
        if (!gD3dParameters.queue)
        {
            // Adapter
            IDXGIFactory4* dxgiFactory;
            IDXGIAdapter1* dxgiAdapter;
            int i = 0;
            std::vector<IDXGIAdapter1*> adapterList;

            HRESULT err = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
            checkDxErrors(err);

            while (dxgiFactory->EnumAdapters(i++, (IDXGIAdapter**)&dxgiAdapter) !=
                DXGI_ERROR_NOT_FOUND)
            {
                DXGI_ADAPTER_DESC adapterDesc;
                dxgiAdapter->GetDesc(&adapterDesc);
                adapterList.push_back(dxgiAdapter);
            }

            int deviceIndex = 0;
            IDXGIAdapter1* dxAdapter = adapterList[deviceIndex];

            // Device
            err = D3D12CreateDevice(
                dxAdapter,
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&gD3dParameters.device));
            checkDxErrors(err);

            // Command queue
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            err = gD3dParameters.device->CreateCommandQueue(
                &queueDesc, IID_PPV_ARGS(&gD3dParameters.queue));
            checkDxErrors(err);
        }
        return gD3dParameters;
    }

    nvigi::VulkanParameters initVulkanCIG(PFun_nvigiLoadInterface* nvigiLoadInterface)
    {
        if (!gVulkanParameters.queue)
        {
            // Create Vulkan instance
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "CIG Compatibility Checker";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_0;

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;

            // No validation layers or extensions needed for basic usage
            createInfo.enabledLayerCount = 0;
            std::vector<const char*> extensionNames = { VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME };
            createInfo.ppEnabledExtensionNames = extensionNames.data();
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());

            VkResult vkErr = vkCreateInstance(&createInfo, nullptr, &gVulkanParameters.instance);
            checkVkErrors(vkErr);

            // Enumerate physical devices
            uint32_t deviceCount = 0;
            vkErr = vkEnumeratePhysicalDevices(gVulkanParameters.instance, &deviceCount, nullptr);
            checkVkErrors(vkErr);

            if (deviceCount == 0)
            {
                printf("Vulkan Error: No physical devices found\n");
                exit(EXIT_FAILURE);
            }

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkErr = vkEnumeratePhysicalDevices(gVulkanParameters.instance, &deviceCount, devices.data());
            checkVkErrors(vkErr);

            // Select the first device (similar to D3D12 approach)
            gVulkanParameters.physicalDevice = devices[0];

            // Find queue families
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(gVulkanParameters.physicalDevice, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(gVulkanParameters.physicalDevice, &queueFamilyCount, queueFamilies.data());

            uint32_t graphicsQueueFamily = UINT32_MAX;
            uint32_t computeQueueFamily = UINT32_MAX;
            uint32_t transferQueueFamily = UINT32_MAX;

            // Find graphics queue family
            for (uint32_t i = 0; i < queueFamilyCount; i++)
            {
                if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    graphicsQueueFamily = i;
                    break;
                }
            }

            // Find dedicated compute queue family (preferred) or use graphics queue
            for (uint32_t i = 0; i < queueFamilyCount; i++)
            {
                if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                    !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    computeQueueFamily = i;
                    break;
                }
            }
            if (computeQueueFamily == UINT32_MAX)
            {
                computeQueueFamily = graphicsQueueFamily; // Fallback to graphics queue
            }

            // Find dedicated transfer queue family (preferred) or use graphics queue
            for (uint32_t i = 0; i < queueFamilyCount; i++)
            {
                if ((queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                    !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                    !(queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
                {
                    transferQueueFamily = i;
                    break;
                }
            }
            if (transferQueueFamily == UINT32_MAX)
            {
                transferQueueFamily = graphicsQueueFamily; // Fallback to graphics queue
            }

            // Create device queues
            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::unordered_set<uint32_t> uniqueQueueFamilies = {
                graphicsQueueFamily, computeQueueFamily, transferQueueFamily
            };

            float queuePriority = 1.0f;
            for (uint32_t queueFamily : uniqueQueueFamilies)
            {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;
                queueCreateInfos.push_back(queueCreateInfo);
            }

            // Create logical device
            VkPhysicalDeviceFeatures deviceFeatures{};

            // Check if external compute queue extension is supported
            uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(gVulkanParameters.physicalDevice, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(gVulkanParameters.physicalDevice, nullptr, &extensionCount, availableExtensions.data());

            std::vector<const char*> deviceExtensions;
            bool externalComputeQueueSupported = false;

            for (const auto& extension : availableExtensions) {
                if (strcmp(extension.extensionName, "VK_NV_external_compute_queue") == 0) {
                    externalComputeQueueSupported = true;
                    deviceExtensions.push_back("VK_NV_external_compute_queue");
                    printf("CIG: Enabled VK_NV_external_compute_queue extension\n");
                    break;
                }
            }

            if (!externalComputeQueueSupported) {
                printf("CIG Warning: VK_NV_external_compute_queue extension not supported by this device\n");
            }

            // Setup VkExternalComputeQueueDeviceCreateInfoNV if the extension is supported
            VkExternalComputeQueueDeviceCreateInfoNV externalComputeQueueCreateInfo{};
            if (externalComputeQueueSupported) {
                externalComputeQueueCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_COMPUTE_QUEUE_DEVICE_CREATE_INFO_NV;
                externalComputeQueueCreateInfo.pNext = nullptr;
                externalComputeQueueCreateInfo.reservedExternalQueues = 1; // We only need one external queue
                printf("CIG: Configured for %d reserved external compute queues\n", externalComputeQueueCreateInfo.reservedExternalQueues);
            }

            VkDeviceCreateInfo deviceCreateInfo{};
            deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceCreateInfo.pNext = externalComputeQueueSupported ? &externalComputeQueueCreateInfo : nullptr;
            deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
            deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
            deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
            deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();
            deviceCreateInfo.enabledLayerCount = 0;

            vkErr = vkCreateDevice(gVulkanParameters.physicalDevice, &deviceCreateInfo, nullptr, &gVulkanParameters.device);
            checkVkErrors(vkErr);

            // Get Vulkan external compute queue properties
            VkPhysicalDeviceExternalComputeQueuePropertiesNV externalComputeProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_COMPUTE_QUEUE_PROPERTIES_NV };
            VkPhysicalDeviceProperties2 physDevProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
            physDevProps.pNext = &externalComputeProperties;
            vkGetPhysicalDeviceProperties2(gVulkanParameters.physicalDevice, &physDevProps);

            printf("CIG: externalDataSize %d\n", externalComputeProperties.externalDataSize);
            printf("CIG: maxExternalQueues %d\n", externalComputeProperties.maxExternalQueues);

            // Get queue handles
            vkGetDeviceQueue(gVulkanParameters.device, graphicsQueueFamily, 0, &gVulkanParameters.queue);
            vkGetDeviceQueue(gVulkanParameters.device, computeQueueFamily, 0, &gVulkanParameters.queueCompute);
            vkGetDeviceQueue(gVulkanParameters.device, transferQueueFamily, 0, &gVulkanParameters.queueTransfer);
        }
        return gVulkanParameters;
    }

    // Common pre-initialization setup
    void initPre()
    {
        CUresult cuerr = cuInit(0);
        checkCuErrors(cuerr);

        // Save the current CUDA context so that we can restore it at the end 
        // of the test. This is because a test could change the current context,
        // and we don't want that to affect other tests.
        cuerr = cuCtxGetCurrent(&gCtxBeforeTest);
        checkCuErrors(cuerr);

        gCheckerState.reset();

#ifndef NVIGI_DISABLE_CUPTI
        // Register callbacks to allow CUPTI to ask us to allocate activity 
        // buffers, and notify us when they are full or flushed
        CUptiResult cuptierr = cuptiActivityRegisterCallbacks(bufferRequestedCallback,
            bufferCompletedCallback);
        if (cuptierr == CUDA_SUCCESS)
        {
            cuptierr = cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);
            checkCuptiErrors(cuptierr);
            cuptierr = cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONTEXT);
            checkCuptiErrors(cuptierr);

            // Callback for synchronous handling of CUDA API calls
            cuptierr = cuptiSubscribe(&gCuptiSubscriber, (CUpti_CallbackFunc)cudaApiCallback, &gCheckerState);
            checkCuptiErrors(cuptierr);

            // Subscribe to CUDA driver API, not cudart
            cuptierr = cuptiEnableDomain(1, gCuptiSubscriber, CUPTI_CB_DOMAIN_DRIVER_API);
            checkCuptiErrors(cuptierr);

            // Enable the state domain callbacks for CUPTI error reporting
            cuptierr = cuptiEnableDomain(1, gCuptiSubscriber, CUPTI_CB_DOMAIN_STATE);
            checkCuptiErrors(cuptierr);
        }
        else
        {
            NVIGI_LOG_WARN_ONCE("Skipping CUPTI due to errors, most likely running on new HW")
        }
#endif
    }

    // Common post-initialization setup
    void initPost(CUcontext cigContext)
    {
        HMODULE dll = LoadLibraryA("cig_scheduler_settings.dll");
        if (!dll)
        {
            printf("Error loading cig_scheduler_settings.dll\n");
            exit(EXIT_FAILURE);
        }

        sched.StreamSetWorkloadType = (PFun_StreamSetWorkloadType*)GetProcAddress(dll, "StreamSetWorkloadType");
        sched.ContextSetDefaultWorkloadType = (PFun_ContextSetDefaultWorkloadType*)GetProcAddress(dll, "ContextSetDefaultWorkloadType");
        sched.StreamGetWorkloadType = (PFun_StreamGetWorkloadType*)GetProcAddress(dll, "StreamGetWorkloadType");
        sched.ContextGetDefaultWorkloadType = (PFun_ContextGetDefaultWorkloadType*)GetProcAddress(dll, "ContextGetDefaultWorkloadType");
        sched.WorkloadTypeGetName = (PFun_WorkloadTypeGetName*)GetProcAddress(dll, "WorkloadTypeGetName");

        if (cigContext)
        {
            CUresult cuerr = cuCtxSetCurrent(cigContext);
            checkCuErrors(cuerr);

            size_t availableSharedMemory = 0;
            int reservedSharedMemory = 0;

            size_t cig{};
            cuerr = cuCtxGetLimit(&cig, CU_LIMIT_CIG_ENABLED);
            checkCuErrors(cuerr);
            cuerr = cuCtxGetLimit(&availableSharedMemory, CU_LIMIT_SHMEM_SIZE);
            checkCuErrors(cuerr);
            cuDeviceGetAttribute(&reservedSharedMemory, CU_DEVICE_ATTRIBUTE_RESERVED_SHARED_MEMORY_PER_BLOCK, 0);
            checkCuErrors(cuerr);
            gCheckerState.maxSharedMemBytesForCig = (uint32_t)availableSharedMemory - reservedSharedMemory;

            NVIGI_LOG_INFO_ONCE("CIG Info: max shared memory bytes for CIG = %d (CTX is %s)\n", gCheckerState.maxSharedMemBytesForCig,
                cig ? "SUPPORTED" : "NOT SUPPORTED");

            cuerr = cuCtxSetCurrent(gCtxBeforeTest);
            checkCuErrors(cuerr);
        }
    }

    // Call at start of test - initializes D3D
    nvigi::D3D12Parameters init(PFun_nvigiLoadInterface* nvigiLoadInterface, PFun_nvigiUnloadInterface* nvigiUnloadInterface, bool useCIG = true)
    {
        initPre();

        nvigi::D3D12Parameters cigParameters;
        CUcontext cigContext;
        if (useCIG)
        {
            cigParameters = initCIG(nvigiLoadInterface);

            // To get the max amount of shared memory supported by CIG we need to 
            // create a CIG context. Note that we don't pass this context to the
            // plugins under test, because we want to test the plugin's own
            // call to create the CIG context
            nvigi::IHWICuda* icig = nullptr;
            bool success = nvigiGetInterfaceDynamic(nvigi::plugin::hwi::cuda::kId, &icig, nvigiLoadInterface);
            nvigi::Result igierr = icig->cudaGetSharedContextForQueue(cigParameters, &cigContext);
            checkAimErrors(igierr);

            nvigiUnloadInterface(nvigi::plugin::hwi::cuda::kId, icig);
        }
        else
        {
            cigContext = gCtxBeforeTest;
        }

        initPost(cigContext);
        return cigParameters;
    }

    // Call at start of test - initializes Vulkan
    nvigi::VulkanParameters initVulkan(PFun_nvigiLoadInterface* nvigiLoadInterface, PFun_nvigiUnloadInterface* nvigiUnloadInterface, bool useCIG = true)
    {
        initPre();

        nvigi::VulkanParameters cigParameters;
        CUcontext cigContext;
        if (useCIG)
        {
            cigParameters = initVulkanCIG(nvigiLoadInterface);

            // To get the max amount of shared memory supported by CIG we need to 
            // create a CIG context. Note that we don't pass this context to the
            // plugins under test, because we want to test the plugin's own
            // call to create the CIG context
            nvigi::IHWICuda* icig = nullptr;
            bool success = nvigiGetInterfaceDynamic(nvigi::plugin::hwi::cuda::kId, &icig, nvigiLoadInterface);
            nvigi::Result igierr = icig->cudaGetSharedContextForVulkanQueue(cigParameters, &cigContext);
            checkAimErrors(igierr);

            nvigiUnloadInterface(nvigi::plugin::hwi::cuda::kId, icig);
        }
        else
        {
            cigContext = gCtxBeforeTest;
        }

        initPost(cigContext);
        return cigParameters;
    }

    // Call at end of test
    bool check(bool useCIG = true, int ordinalOfRequestedCudaDevice=-1)
    {
#ifdef NVIGI_DISABLE_CUPTI
        return true;
#endif
        const std::lock_guard<std::mutex> lock(gCheckerState.mutex);

        // Make sure that we get all the activity events
        CUptiResult cuptierr = cuptiActivityFlushAll(1);
        if (cuptierr != CUDA_SUCCESS) return true;

#if ENABLE_VERBOSE_CIG_LOGGING
        printf("\n");
        printf("CIG Info: CIG contexts created: ");
        for (CUcontext context : gCheckerState.cigContextsCreated)
        {
            printf("%p ", context);
        }
        printf("\n");

        printf("CIG Info: non-CIG contexts created: ");
        for (CUcontext context : gCheckerState.nonCigContextsCreated)
        {
            printf("%p ", context);
        }
        printf("\n");
#endif

        // Make mapping from device to ordinal for pretty display
        std::unordered_map<CUdevice, int> deviceToOrdinal;
        int deviceCount;
        CUresult err = cuDeviceGetCount(&deviceCount);
        checkCuErrors(err);
        for (int i = 0; i != deviceCount; i++)
        {
            CUdevice device;
            cuDeviceGet(&device, i);
            deviceToOrdinal[device] = i;
        }

        // Struct to hold device information
        struct DeviceInfo {
            int ordinal;
            char name[256];
        };

        // Lambda to get device info from a context
        auto getDeviceInfoFromContext = [&deviceToOrdinal](CUcontext context) -> DeviceInfo {
            DeviceInfo info;
            info.ordinal = -1;
            info.name[0] = '\0';

            // Save the current context so we can restore it after querying
            CUcontext savedContext;
            CUresult err = cuCtxGetCurrent(&savedContext);
            checkCuErrors(err);
            
            // Switch to the context we want to query
            err = cuCtxSetCurrent(context);
            checkCuErrors(err);
            
            CUdevice device;
            err = cuCtxGetDevice(&device);
            checkCuErrors(err);
            
            auto iter = deviceToOrdinal.find(device);
            if (iter != deviceToOrdinal.end())
            {
                info.ordinal = iter->second;
            }

            err = cuDeviceGetName(info.name, sizeof(info.name), device);
            checkCuErrors(err);
            
            // Restore the original context
            err = cuCtxSetCurrent(savedContext);
            checkCuErrors(err);

            return info;
        };

        bool runningOnRequestedDevice = true;

        if (gCheckerState.cigContextsUsed.size() != 0)
        {
            printf("CIG Info: CIG contexts used: ");
            for (CUcontext context : gCheckerState.cigContextsUsed)
            {
                DeviceInfo info = getDeviceInfoFromContext(context);
                if (ordinalOfRequestedCudaDevice != -1 && info.ordinal != ordinalOfRequestedCudaDevice) runningOnRequestedDevice = false;
                printf("%p (device %d: %s) ", context, info.ordinal, info.name);
            }
            printf("\n");
        }

        if (gCheckerState.nonCigContextsUsed.size() != 0)
        {
            printf("CIG Compatibility Error: the following non-CIG contexts were used: ");
            for (CUcontext context : gCheckerState.nonCigContextsUsed)
            {
                DeviceInfo info = getDeviceInfoFromContext(context);
                if (ordinalOfRequestedCudaDevice != -1 && info.ordinal != ordinalOfRequestedCudaDevice) runningOnRequestedDevice = false;
                printf("%p (device %d: %s) ", context, info.ordinal, info.name);
            }
            printf("\n");
        }

        if (!runningOnRequestedDevice && ordinalOfRequestedCudaDevice != -1)
        {
            printf("Multi-GPU Compatibility Error: CUDA work was run on a device other than requested device %d\n", ordinalOfRequestedCudaDevice);
        }

        if (gCheckerState.launchesOfTypeIsValid)
        {
            printf("CIG Info: Launches of each workload type: \n");
            for (int i = 0; i != CIG_WORKLOAD_MAX; i++)
            {
                size_t launchCount = gCheckerState.launchesOfType[i];
                printf("%30s: %zu\n", sched.WorkloadTypeGetName(CigWorkloadType(i)), launchCount);
            }
        }
        else
        {
            printf("CIG Info: Could not test CIG priorities. Please use 575 driver or higher\n");
        }

#if ENABLE_VERBOSE_CIG_LOGGING
        printf("CIG Info: unhandled CUDA API function Ids:\n");
        for (uint32_t id : gCheckerState.unhandledCudaApiFunctionIds)
        {
            printf("  %u\n", id);
        }
        printf("\n");
#endif

        // Unregister CUPTI callbacks
        cuptierr = cuptiActivityDisable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL);
        checkCuptiErrors(cuptierr);
        cuptierr = cuptiActivityDisable(CUPTI_ACTIVITY_KIND_CONTEXT);
        checkCuptiErrors(cuptierr);
        cuptierr = cuptiUnsubscribe(gCuptiSubscriber);
        checkCuptiErrors(cuptierr);


        // Restore CUDA context to what is was before test started
        CUresult cuerr = cuCtxSetCurrent(gCtxBeforeTest);
        checkCuErrors(cuerr);

        bool contextsOk =
            gCheckerState.nonCigContextsUsed.size() == 0;

        return !useCIG || (gCheckerState.bytesUsedIsCigCompatible && contextsOk);
    }

    // Size and alignment of the activity buffer
    constexpr size_t activityBufferSize = 8 * 1024 * 1024; // 8 MB
    constexpr size_t alignSize = 8;
    uint8_t* alignBuffer(uint8_t* buffer, size_t align)
    {
        return ((uintptr_t)(buffer) & ((align)-1)) ?
            ((buffer)+(align)-((uintptr_t)(buffer) & ((align)-1))) : (buffer);
    }

    // CUPTI calls us when it needs a new activity buffer
    void bufferRequestedCallback(
        uint8_t** ppBuffer,
        size_t* pSize,
        size_t* pMaxNumRecords)
    {
        uint8_t* pBuffer = (uint8_t*)malloc(activityBufferSize + alignSize);

        *pSize = activityBufferSize;
        *ppBuffer = alignBuffer(pBuffer, alignSize);
        *pMaxNumRecords = 0;
    }

    // Process a single activity record from CUPTI
    void processActivity(
        CUpti_Activity* pRecord,
        CheckerState* pCheckerState)
    {
        CUpti_ActivityKind activityKind = pRecord->kind;

        if (activityKind == CUPTI_ACTIVITY_KIND_KERNEL ||
            activityKind == CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL)
        {
            CUpti_ActivityKernel9* pKernelRecord = (CUpti_ActivityKernel9*)pRecord;

            uint32_t totalSharedMemBytes = pKernelRecord->staticSharedMemory +
                pKernelRecord->dynamicSharedMemory;

            if (pCheckerState->maxSharedMemBytesForCig && (pCheckerState->maxSharedMemBytesForCig < totalSharedMemBytes))
            {
                pCheckerState->bytesUsedIsCigCompatible = false;

                // Log the name and shared memory usage of the kernel that is 
                // using too much shared memory to allow the user to optimize it
                printf("CIG Compatibility Error: Kernel %s uses %u bytes of shared memory,"
                    "and maximum allowed by CIG is %u\n",
                    pKernelRecord->name, totalSharedMemBytes,
                    pCheckerState->maxSharedMemBytesForCig);
            }
        }
    }

    // Process a buffer of activity records
    void processActivityBuffer(
        uint8_t* pBuffer,
        size_t validBytes,
        CheckerState* pCheckerState)
    {
        CUpti_Activity* pRecord = NULL;
        CUptiResult status = CUPTI_SUCCESS;

        do {
            status = cuptiActivityGetNextRecord(pBuffer, validBytes, &pRecord);
            if (status == CUPTI_SUCCESS)
            {
                processActivity(pRecord, pCheckerState);
            }
            else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED)
            {
                break;
            }
            else
            {
                const char* pErrorString;
                cuptiGetResultString(status, &pErrorString);
                printf("Error: cuptiActivityGetNextRecord with (%d): %s.\n",
                    status, pErrorString);
                exit(0);
            }
        } while (1);
    }

    // CUPTI calls us when the activity buffer is full, or when it is flushed, 
    // for example when the test is over
    void bufferCompletedCallback(
        CUcontext context,
        uint32_t streamId,
        uint8_t* pBuffer,
        size_t size,
        size_t validSize)
    {
        if (validSize > 0)
        {
            processActivityBuffer(pBuffer, validSize, &gCheckerState);
        }

        free(pBuffer);
    }

    // This is for CUPTI error handling
    void handleDomainStateCallback(
        CUpti_CallbackId callbackId,
        const CUpti_StateData* pStateData)
    {
        if (callbackId == CUPTI_CBID_STATE_FATAL_ERROR)
        {
            const char* errorString = NULL;
            cuptiGetResultString(pStateData->notification.result, &errorString);

            printf("\nCUPTI encountered fatal error: %s\n", errorString);
            printf("Error message: %s\n", pStateData->notification.message);

            // Exiting the application if fatal error encountered in CUPTI
            // If there is a CUPTI fatal error, it means CUPTI has stopped profiling the application.
            exit(EXIT_FAILURE);
        }
    }

    // We get called at the start and end of every CUDA driver API call
    void cudaApiCallback(
        void* pUserdata,
        CUpti_CallbackDomain domain,
        CUpti_CallbackId callbackId,
        const CUpti_CallbackData* pCallbackData)
    {
        CheckerState* pCheckerState = (CheckerState*)pUserdata;

        if (domain == CUPTI_CB_DOMAIN_DRIVER_API)
        {
            if (pCallbackData->callbackSite == CUPTI_API_ENTER)
            {
                if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD || // 43
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemcpyHtoD || // 44
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH || // 45
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoH || // 46
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD || // 47
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoD || // 48
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA || // 49
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoA || // 50
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD || // 51
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemcpyAtoD || // 52
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA || // 53
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH || // 54
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA || // 55
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D || // 56
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned || // 57
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D || // 58
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64Memcpy3D || // 59
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync || // 60
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemcpyHtoDAsync || // 61
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync || // 62
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoHAsync || // 63
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync || // 64
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemcpyDtoDAsync || // 65
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync || // 66
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync || // 67
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync || // 68
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync || // 69
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64Memcpy3DAsync || // 70
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD8 || // 71
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemsetD8 || // 72
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD16 || // 73
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemsetD16 || // 74
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD32 || // 75
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemsetD32 || // 76
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD2D8 || // 77
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemsetD2D8 || // 78
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD2D16 || // 79
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemsetD2D16 || // 80
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD2D32 || // 81
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cu64MemsetD2D32 || // 82
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy_v2 || // 248
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD8_v2 || // 249
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD16_v2 || // 250
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD32_v2 || // 251
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD2D8_v2 || // 252
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD2D16_v2 || // 253
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemsetD2D32_v2 || // 254
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2 || // 276
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoDAsync_v2 || // 277
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2 || // 278
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoHAsync_v2 || // 279
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2 || // 280
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoDAsync_v2 || // 281
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2 || // 282
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoHAsync_v2 || // 283
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2 || // 284
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2 || // 285
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2 || // 286
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2D_v2 || // 287
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DUnaligned_v2 || // 288
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy2DAsync_v2 || // 289
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3D_v2 || // 290
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpy3DAsync_v2 || // 291
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2 || // 292
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoAAsync_v2 || // 293
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel || // 307
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel_ptsz || // 442
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuGraphLaunch || // 514
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuGraphLaunch_ptsz || // 515
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernelEx || // 652,
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernelEx_ptsz) // 653
                {
                    const std::lock_guard<std::mutex> lock(pCheckerState->mutex);

                    auto iter = pCheckerState->cigContextsCreated.find(pCallbackData->context);
                    bool usingCigContext = (iter != pCheckerState->cigContextsCreated.end());
                    if (usingCigContext)
                    {
                        pCheckerState->cigContextsUsed.insert(pCallbackData->context);
                    }
                    else
                    {
                        pCheckerState->nonCigContextsUsed.insert(pCallbackData->context);
                    }

                    if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel || // 307
                        callbackId == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel_ptsz || // 442
                        callbackId == CUPTI_DRIVER_TRACE_CBID_cuGraphLaunch || // 514
                        callbackId == CUPTI_DRIVER_TRACE_CBID_cuGraphLaunch_ptsz) // 653
                    {
                        CUstream stream{};

                        if (pCallbackData->functionParams)
                        {
                            if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel) // 307
                            {
                                cuLaunchKernel_params* params = (cuLaunchKernel_params*)pCallbackData->functionParams;
                                stream = params->hStream;
                            }
                            else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel_ptsz) // 442
                            {
                                cuLaunchKernel_ptsz_params* params = (cuLaunchKernel_ptsz_params*)pCallbackData->functionParams;
                                stream = params->hStream;
                            }
                            else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuGraphLaunch) // 514
                            {
                                cuGraphLaunch_params* params = (cuGraphLaunch_params*)pCallbackData->functionParams;
                                stream = params->hStream;
                            }
                            else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuGraphLaunch_ptsz) // 515
                            {
                                cuGraphLaunch_ptsz_params* params = (cuGraphLaunch_ptsz_params*)pCallbackData->functionParams;
                                stream = params->hStream;
                            }
                        }

                        if (stream)
                        {
                            CigWorkloadType workloadType;
                            CUresult cuerr = sched.StreamGetWorkloadType(stream, &workloadType);

                            // The preceding call can fail, for example if the user doesn't have the right driver
                            if (cuerr == CUDA_SUCCESS)
                            {
                                pCheckerState->launchesOfType[workloadType]++;

                                // We only check launchesOfType at the end of the test if the driver etc. was ok
                                pCheckerState->launchesOfTypeIsValid = true;
                            }
                        }
                    }
                }
            }
            else if (pCallbackData->callbackSite == CUPTI_API_EXIT)
            {
                if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuCtxCreate ||
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuCtxCreate_v2 ||
                    callbackId == CUPTI_DRIVER_TRACE_CBID_cuCtxCreate_v3)
                {
                    const std::lock_guard<std::mutex> lock(pCheckerState->mutex);

                    printf("CIG Info: Created non-CIG context: context=%p, contextId=%u\n", pCallbackData->context, pCallbackData->contextUid);
                    pCheckerState->nonCigContextsCreated.insert(pCallbackData->context);
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuCtxCreate_v4)
                {
                    const std::lock_guard<std::mutex> lock(pCheckerState->mutex);

                    cuCtxCreate_v4_params* params = (cuCtxCreate_v4_params*)pCallbackData->functionParams;

                    printf("CIG Info: Created CIG context: context=%p, contextId=%u, returned context=%p\n", pCallbackData->context, pCallbackData->contextUid, *params->pctx);

                    pCheckerState->cigContextsCreated.insert(*params->pctx);
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuCtxPushCurrent_v2) // 323
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    printf("CIG Info: cuCtxPushCurrent_v2: new context=%p\n", pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuCtxPopCurrent_v2) // 324
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    printf("CIG Info: cuCtxPopCurrent_v2: new context=%p\n", pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuInit) // 1
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    printf("CIG Info: cuInit: new context = %p\n", pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuCtxSetCurrent) // 322
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuCtxSetCurrent_params* params = (cuCtxSetCurrent_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuCtxSetCurrent: context=%p (context after call=%p)\n",
                        params->ctx, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuDevicePrimaryCtxRetain) // 330
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuDevicePrimaryCtxRetain_params* params = (cuDevicePrimaryCtxRetain_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuDevicePrimaryCtxRetain: device=%d, returned context=%p (context after call=%p)\n",
                        params->dev, *params->pctx, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemAlloc_v2) // 219
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuMemAlloc_v2_params* params = (cuMemAlloc_v2_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuMemAlloc_v2: size=%zu, returned dptr=%zu, context=%p\n",
                        params->bytesize, *params->dptr, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemAllocPitch_v2) // 220
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuMemAllocPitch_v2_params* params = (cuMemAllocPitch_v2_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuMemAllocPitch_v2: width=%zu, height=%zu, returned dptr=%zu, pitch=%zu, context=%p\n",
                        params->WidthInBytes, params->Height, *params->dptr, *params->pPitch, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemAllocManaged) // 396
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuMemAllocManaged_params* params = (cuMemAllocManaged_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuMemAllocManaged: size=%zu, flags=%u, returned dptr=%zu, context=%p\n",
                        params->bytesize, params->flags, *params->dptr, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemAllocAsync) // 682
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuMemAllocAsync_params* params = (cuMemAllocAsync_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuMemAllocAsync: size=%zu, stream=%p, returned dptr=%zu, context=%p\n",
                        params->bytesize, params->hStream, *params->dptr, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemAllocAsync_ptsz) // 683
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuMemAllocAsync_ptsz_params* params = (cuMemAllocAsync_ptsz_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuMemAllocAsync_ptsz: size=%zu, stream=%p, returned dptr=%zu, context=%p\n",
                        params->bytesize, params->hStream, *params->dptr, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemFree_v2) // 221
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuMemFree_v2_params* params = (cuMemFree_v2_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuMemFree_v2: dptr=%zu, context=%p\n",
                        params->dptr, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemFreeAsync) // 684
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuMemFreeAsync_params* params = (cuMemFreeAsync_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuMemFreeAsync: dptr=%zu, stream=%p, context=%p\n",
                        params->dptr, params->hStream, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMemFreeAsync_ptsz) // 685
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuMemFreeAsync_ptsz_params* params = (cuMemFreeAsync_ptsz_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuMemFreeAsync_ptsz: dptr=%zu, stream=%p, context=%p\n",
                        params->dptr, params->hStream, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuArrayCreate_v2) // 218
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuArrayCreate_v2_params* params = (cuArrayCreate_v2_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuArrayCreate_v2: returned array=%p, context=%p\n",
                        *params->pHandle, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuArray3DCreate_v2) // 233
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuArray3DCreate_v2_params* params = (cuArray3DCreate_v2_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuArray3DCreate_v2: returned array=%p, context=%p\n",
                        *params->pHandle, pCallbackData->context);
#endif
                }
                else if (callbackId == CUPTI_DRIVER_TRACE_CBID_cuMipmappedArrayCreate) // 234
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    cuMipmappedArrayCreate_params* params = (cuMipmappedArrayCreate_params*)pCallbackData->functionParams;
                    printf("CIG Info: cuMipmappedArrayCreate: returned array=%p, context=%p\n",
                        *params->pHandle, pCallbackData->context);
#endif
                }
                else
                {
#if ENABLE_VERBOSE_CIG_LOGGING
                    // Note: Don't lock mutex here as this callback may be triggered while check() holds the mutex
                    // (e.g., from cuDeviceGet, cuCtxGetDevice, etc. called inside check())
                    pCheckerState->unhandledCudaApiFunctionIds.insert(callbackId);
#endif
                }
            }
        }
        else if (domain == CUPTI_CB_DOMAIN_STATE)
        {
            if (callbackId == CUPTI_CBID_STATE_FATAL_ERROR)
            {
                handleDomainStateCallback(callbackId, (CUpti_StateData*)pCallbackData);
            }
        }
    }

    // Each plugin may do CUDA work in createInstance before it sets the CIG priority, so we
    // provide a way to reset the counters to count only work done after createInstance
    void resetLaunchCounters()
    {
        for (int i = 0; i != CIG_WORKLOAD_MAX; i++)
        {
            gCheckerState.launchesOfType[i] = 0;
        }
    }
#else
    nvigi::D3D12Parameters init(PFun_nvigiLoadInterface*, PFun_nvigiUnloadInterface*, bool = true)
    {
        return nvigi::D3D12Parameters{};
    }

    nvigi::VulkanParameters initVulkan(PFun_nvigiLoadInterface*, PFun_nvigiUnloadInterface*, bool = true)
    {
        return nvigi::VulkanParameters{};
    }

    void resetLaunchCounters() {}

    // Call at end of test
    bool check(bool = true) { return true; }
#endif
};
