#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"
class FVignettingPass : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context) override;
    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }
    void SubmitDrawCommands(FRenderPipelineContext& Context) override;
    bool BindVignettingConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command);

protected:
    bool IsEnabled(const FRenderPipelineContext& Context) const override;
};
 
