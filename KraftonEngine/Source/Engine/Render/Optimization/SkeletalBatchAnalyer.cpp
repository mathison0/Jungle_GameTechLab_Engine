#include "Render/Optimization/SkeletalBatchAnalyzer.h"

#include "Profiling/Stats/Stats.h"
#include "Render/Command/DrawCommand.h"

#include <unordered_set>

namespace
{
	struct FSkeletalBatchKey
	{
		ERenderPass Pass = ERenderPass::Opaque;
		FShader* Shader = nullptr;

		EDepthStencilState DepthStencil = EDepthStencilState::Default;
		EBlendState Blend = EBlendState::Opaque;
		ERasterizerState Rasterizer = ERasterizerState::SolidBackCull;
		D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		ID3D11Buffer* VB = nullptr;
		uint32 VBStride = 0;
		ID3D11Buffer* IB = nullptr;

		uint32 FirstIndex = 0;
		uint32 IndexCount = 0;
		int32 BaseVertex = 0;

		FConstantBuffer* PerShaderCB0 = nullptr;
		FConstantBuffer* PerShaderCB1 = nullptr;
		FConstantBuffer* BoneHeatMapCB = nullptr;
		FConstantBuffer* MaterialBloomCB = nullptr;

		ID3D11ShaderResourceView* SRVs[(int)EMaterialTextureSlot::Max] = {};

		bool operator==(const FSkeletalBatchKey& Other) const
		{
			if (Pass != Other.Pass) return false;
			if (Shader != Other.Shader) return false;

			if (DepthStencil != Other.DepthStencil) return false;
			if (Blend != Other.Blend) return false;
			if (Rasterizer != Other.Rasterizer) return false;
			if (Topology != Other.Topology) return false;

			if (VB != Other.VB || VBStride != Other.VBStride || IB != Other.IB) return false;
			if (FirstIndex != Other.FirstIndex || IndexCount != Other.IndexCount || BaseVertex != Other.BaseVertex) return false;

			if (PerShaderCB0 != Other.PerShaderCB0) return false;
			if (PerShaderCB1 != Other.PerShaderCB1) return false;
			if (BoneHeatMapCB != Other.BoneHeatMapCB) return false;
			if (MaterialBloomCB != Other.MaterialBloomCB) return false;

			for (int32 Index = 0; Index < (int32)EMaterialTextureSlot::Max; ++Index)
			{
				if (SRVs[Index] != Other.SRVs[Index])
				{
					return false;
				}
			}

			return true;
		}
	};

	struct FSkeletalBatchKeyHash
	{
		size_t operator()(const FSkeletalBatchKey& Key) const
		{
			size_t Hash = 1469598103934665603ull;

			auto Mix = [&Hash](uintptr_t Value)
			{
				Hash ^= static_cast<size_t>(Value);
				Hash *= 1099511628211ull;
			};

			Mix((uintptr_t)Key.Pass);
			Mix((uintptr_t)Key.Shader);
			Mix((uintptr_t)Key.DepthStencil);
			Mix((uintptr_t)Key.Blend);
			Mix((uintptr_t)Key.Rasterizer);
			Mix((uintptr_t)Key.Topology);

			Mix((uintptr_t)Key.VB);
			Mix((uintptr_t)Key.VBStride);
			Mix((uintptr_t)Key.IB);
			Mix((uintptr_t)Key.FirstIndex);
			Mix((uintptr_t)Key.IndexCount);
			Mix((uintptr_t)Key.BaseVertex);

			Mix((uintptr_t)Key.PerShaderCB0);
			Mix((uintptr_t)Key.PerShaderCB1);
			Mix((uintptr_t)Key.BoneHeatMapCB);
			Mix((uintptr_t)Key.MaterialBloomCB);

			for (int32 Index = 0; Index < (int32)EMaterialTextureSlot::Max; ++Index)
			{
				Mix((uintptr_t)Key.SRVs[Index]);
			}

			return Hash;
		}
	};

	FSkeletalBatchKey MakeSkeletalBatchKey(const FDrawCommand& Cmd)
	{
		FSkeletalBatchKey Key;

		Key.Pass = Cmd.Pass;
		Key.Shader = Cmd.Shader;

		Key.DepthStencil = Cmd.RenderState.DepthStencil;
		Key.Blend = Cmd.RenderState.Blend;
		Key.Rasterizer = Cmd.RenderState.Rasterizer;
		Key.Topology = Cmd.RenderState.Topology;

		Key.VB = Cmd.Buffer.VB;
		Key.VBStride = Cmd.Buffer.VBStride;
		Key.IB = Cmd.Buffer.IB;

		Key.FirstIndex = Cmd.Buffer.FirstIndex;
		Key.IndexCount = Cmd.Buffer.IndexCount;
		Key.BaseVertex = Cmd.Buffer.BaseVertex;

		Key.PerShaderCB0 = Cmd.Bindings.PerShaderCB[0];
		Key.PerShaderCB1 = Cmd.Bindings.PerShaderCB[1];
		Key.BoneHeatMapCB = Cmd.Bindings.BoneHeatMapCB;
		Key.MaterialBloomCB = Cmd.Bindings.MaterialBloomCB;

		for (int32 Index = 0; Index < (int32)EMaterialTextureSlot::Max; ++Index)
		{
			Key.SRVs[Index] = Cmd.Bindings.SRVs[Index];
		}

		return Key;
	}

	bool IsSkeletalBatchCandidate(const FDrawCommand& Cmd)
	{
		if (!Cmd.bIsSkeletal)
		{
			return false;
		}

		if (!Cmd.bIsGpuSkinned)
		{
			return false;
		}

		if (Cmd.Buffer.IsInstanced())
		{
			return false;
		}

		if (!Cmd.Buffer.VB || !Cmd.Buffer.IB)
		{
			return false;
		}

		if (Cmd.Buffer.IndexCount == 0)
		{
			return false;
		}

		if (Cmd.Pass != ERenderPass::PreDepth &&
			Cmd.Pass != ERenderPass::Opaque &&
			Cmd.Pass != ERenderPass::AlphaBlend)
		{
			return false;
		}

		return true;
	}
}

void FSkeletalBatchAnalyzer::Analyze(const TArray<FDrawCommand>& Commands)
{
	uint32 GpuSkinCommands = 0;
	uint32 BatchableCommands = 0;
	uint32 RejectedCommands = 0;

	std::unordered_set<FSkeletalBatchKey, FSkeletalBatchKeyHash> UniqueKeys;

	for (const FDrawCommand& Cmd : Commands)
	{
		if (!Cmd.bIsSkeletal) continue;

		if (Cmd.bIsGpuSkinned)
		{
			++GpuSkinCommands;
		}

		if (!IsSkeletalBatchCandidate(Cmd))
		{
			++RejectedCommands;
			continue;
		}

		++BatchableCommands;
		UniqueKeys.insert(MakeSkeletalBatchKey(Cmd));
	}

	FSkeletalRenderStats::RecordBatchAnalysis(GpuSkinCommands, BatchableCommands,
		RejectedCommands, static_cast<uint32>(UniqueKeys.size()));
}
