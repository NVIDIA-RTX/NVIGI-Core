// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//
#include "source/plugins/nvigi.template/nvigi_template.h"

namespace nvigi
{
namespace tmpl
{

//! Make sure to apply appropriate tags for your tests ([gpu], [cpu], [cuda], [trt], [grpc], [nvcf], [d3d12] etc. see other tests for examples)
//! 
//! This allows easy unit test filtering as described here https://github.com/catchorg/Catch2/blob/devel/docs/command-line.md#specifying-which-tests-to-run
//! 
TEST_CASE("template_backend", "[tag1],[tag2]")
{
    //! Use global params as needed (see source/tests/ai/main.cpp for details and add modify if required)

    nvigi::ITemplate* itemplate{};
    nvigiGetInterfaceDynamic(plugin::tmpl::kIdBackendApi, &itemplate, params.nvigiLoadInterface);
    REQUIRE(itemplate != nullptr);

    // Get instance(s) from your interface, run tests, check results etc.

    auto res = params.nvigiUnloadInterface(plugin::tmpl::kIdBackendApi, itemplate);
    REQUIRE(res == nvigi::kResultOk);
}

//! Add more test cases as needed

}
}
