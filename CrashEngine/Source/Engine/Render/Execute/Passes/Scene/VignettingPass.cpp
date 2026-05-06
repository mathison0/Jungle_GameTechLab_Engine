#include "VignettingPass.h"
#include "Render/Execute/Context/RenderPipelineContext.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Submission/Command/DrawCommandList.h"
#include "Render/Submission/Command/BuildDrawCommand.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Resources/Buffers/ConstantBufferCache.h"
#include "Render/Resources/Bindings/RenderCBKeys.h"

void FVignettingPass::PrepareInputs(FRenderPipelineContext& Context)
{
    if (!Context.Targets || !Context.Targets->SceneColorCopySRV)
    {
        return;
    }

    CopyViewportColorToSceneColor(Context);
    BindSceneColorInput(Context, true);
}

void FVignettingPass::BuildDrawCommands(FRenderPipelineContext& Context)
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

    DrawCommandBuild::BuildFullscreenDrawCommand(ERenderPass::PostProcess, Context, *Context.DrawCommandList, EViewModePostProcessVariant::Vignetting);
    if (Commands.size() > CommandCountBefore)
    {
        BindVignettingConstantBuffer(Context, Commands.back());
    }
}

void FVignettingPass::SubmitDrawCommands(FRenderPipelineContext& Context)
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
        if (UserBits == static_cast<uint8>(ToPostProcessUserBits(EViewModePostProcessVariant::Vignetting)))
        {
            Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
        }
    }
}

bool FVignettingPass::BindVignettingConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command)
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