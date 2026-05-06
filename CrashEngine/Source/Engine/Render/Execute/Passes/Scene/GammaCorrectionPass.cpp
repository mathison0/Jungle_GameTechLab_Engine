#include "GammaCorrectionPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"

bool FGammaCorrectionPass::IsEnabled(const FRenderPipelineContext& Context) const
{
    return Context.SceneView && Context.SceneView->PostProcessSettings.GammaCorrection.bEnabled;
}

void FGammaCorrectionPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.Targets || !Context.Targets->SceneColorCopySRV)
    {
        return;
    }

    CopyViewportColorToSceneColor(Context);
    BindSceneColorInput(Context, true);
}

void FGammaCorrectionPass::BuildDrawCommands(FRenderPipelineContext& Context)
{
    const FViewportRenderTargets* Targets = Context.Targets;
    if (!IsEnabled(Context))
    {
        return;
    }

    if (!Targets || !Targets->SceneColorCopySRV || !Targets->SceneColorCopyTexture)
    {
        return;
    }

    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::GammaCorrection);
}

void FGammaCorrectionPass::SubmitDrawCommands(FRenderPipelineContext& Context)
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
        const auto& Command = Context.DrawCommandList->GetCommands()[i];
        const uint8 UserBits = static_cast<uint8>((Command.SortKey >> 48) & 0xFFu);
        if (UserBits == static_cast<uint8>(ToPostProcessUserBits(EViewModePostProcessVariant::GammaCorrection)))
        {
            Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}
