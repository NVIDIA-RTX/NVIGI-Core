// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once
#include "nvigi.h"
#include "source/core/nvigi.api/nvigi_d3d12.h"
#include "source/core/nvigi.api/nvigi_cuda.h"

namespace nvigi
{
	namespace cudaScg
	{
		Result CreateSharedCUDAContext(ID3D12Device* device, ID3D12CommandQueue* queue, CUcontext& cuContext);
	}
}
