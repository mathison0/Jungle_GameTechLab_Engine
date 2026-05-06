#pragma once

#include "Render/Execute/Passes/Base/FullscreenPassBase.h"
#include "Render/Execute/Context/Viewport/ViewportRenderTargets.h"
#include "Render/Execute/Registry/ViewModePassRegistry.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"
#include "Render/Submission/Command/BuildDrawCommand.h"

/*
    Pass Summary
    - Role: helper base for fullscreen post-process passes.
    - Inputs: scene-color/depth/stencil copy resources from viewport targets.
    - Outputs: viewport RTV through fullscreen compositing.
    - Registers: helper methods bind PS t0, t10, t11, t13 as needed by derived passes.
*/
class FPostProcessPassBase : public FFullscreenPassBase
{
public:
    void PrepareTargets(FRenderPipelineContext& Context) override
    {
        BindViewportTarget(Context);
    }

protected:
    bool HasSceneColorCopyInput(const FRenderPipelineContext& Context) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        return Targets && Targets->SceneColorCopySRV && Targets->SceneColorCopyTexture;
    }

    void PrepareSceneColorCopyInput(FRenderPipelineContext& Context) const
    {
        if (!Context.Context || !HasSceneColorCopyInput(Context))
        {
            return;
        }

        CopyViewportColorToSceneColor(Context);
        BindSceneColorInput(Context, true);
    }

    void UnbindTargetsForCopy(FRenderPipelineContext& Context) const
    {
        Context.Context->OMSetRenderTargets(0, nullptr, nullptr);

        if (Context.StateCache)
        {
            Context.StateCache->RTV       = nullptr;
            Context.StateCache->DSV       = nullptr;
            Context.StateCache->bForceAll = true;
        }
    }

    void CopyViewportColorToSceneColor(FRenderPipelineContext& Context) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->ViewportRenderTexture || !Targets->SceneColorCopyTexture ||
            Targets->ViewportRenderTexture == Targets->SceneColorCopyTexture)
        {
            return;
        }

        UnbindTargetsForCopy(Context);
        Context.Context->CopyResource(Targets->SceneColorCopyTexture, Targets->ViewportRenderTexture);
    }

    void CopyViewportDepthToReadable(FRenderPipelineContext& Context) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->DepthTexture || !Targets->DepthCopyTexture || Targets->DepthTexture == Targets->DepthCopyTexture)
        {
            return;
        }

        UnbindTargetsForCopy(Context);
        Context.Context->CopyResource(Targets->DepthCopyTexture, Targets->DepthTexture);
    }

    void BindSceneColorInput(FRenderPipelineContext& Context, bool bBindSlot0 = false) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->SceneColorCopySRV)
        {
            return;
        }

        ID3D11ShaderResourceView* SceneColorSRV = Targets->SceneColorCopySRV;
        if (bBindSlot0)
        {
            Context.Context->PSSetShaderResources(0, 1, &SceneColorSRV);
        }
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);
    }

    void BindDepthInput(FRenderPipelineContext& Context) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->DepthCopySRV)
        {
            return;
        }

        ID3D11ShaderResourceView* DepthSRV = Targets->DepthCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
    }

    void BindStencilInput(FRenderPipelineContext& Context) const
    {
        const FViewportRenderTargets* Targets = Context.Targets;
        if (!Targets || !Targets->StencilCopySRV)
        {
            return;
        }

        ID3D11ShaderResourceView* StencilSRV = Targets->StencilCopySRV;
        Context.Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &StencilSRV);
    }

    void Cleanup(FRenderPipelineContext& Context) override
    {
        ID3D11ShaderResourceView* NullSRV = nullptr;
        Context.Context->PSSetShaderResources(0, 1, &NullSRV);
        Context.Context->PSSetShaderResources(1, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
        Context.Context->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &NullSRV);

        if (Context.StateCache)
        {
            Context.StateCache->DiffuseSRV     = nullptr;
            Context.StateCache->NormalSRV      = nullptr;
            Context.StateCache->SpecularSRV    = nullptr;
            Context.StateCache->PerShaderCB[0] = nullptr;
            Context.StateCache->PerShaderCB[1] = nullptr;
            Context.StateCache->bForceAll      = true;
        }
    }
};

class FPostProcessVariantPassBase : public FPostProcessPassBase
{
public:
    void PrepareInputs(FRenderPipelineContext& Context) override
    {
        PrepareSceneColorCopyInput(Context);
    }

    void BuildDrawCommands(FRenderPipelineContext& Context) override
    {
        if (!IsEnabled(Context) || !HasSceneColorCopyInput(Context) || !Context.DrawCommandList)
        {
            return;
        }

        TArray<FDrawCommand>& Commands           = Context.DrawCommandList->GetCommands();
        const size_t          CommandCountBefore = Commands.size();

        DrawCommandBuild::BuildFullscreenDrawCommand(
            ERenderPass::PostProcess,
            Context,
            *Context.DrawCommandList,
            GetPostProcessVariant());

        if (Commands.size() > CommandCountBefore)
        {
            (void)BindPostProcessConstantBuffer(Context, Commands.back());
        }
    }

    void BuildDrawCommands(FRenderPipelineContext& Context, const FPrimitiveProxy& Proxy) override
    {
        (void)Context;
        (void)Proxy;
    }

    void SubmitDrawCommands(FRenderPipelineContext& Context) override
    {
        if (!Context.DrawCommandList || !Context.Device || !Context.Context || !Context.StateCache)
        {
            return;
        }

        uint32 Start = 0;
        uint32 End   = 0;
        Context.DrawCommandList->GetPassRange(ERenderPass::PostProcess, Start, End);

        const uint8 TargetUserBits = static_cast<uint8>(ToPostProcessUserBits(GetPostProcessVariant()));
        for (uint32 i = Start; i < End; ++i)
        {
            const FDrawCommand& Command  = Context.DrawCommandList->GetCommands()[i];
            const uint8         UserBits = static_cast<uint8>((Command.SortKey >> 48) & 0xFFu);
            if (UserBits == TargetUserBits)
            {
                Context.DrawCommandList->SubmitRange(i, i + 1, *Context.Device, Context.Context, *Context.StateCache);
            }
        }
    }

protected:
    virtual EViewModePostProcessVariant GetPostProcessVariant() const = 0;

    virtual bool BindPostProcessConstantBuffer(FRenderPipelineContext& Context, FDrawCommand& Command)
    {
        (void)Context;
        (void)Command;
        return true;
    }
};
