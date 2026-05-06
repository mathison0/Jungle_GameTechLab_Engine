#include "LetterboxPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

bool FLetterboxPass::IsEnabled(const FRenderPipelineContext& Context) const
{
    return Context.SceneView && Context.SceneView->PostProcessSettings.Letterbox.bEnabled;
}

void FLetterboxPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.Targets || !Context.Targets->SceneColorCopySRV)
    {
        return;
    }

    CopyViewportColorToSceneColor(Context);
    BindSceneColorInput(Context, true);
}

void FLetterboxPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!IsEnabled(Context))
    {
        return;
    }

    if (!Targets || !Targets->SceneColorCopySRV || !Targets->SceneColorCopyTexture || !Context.DrawCommandList)
    {
        return;
    }

    TArray<FDrawCommand>& Commands           = Context.DrawCommandList->GetCommands();
    const size_t          CommandCountBefore = Commands.size();

    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::Letterbox);
    if (Commands.size() > CommandCountBefore)
    {
        BindLetterboxConstantBuffer(Context, Commands.back());
    }
}

void FLetterboxPass::SubmitDrawCommands(FRenderPipelineContext& Context)
{
    if (!Context.DrawCommandList)
    {
        return;
    }

    uint32 Start = 0;
    uint32 End   = 0;
    Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, Start, End);
    for (uint32 i = Start; i < End; ++i)
    {
        const auto& Command  = Context.DrawCommandList->GetCommands()[i];
        const uint8 UserBits = static_cast<uint8>((Command.SortKey >> 48) & 0xFFu);
        if (UserBits == static_cast<uint8>(ToPostProcessUserBits(EViewModePostProcessVariant::Letterbox)))
        {
            Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}

bool FLetterboxPass::BindLetterboxConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command)
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
