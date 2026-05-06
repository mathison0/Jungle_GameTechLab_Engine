#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"
class FVignettingPass : public FPostProcessVariantPassBase
{
protected:
    bool IsEnabled(const FRenderPipelineContext& Context) const override;
    EViewModePostProcessVariant GetPostProcessVariant() const override { return EViewModePostProcessVariant::Vignetting; }
    bool BindPostProcessConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command) override;
};
 
