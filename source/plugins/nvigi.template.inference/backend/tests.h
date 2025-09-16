// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//
#include "source/plugins/nvigi.template.inference/nvigi_template_infer.h"

namespace nvigi
{
namespace tmpl
{

//! Make sure to apply appropriate tags for your tests ([gpu], [cpu], [cuda], [trt], [grpc], [nvcf], [d3d12] etc. see other tests for examples)
//! 
//! This allows easy unit test filtering as described here https://github.com/catchorg/Catch2/blob/devel/docs/command-line.md#specifying-which-tests-to-run
//! 
TEST_CASE("template_backend_inference", "[tag1],[tag2]")
{
    //! Use global params as needed (see source/tests/ai/main.cpp for details and add modify if required)

    nvigi::ITemplateInfer* itemplate{};
    nvigiGetInterfaceDynamic(plugin::tmpl_infer::kIdBackendApi, &itemplate, params.nvigiLoadInterface);
    REQUIRE(itemplate != nullptr);

    // Get instance(s) from your interface, run tests, check results etc.
    nvigi::TemplateInferCreationParameters createParams{};
    nvigi::CommonCreationParameters commonParams{};
    createParams.chain(commonParams);
	commonParams.utf8PathToModels = params.modelDir.c_str();
    commonParams.modelGUID = "{01234567-0123-0123-0123-0123456789AB}";

    nvigi::Result result;
    nvigi::InferenceInstance* templateInstance{};
    result = itemplate->createInstance(createParams, &templateInstance);
	REQUIRE(result == nvigi::kResultOk);

    result = itemplate->destroyInstance(templateInstance);
    REQUIRE(result == nvigi::kResultOk);

    auto res = params.nvigiUnloadInterface(plugin::tmpl_infer::kIdBackendApi, itemplate);
    REQUIRE(res == nvigi::kResultOk);
}

//! Add more test cases as needed

}
}
