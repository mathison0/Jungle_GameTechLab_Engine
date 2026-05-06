#include "LetterboxPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

bool FLetterboxPass::IsEnabled(const FRenderPipelineContext& Context) const
{
    return Context.SceneView && Context.SceneView->PostProcessSettings.Letterbox.bEnabled;
}

bool FLetterboxPass::BindPostProcessConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command)
{
    if (!Context.SceneView || !Context.Context)
    {
        return false;
    }

    const FLetterboxSettings& Settings = Context.SceneView->PostProcessSettings.Letterbox;

    const float ViewportHeight = Context.SceneView->ViewportHeight;
    const float ViewportAspect =
        ViewportHeight > 0.0f ? Context.SceneView->ViewportWidth / ViewportHeight : 1.0f;

    FLetterboxCBData Constants = {};
    Constants.TargetAspectRatio  = Settings.TargetAspectRatio;
    Constants.ViewportAspectRatio = ViewportAspect;
    Constants.Opacity             = Settings.Opacity;
    Constants.Color               = Settings.Color;

    FConstantBuffer* CB = FConstantBufferCache::Get().GetBuffer(ERenderCBKey::Letterbox, sizeof(FLetterboxCBData));
    if (!CB)
    {
        return false;
    }

    CB->Update(Context.Context, &Constants, sizeof(Constants));
    Command.PerShaderCB[0] = CB;
    return true;
}
