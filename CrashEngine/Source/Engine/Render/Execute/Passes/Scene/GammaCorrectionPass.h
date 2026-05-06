#pragma once
#include "Render/Execute/Passes/Base/PostProcessPassBase.h"

/*
    Pass Summary
    - Role: apply display gamma correction as a fullscreen post-process.
    - Inputs: scene-color copy.
    - Outputs: viewport color.
    - Registers: PS t11 SceneColor, PS t0 SceneColorCopy when prebound.
    - State: uses the shared PostProcess bucket, but GammaCorrection variant overrides blend to Opaque.
*/
class FGammaCorrectionPass : public FPostProcessVariantPassBase
{
protected:
    bool IsEnabled(const FRenderPipelineContext& Context) const override;
    EViewModePostProcessVariant GetPostProcessVariant() const override { return EViewModePostProcessVariant::GammaCorrection; }
    bool BindPostProcessConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command) override;
};
