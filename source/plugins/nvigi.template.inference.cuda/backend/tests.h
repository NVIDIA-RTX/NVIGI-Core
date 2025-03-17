// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//
#include "source/plugins/nvigi.template.inference.cuda/nvigi_template_infer.h"
#include "source/utils/nvigi.cig_compatibility_checker/CIG_compatibility_checker.h"

namespace nvigi
{
    namespace tmpl
    {

        //! Make sure to apply appropriate tags for your tests ([gpu], [cpu], [cuda], [trt], [grpc], [nvcf], [d3d12] etc. see other tests for examples)
        //! 
        //! This allows easy unit test filtering as described here https://github.com/catchorg/Catch2/blob/devel/docs/command-line.md#specifying-which-tests-to-run
        //! 
        TEST_CASE("template_inference_cuda_backend", "[tag1],[tag2]")
        {
            // All tests for CUDA plugins must instantiate the CIGCompatibilityChecker
            // to ensure that the plugin will run correctly when run in parallel with 
            // graphics
#ifdef NVIGI_WINDOWS
            D3D12Parameters cigParameters;
            if (nvigi::params.useCiG)
            {
                cigParameters = CIGCompatibilityChecker::init(params.nvigiLoadInterface, params.nvigiUnloadInterface);
                REQUIRE(cigParameters);
            }
#endif
            
            //! Use global params as needed (see source/tests/ai/main.cpp for details and add modify if required)

            nvigi::ITemplateInferCuda* itemplate{};
            nvigiGetInterfaceDynamic(plugin::tmpl_infer_cuda::kId, &itemplate, params.nvigiLoadInterface);
            REQUIRE(itemplate != nullptr);

            nvigi::IHWICuda* icig{};
            nvigiGetInterfaceDynamic(nvigi::plugin::hwi::cuda::kId, &icig, nvigi::params.nvigiLoadInterface);
            REQUIRE(icig != nullptr);

            // Get instance(s) from your interface, run tests, check results etc.

            auto res = params.nvigiUnloadInterface(nvigi::plugin::hwi::cuda::kId, icig);
            REQUIRE(res == nvigi::kResultOk);
            res = params.nvigiUnloadInterface(plugin::tmpl_infer_cuda::kId, itemplate);
            REQUIRE(res == nvigi::kResultOk);

            // CIGCompatibilityChecker::check() must always go last to make sure that 
            // everything the test did was CIG compatible
#ifdef NVIGI_WINDOWS
            if (nvigi::params.useCiG)
            {
                REQUIRE(CIGCompatibilityChecker::check());
            }
#endif
        }

        //! Add more test cases as needed

    }
}

