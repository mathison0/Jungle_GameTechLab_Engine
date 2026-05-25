#pragma once
#include "RenderPass.h"
#include "Render/Resource/Buffer.h"
#include "Render/Resource/InstanceBuffer.h"

struct FRenderCommand;

class FParticleRenderPass : public FBaseRenderPass
{
public:
    bool Initialize() override;
    bool Release() override;

private:
    bool Begin(const FRenderPassContext* Context) override;
    bool DrawCommand(const FRenderPassContext* Context) override;
    bool End(const FRenderPassContext* Context) override;

    bool EnsureGPUResources(ID3D11Device* Device);

    // Cycle 10a: type별 helper 분기 (단일 Pass + procedural switch — 사용자 결정 3).
    // 본 cycle에서 Sprite만 실제 본문 보유, Mesh/Ribbon/Beam은 NOP. Cycle 11+ 각 emitter cycle에서 본문 채움.
    // 비대칭 인지: 데이터 생성은 polymorphism(instance virtual), 렌더 분기는 procedural switch(여기) — 의도된 비대칭.
    void RenderSpriteEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context);
    void RenderMeshEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context);
    void RenderRibbonEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context);
    void RenderBeamEmitter(const FRenderCommand& Cmd, const FRenderPassContext& Context);

    FVertexBuffer   QuadVertexBuffer;
    FIndexBuffer    QuadIndexBuffer;
    FInstanceBuffer InstanceBuffer;
    FConstantBuffer SpriteParticleCB;   // b8: SubUV grid

    // Cycle 11: Mesh emitter용 per-instance VB (Sprite의 InstanceBuffer와 별도).
    // EnsureGPUResources에서 sizeof(FMeshParticleInstanceData) stride로 Create.
    FInstanceBuffer MeshInstanceBuffer;

    bool bGPUResourcesReady = false;
};
