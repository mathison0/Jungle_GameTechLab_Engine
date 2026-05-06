#include "FadePass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

bool FFadePass::IsEnabled(const FRenderPipelineContext& Context) const
{
    return Context.SceneView && Context.SceneView->PostProcessSettings.Fade.bEnabled;
}

bool FFadePass::BindPostProcessConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command)
{
    if (!Context.SceneView || !Context.Context)
    {
        return false;
    }

    const FFadeSettings& Settings = Context.SceneView->PostProcessSettings.Fade;

    FFadeCBData Constants = {};
    Constants.Color       = Settings.Color;
    Constants.Alpha       = Settings.Alpha;

    FConstantBuffer* CB = FConstantBufferCache::Get().GetBuffer(ERenderCBKey::Fade, sizeof(FFadeCBData));
    if (!CB)
    {
        return false;
    }

    CB->Update(Context.Context, &Constants, sizeof(Constants));
    Command.PerShaderCB[0] = CB;
    return true;
}
