// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi.h"
#include "source/plugins/nvigi.template/nvigi_template.h"
#include "source/servers/server.h"
#include "nvigi.extra/extra.h"

namespace nvigi
{

namespace tmpl
{

namespace nvcf
{

struct Context
{
    nvigi::ITemplate* _interface{};
    std::map<std::string,nvigi::InferenceInstance*> instances{};
};
Context ctx;

nvigi::Result addMicroService(Server& svr, const std::string& pathToModels, PFun_nvigiLoadInterface* nvigiLoadInterface)
{
    if (NVIGI_FAILED(error, nvigiGetInterfaceDynamic(plugin::tmpl::kBackendApi, &ctx._interface, nvigiLoadInterface)))
    {
        LOG_ERROR("slGPTGetInterface failed", { {"reason",error} });
        return error;
    }

    //! IMPORTANT: Make sure your URL is unique, consider maybe using feature id to ensure that
    //! 
    svr.Post("/my_url", [pathToModels](const Request& req, Response& res)
    {
        using namespace extra;

        TemplateCreationParameters params{};

        //! Assuming body is JSON, change as needed
        std::string modelGUID;
        try
        {
            auto body = json::parse(req.body);

            //! We expect request to tell us which model to use
            modelGUID = getJSONValue(body, "model", modelGUID);
            params.common->modelGUID = modelGUID.c_str();

            //! Extract additional parameters related to your model
            //! 
            //! This could be anything that is part of creation or runtime properties for the model
        }
        catch (std::exception& e)
        {
            LOG_ERROR("JSON exception", { {"what",e.what()} });
            res.status = 400;
            return;
        }

        //! Create our instance, normally model GUID comes in the request above
        nvigi::InferenceInstance* instance{};
        params.common->utf8PathToModels = pathToModels.c_str();

        if (ctx.instances.find(params.common->modelGUID) != ctx.instances.end())
        {
            instance = ctx.instances[params.common->modelGUID];
        }
        else
        {
            if (NVIGI_FAILED(error, ctx._interface->createInstance(params, &instance)))
            {
                LOG_ERROR("gpt::createInstance failed", { {"model",params.common->modelGUID} , {"reason",error} });
                return;
            }
            ctx.instances[params.common->modelGUID] = instance;
        }

        //! Setup your callback
        auto callbackTemplate = [](const nvigi::InferenceExecutionContext* ctx, nvigi::InferenceExecutionState state, void* data)->nvigi::InferenceExecutionState
        {
            if (ctx)
            {
                auto slots = &ctx->outputs;
                //const nvigi::InferenceDataText* text{};
                //slots->findAndValidateSlot(nvigi::kGPTDataSlotResponse, &text);
            }
            return state;
        };
        
        //! Setup and evaluate execution context
        {
            //nvigi::InferenceDataText data(prompt.c_str());
            std::vector<nvigi::InferenceDataSlot> inSlots;// = { {nvigi::kGPTDataSlotSystem, &data} };

            nvigi::InferenceExecutionContext ctx{};
            ctx.instance = instance;
            ctx.callback = callbackTemplate;
            ctx.callbackUserData = nullptr;
            ctx.inputs = { inSlots.size(), inSlots.data() };
            if (NVIGI_FAILED(error, ctx.instance->evaluate(&ctx)))
            {
                LOG_ERROR("template evaluate failed", { {"model",params.common->modelGUID} , {"reason", error} });
                res.status = 400;
                return;
            }
        }

        //! Send back the response as JSON
        //! 
        //! Change as needed if response isn't JSON etc.
        json data = json
        {
            {"stop", true},
            {"model", params.common->modelGUID},
            {"n_threads", params.common->numThreads},
            //! Add your results as needed
        };

        res.set_content(data.dump(-1, ' ', false, json::error_handler_t::replace), "application/json");
    });

    return nvigi::kResultOk;
}

Result removeMicroService()
{
    for (auto& [guid, instance] : ctx.instances)
    {
        if (NVIGI_FAILED(error, ctx._interface->destroyInstance(instance)))
        {
            return error;
        }
    }
    return nvigi::kResultOk;
}

}
}
}