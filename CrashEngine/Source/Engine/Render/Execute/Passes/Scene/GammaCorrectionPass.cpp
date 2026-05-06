#include "GammaCorrectionPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

bool FGammaCorrectionPass::IsEnabled(const FRenderPipelineContext& Context) const
{
    return Context.SceneView && Context.SceneView->PostProcessSettings.GammaCorrection.bEnabled;
}

bool FGammaCorrectionPass::BindPostProcessConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command)
{
    if (!Context.SceneView || !Context.Context)
    {
        return false;
    }

    const FGammaCorrectionSettings& Settings = Context.SceneView->PostProcessSettings.GammaCorrection;

    FGammaCorrectionCBData Constants = {};
    Constants.DisplayGamma           = Settings.DisplayGamma;

    FConstantBuffer* CB = FConstantBufferCache::Get().GetBuffer(ERenderCBKey::GammaCorrection, sizeof(FGammaCorrectionCBData));
    if (!CB)
    {
        return false;
    }

    CB->Update(Context.Context, &Constants, sizeof(Constants));
    Command.PerShaderCB[0] = CB;
    return true;
}
