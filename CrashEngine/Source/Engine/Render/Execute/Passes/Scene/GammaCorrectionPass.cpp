#include "GammaCorrectionPass.h"
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

namespace
{
bool BindGammaCorrectionConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command)
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
} // namespace

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

    TArray<FDrawCommand>& Commands           = Context.DrawCommandList->GetCommands();
    const size_t          CommandCountBefore = Commands.size();

    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::GammaCorrection);
    if (Commands.size() > CommandCountBefore)
    {
        BindGammaCorrectionConstantBuffer(Context, Commands.back());
    }
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
