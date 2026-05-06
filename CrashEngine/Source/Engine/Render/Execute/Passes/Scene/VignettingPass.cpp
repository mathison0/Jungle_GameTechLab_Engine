#include "VignettingPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

bool FVignettingPass::IsEnabled(const FRenderPipelineContext& Context) const
{
    return Context.SceneView && Context.SceneView->PostProcessSettings.Vignetting.bEnabled;
}

bool FVignettingPass::BindPostProcessConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command)
{
    if (!Context.SceneView || !Context.Context)
    {
        return false;
    }

    const FVignettingSettings& Settings = Context.SceneView->PostProcessSettings.Vignetting;

    FVignettingCBData Constants = {};
    Constants.Intensity         = Settings.Intensity;
    Constants.Color             = Settings.Color;
    Constants.Radius            = Settings.Radius;
    Constants.bEnabled          = static_cast<float>(Settings.bEnabled);
    Constants.Softness          = Settings.Softness;

    FConstantBuffer* CB = FConstantBufferCache::Get().GetBuffer(ERenderCBKey::Vignetting, sizeof(FVignettingCBData));
    if (!CB)
    {
        return false;
    }

    CB->Update(Context.Context, &Constants, sizeof(Constants));
    Command.PerShaderCB[0] = CB;
    return true;
}
