#pragma once

#include "Render/RenderPass/RenderPassBase.h"

class FRefractionPass final : public FRenderPassBase
{
public:
	FRefractionPass();

	bool BeginPass(const FPassContext& Ctx) override;
	void Execute(const FPassContext& Ctx) override;
	void EndPass(const FPassContext& Ctx) override;
};
