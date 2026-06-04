#include "Render/Optimization/SkeletalInstanceBatcher.h"

#include "Profiling/Stats/Stats.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"

#include <unordered_map>

namespace
{
	struct FSkeletalInstanceBatchKey
	{
		ERenderPass Pass = ERenderPass::Opaque;
		FShader* Shader = nullptr;
		FDrawCommandRenderState RenderState;

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

		bool operator==(const FSkeletalInstanceBatchKey& Other) const
		{
			if (Pass != Other.Pass) return false;
			if (Shader != Other.Shader) return false;

			if (RenderState.DepthStencil != Other.RenderState.DepthStencil) return false;
			if (RenderState.Blend != Other.RenderState.Blend) return false;
			if (RenderState.Rasterizer != Other.RenderState.Rasterizer) return false;
			if (RenderState.Topology != Other.RenderState.Topology) return false;

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

	struct FSkeletalInstanceBatchKeyHash
	{
		size_t operator()(const FSkeletalInstanceBatchKey& Key) const
		{
			size_t Hash = 1469598103934665603ull;

			auto Mix = [&Hash](uintptr_t Value)
			{
				Hash ^= static_cast<size_t>(Value);
				Hash *= 1099511628211ull;
			};

			Mix((uintptr_t)Key.Pass);
			Mix((uintptr_t)Key.Shader);

			Mix((uintptr_t)Key.RenderState.DepthStencil);
			Mix((uintptr_t)Key.RenderState.Blend);
			Mix((uintptr_t)Key.RenderState.Rasterizer);
			Mix((uintptr_t)Key.RenderState.Topology);

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

	bool IsSkeletalInstanceCandidate(const FDrawCommand& Cmd)
	{
		if (!Cmd.bIsSkeletal) return false;
		if (!Cmd.bIsGpuSkinned) return false;
		if (Cmd.Buffer.IsInstanced()) return false;

		if (!Cmd.Buffer.VB || !Cmd.Buffer.IB) return false;
		if (Cmd.Buffer.IndexCount == 0) return false;

		if (Cmd.Pass != ERenderPass::PreDepth &&
			Cmd.Pass != ERenderPass::Opaque)
		{
			return false;
		}

		return true;
	}

	FSkeletalInstanceData MakeInstanceData(const FDrawCommand& Cmd)
	{
		const FMatrix& World = Cmd.PerObjectConstants.Model;

		FSkeletalInstanceData Data = {};
		Data.World0 = FVector4(World.M[0][0], World.M[0][1], World.M[0][2], World.M[0][3]);
		Data.World1 = FVector4(World.M[1][0], World.M[1][1], World.M[1][2], World.M[1][3]);
		Data.World2 = FVector4(World.M[2][0], World.M[2][1], World.M[2][2], World.M[2][3]);
		Data.World3 = FVector4(World.M[3][0], World.M[3][1], World.M[3][2], World.M[3][3]);
		Data.InstanceColor = Cmd.PerObjectConstants.Color;
		return Data;
	}

	FSkeletalInstanceBatchKey MakeSkeletalInstanceBatchKey(const FDrawCommand& Cmd)
	{
		FSkeletalInstanceBatchKey Key;

		Key.Pass = Cmd.Pass;
		Key.Shader = Cmd.Shader;
		Key.RenderState = Cmd.RenderState;

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
}

void FSkeletalInstanceBatcher::Create(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
	Device = InDevice;
	Context = InContext;
	InstanceBuffer.Create(Device, 256, sizeof(FSkeletalInstanceData));
}

void FSkeletalInstanceBatcher::Release()
{
	InstanceBuffer.Release();
	InstanceData.clear();
	Device = nullptr;
	Context = nullptr;
}

void FSkeletalInstanceBatcher::BuildInstancedCommands(const TArray<FDrawCommand>& InCommands, TArray<FDrawCommand>& OutCommands)
{
	OutCommands.clear();
	InstanceData.clear();

	uint32 CandidateCount = 0;
	uint32 RejectedCount = 0;
	uint32 SingleCount = 0;
	uint32 MergedDrawCount = 0;
	uint32 MergedInstanceCount = 0;

	std::unordered_map<FSkeletalInstanceBatchKey, TArray<const FDrawCommand*>, FSkeletalInstanceBatchKeyHash> Batches;

	for (const FDrawCommand& Cmd : InCommands)
	{
		if (!IsSkeletalInstanceCandidate(Cmd))
		{
			if (Cmd.bIsSkeletal)
			{
				++RejectedCount;
			}
			OutCommands.push_back(Cmd);
			continue;
		}

		++CandidateCount;
		FSkeletalInstanceBatchKey Key = MakeSkeletalInstanceBatchKey(Cmd);
		Batches[Key].push_back(&Cmd);
	}

	for (auto& Pair : Batches)
	{
		const TArray<const FDrawCommand*>& BatchCommands = Pair.second;
		if (BatchCommands.empty()) continue;

		if (BatchCommands.size() == 1)
		{
			++SingleCount;
			OutCommands.push_back(*BatchCommands[0]);
			continue;
		}

		const uint32 FirstInstance = static_cast<uint32>(InstanceData.size());

		for (const FDrawCommand* SourceCmd : BatchCommands)
		{
			InstanceData.push_back(MakeInstanceData(*SourceCmd));
		}

		FDrawCommand MergedCmd = *BatchCommands[0];
		MergedCmd.Buffer.InstanceVB = nullptr;
		MergedCmd.Buffer.InstanceStride = sizeof(FSkeletalInstanceData);
		MergedCmd.Buffer.FirstInstance = FirstInstance;

		MergedCmd.Buffer.InstanceCount = static_cast<uint32>(BatchCommands.size());
		++MergedDrawCount;
		MergedInstanceCount += MergedCmd.Buffer.InstanceCount;

		MergedCmd.Shader = FShaderManager::Get().GetOrCreateUberLitPermutation(
			EUberLitDefines::ELightingModel::Phong,
			EUberLitDefines::EVertexFactory::InstancedSkeletalMesh,
			EShaderErrorMode::Notification,
			false,
			false);

		MergedCmd.BuildSortKey();

		OutCommands.push_back(MergedCmd);
	}

	if (!InstanceData.empty())
	{
		InstanceBuffer.EnsureCapacity(Device, static_cast<uint32>(InstanceData.size()));
		InstanceBuffer.Update(Context, InstanceData.data(), static_cast<uint32>(InstanceData.size()));

		for (FDrawCommand& Cmd : OutCommands)
		{
			if (Cmd.bIsSkeletal && Cmd.bIsGpuSkinned && Cmd.Buffer.InstanceCount > 0)
			{
				Cmd.Buffer.InstanceVB = InstanceBuffer.GetBuffer();
			}
		}
	}

	FSkeletalRenderStats::RecordInstanceBatching(
		CandidateCount,
		RejectedCount,
		SingleCount,
		MergedDrawCount,
		MergedInstanceCount,
		static_cast<uint32>(OutCommands.size()));
}
