#pragma once

#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

class FFadePass : public FPostProcessVariantPassBase
{
protected:
    bool IsEnabled(const FRenderPipelineContext& Context) const override;
    EViewModePostProcessVariant GetPostProcessVariant() const override { return EViewModePostProcessVariant::Fade; }
    bool BindPostProcessConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command) override;
};
