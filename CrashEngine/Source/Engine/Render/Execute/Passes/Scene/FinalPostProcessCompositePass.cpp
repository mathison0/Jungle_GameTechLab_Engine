#include "FinalPostProcessCompositePass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

namespace
{
bool ShouldRunFinalComposite(const FPostProcessSettings& Settings)
{
    return Settings.Vignetting.bEnabled ||
           Settings.GammaCorrection.bEnabled ||
           Settings.Fade.bEnabled ||
           Settings.Letterbox.bEnabled;
}
} // namespace

bool FFinalPostProcessCompositePass::IsEnabled(const FRenderPipelineContext& Context) const
{
    return Context.SceneView && ShouldRunFinalComposite(Context.SceneView->PostProcessSettings);
}

void FFinalPostProcessCompositePass::PrepareInputs(FRenderPipelineContext& Context)
{
    PrepareSceneColorCopyInput(Context);
}

void FFinalPostProcessCompositePass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    if (!IsEnabled(Context) || !HasSceneColorCopyInput(Context) || !Context.DrawCommandList)
    {
        return;
    }

    TArray<FDrawCommand>& Commands           = Context.DrawCommandList->GetCommands();
    const size_t          CommandCountBefore = Commands.size();

    DrawCommandBuild::BuildFullscreenDrawCommand(
        ERenderPass::FinalPostProcess,
        Context,
        *Context.DrawCommandList);

    if (Commands.size() > CommandCountBefore)
    {
        (void)BindCompositeConstantBuffer(Context, Commands.back());
    }
}

void FFinalPostProcessCompositePass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    SubmitPassRange(Context, ERenderPass::FinalPostProcess);
}

bool FFinalPostProcessCompositePass::BindCompositeConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command)
{
    if (!Context.SceneView || !Context.Context)
    {
        return false;
    }

    const FPostProcessSettings& Settings = Context.SceneView->PostProcessSettings;

    const float ViewportHeight = Context.SceneView->ViewportHeight;
    const float ViewportAspect =
        ViewportHeight > 0.0f ? Context.SceneView->ViewportWidth / ViewportHeight : 1.0f;

    FFinalPostProcessCompositeCBData Constants = {};
    Constants.bVignettingEnabled      = Settings.Vignetting.bEnabled ? 1.0f : 0.0f;
    Constants.VignetteIntensity       = Settings.Vignetting.Intensity;
    Constants.VignetteRadius          = Settings.Vignetting.Radius;
    Constants.VignetteSoftness        = Settings.Vignetting.Softness;
    Constants.VignetteColor           = Settings.Vignetting.Color;
    Constants.bGammaCorrectionEnabled = Settings.GammaCorrection.bEnabled ? 1.0f : 0.0f;
    Constants.DisplayGamma            = Settings.GammaCorrection.DisplayGamma;
    Constants.bFadeEnabled            = Settings.Fade.bEnabled ? 1.0f : 0.0f;
    Constants.FadeAlpha               = Settings.Fade.Alpha;
    Constants.FadeColor               = Settings.Fade.Color;
    Constants.bLetterboxEnabled       = Settings.Letterbox.bEnabled ? 1.0f : 0.0f;
    Constants.TargetAspectRatio       = Settings.Letterbox.TargetAspectRatio;
    Constants.ViewportAspectRatio     = ViewportAspect;
    Constants.LetterboxOpacity        = Settings.Letterbox.Opacity;
    Constants.LetterboxColor          = Settings.Letterbox.Color;

    FConstantBuffer* CB = FConstantBufferCache::Get().GetBuffer(
        ERenderCBKey::FinalPostProcessComposite,
        sizeof(FFinalPostProcessCompositeCBData));
    if (!CB)
    {
        return false;
    }

    CB->Update(Context.Context, &Constants, sizeof(Constants));
    Command.PerShaderCB[0] = CB;
    return true;
}
