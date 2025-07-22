// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

namespace nvigi
{
using namespace Microsoft::WRL;

struct D3D12ContextInfo {
    typedef std::function<bool(D3D12ContextInfo&)> SwapFunc;

    HWND window{};
    ComPtr <IDXGIAdapter> adapter{};
    ComPtr <IDXGIFactory2> factory{};
    ComPtr<ID3D12Device> device{};
    ComPtr<ID3D12CommandQueue> d3d_queue{};
    ComPtr<IDXGISwapChain1> swap_chain{};
    nvigi::thread::WorkerThread* swap_thread{};
    bool run_swap_loop = true;
    SwapFunc swap_loop_func{};

    // TODO: ideally we would use the same window API for both DX and VK, making cross-platform support easier (e.g., use glfw everywhere)
    static D3D12ContextInfo* CreateD3D12WindowAndSwapchain(int width, int height, bool make_swap_chain = true)
    {
        D3D12ContextInfo* p = new D3D12ContextInfo;
        UINT dxgi_factory_flags = 0;

#ifdef _DEBUG
        {
            // Enable the debug layer (requires the Graphics Tools "optional feature").
            // NOTE: Enabling the debug layer after device creation will invalidate the active device.
            {
                ComPtr<ID3D12Debug> debug_controller;
                if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
                {
                    debug_controller->EnableDebugLayer();

                    // Enable additional debug layers.
                    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
                }
            }
        }
#endif

        HRESULT hres = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&p->factory));

        unsigned int adapter_no = 0;
        while (SUCCEEDED(hres))
        {
            ComPtr<IDXGIAdapter> p_adapter;
            hres = p->factory->EnumAdapters(adapter_no, &p_adapter);

            if (SUCCEEDED(hres))
            {

                DXGI_ADAPTER_DESC a_desc;
                p_adapter->GetDesc(&a_desc);

                // NVDA adapter
                if (a_desc.VendorId == 0x10DE)
                {
                    p->adapter = p_adapter;
                    break;
                }
            }

            adapter_no++;
        }

        if (!p->adapter)
        {
            NVIGI_LOG_TEST_ERROR("No NV adapter found!");
            delete p;
            return nullptr;
        }

        if (make_swap_chain)
        {
            HINSTANCE h_instance = GetModuleHandle(NULL);
            WNDCLASSEX window_class = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, DefWindowProc,
                0L, sizeof(void*), h_instance, NULL, NULL, NULL, NULL, L"NVIGI_TEST", NULL };

            RegisterClassEx(&window_class);

            p->window = CreateWindowEx(
                0,
                window_class.lpszClassName,
                L"NVIGI_TEST",
                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                width,
                height,
                GetDesktopWindow(),
                NULL,
                h_instance,
                NULL
            );
            if (!p->window)
            {
                NVIGI_LOG_TEST_ERROR("failed to create a window");
                delete p;
                return nullptr;
            }
        }

        if (!SUCCEEDED(D3D12CreateDevice(
            p->adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&p->device))
        ))
        {
            NVIGI_LOG_TEST_ERROR("failed to create a D32D12 device");
            delete p;
            return nullptr;
        }

        // create command queue for the device
        D3D12_COMMAND_QUEUE_DESC commandqueue_desc;
        commandqueue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        commandqueue_desc.NodeMask = 0;
        commandqueue_desc.Priority = 0;
        commandqueue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        if (!SUCCEEDED(p->device->CreateCommandQueue(&commandqueue_desc, __uuidof(ID3D12CommandQueue), (void**)&p->d3d_queue)))
        {
            NVIGI_LOG_TEST_ERROR("failed to create a D32D12 queue");
            delete p;
            return nullptr;
        }

        RECT client_rect;
        GetClientRect(p->window, &client_rect);

        UINT window_width = client_rect.right - client_rect.left;
        UINT window_height = client_rect.bottom - client_rect.top;

        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
        swap_chain_desc.Width = window_width;
        swap_chain_desc.Height = window_height;
        swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.SampleDesc.Count = 1;
        swap_chain_desc.SampleDesc.Quality = 0;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.BufferCount = 2;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_chain_desc.Flags = 0;

        if (make_swap_chain)
        {
            if (!SUCCEEDED(p->factory->CreateSwapChainForHwnd(p->d3d_queue.Get(), p->window, &swap_chain_desc, NULL, NULL, &p->swap_chain)))
            {
                NVIGI_LOG_TEST_ERROR("failed to create a swap chain");
                delete p;
                return nullptr;
            }
        }

        active_instance = p;
        return p;
    }

    int LaunchSwapThread(SwapFunc&_swap_loop_func)
    {
        if (!window || !swap_chain)
            return - 1;

        swap_loop_func = _swap_loop_func;
        auto swapWork = [this]()->void {
            while (run_swap_loop)
            {
                if (swap_chain != nullptr)
                {
                    if (!SUCCEEDED(swap_chain->Present(0, 0)))
                        break;
                }
                if (!swap_loop_func(*this))
                    run_swap_loop = false;
            }
            NVIGI_LOG_TEST_INFO("quitting swap!");
        };

        swap_thread = new nvigi::thread::WorkerThread(L"nvigi.test.swap", THREAD_PRIORITY_HIGHEST);
        swap_thread->scheduleWork(swapWork);

        return 0;
    }

    void WaitForIdle()
    {
        if (d3d_queue)
        {
            ID3D12Fence* d3d_fence{};
            device->CreateFence(1, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d_fence));
            d3d_fence->Signal(0); // Reset value on the CPU
            d3d_queue->Signal(d3d_fence, UINT64(-1)); // Signal it on the GPU
            d3d_fence->SetEventOnCompletion(UINT64(-1), nullptr);
            d3d_fence->Release();
        }
    }

    ~D3D12ContextInfo()
    {
        WaitForIdle();
        if (window)
            CloseWindow(window);
        swap_chain = nullptr;
        d3d_queue = nullptr;
        device = nullptr;
        adapter = nullptr;
        factory = nullptr;

        active_instance = nullptr;
    }

    static D3D12ContextInfo* GetActiveInstance() { return active_instance; }

    private:
        D3D12ContextInfo() {}

        static D3D12ContextInfo* active_instance;
};
D3D12ContextInfo* D3D12ContextInfo::active_instance = nullptr;
}