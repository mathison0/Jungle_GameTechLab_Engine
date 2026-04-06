#include "Renderer/SceneRenderer.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <vector>

#include <Windows.h>

#include "Camera/Camera.h"
#include "Graphics/D3D11/D3D11Utils.h"
#include "Graphics/D3D11/D3D11RHI.h"
#include "Picking/PickingSystem.h"
#include "Scene/Scene.h"
#include "StaticMesh/StaticMesh.h"
#include "Types/Array.h"
#include "Visibility/VisibilitySystem.h"

namespace
{
	constexpr UINT HzbThreadGroupSizeX = 8;
	constexpr UINT HzbThreadGroupSizeY = 8;
	constexpr UINT OcclusionCullThreadGroupSizeX = 64;
	constexpr size_t OcclusionReadbackSlotCount = 3;
	constexpr float GpuDepthEpsilon = 1.e-3f;
	constexpr float SceneDepthClearValue = 1.0f;

	enum class ERenderPassType
	{
		DepthOnly,
		BasePass,
	};

#ifndef NDEBUG
	void ValidateHzbStandardZContract()
	{
		constexpr float ValidationNearClip = 0.1f;
		constexpr float ValidationFarClip = 1000.0f;
		constexpr float ValidationFovRadians = 90.0f * (3.14159265358979323846f / 180.0f);
		const FMatrix Projection = FMatrix::MakePerspectiveFovLH(
			ValidationFovRadians,
			1.0f,
			ValidationNearClip,
			ValidationFarClip);
		const float NearDepth = Projection.TransformPosition(FVector(0.0f, 0.0f, ValidationNearClip)).Z;
		const float FarDepth = Projection.TransformPosition(FVector(0.0f, 0.0f, ValidationFarClip)).Z;

		assert(std::fabs(SceneDepthClearValue - 1.0f) <= 1.0e-6f);
		assert(NearDepth >= -1.0e-4f && NearDepth < 0.01f);
		assert(FarDepth > 0.99f && FarDepth <= 1.0001f);
		assert(NearDepth < FarDepth);
	}
#endif

	bool CreateWhiteTexture(ID3D11Device* InDevice, TComPtr<ID3D11ShaderResourceView>& OutTextureView)
	{
		const uint32 WhitePixel = 0xFFFFFFFFu;

		const D3D11_TEXTURE2D_DESC TextureDesc =
		{
			1,
			1,
			1,
			1,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			{ 1, 0 },
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_SHADER_RESOURCE,
			0,
			0
		};

		const D3D11_SUBRESOURCE_DATA InitialData =
		{
			&WhitePixel,
			sizeof(uint32),
			0
		};

		TComPtr<ID3D11Texture2D> Texture;
		if (FAILED(InDevice->CreateTexture2D(&TextureDesc, &InitialData, Texture.GetAddressOf())))
		{
			return false;
		}

		return SUCCEEDED(InDevice->CreateShaderResourceView(Texture.Get(), nullptr, OutTextureView.GetAddressOf()));
	}

	std::filesystem::path SearchForRelativePathFrom(const std::filesystem::path& InStartDirectory, const std::filesystem::path& InRelativePath)
	{
		std::filesystem::path Cursor = InStartDirectory;
		while (!Cursor.empty())
		{
			const std::filesystem::path Candidate = Cursor / InRelativePath;
			if (std::filesystem::exists(Candidate))
			{
				return std::filesystem::absolute(Candidate);
			}

			if (!Cursor.has_parent_path())
			{
				break;
			}

			const std::filesystem::path Parent = Cursor.parent_path();
			if (Parent == Cursor)
			{
				break;
			}

			Cursor = Parent;
		}

		return {};
	}

	std::filesystem::path FindHzbShaderPath(const std::filesystem::path& InFileName)
	{
		static const std::array<std::filesystem::path, 2> RelativeRoots =
		{
			std::filesystem::path(L"PerformanceEngine/Shader/HZB"),
			std::filesystem::path(L"Shader/HZB"),
		};

		for (const std::filesystem::path& RelativeRoot : RelativeRoots)
		{
			if (const std::filesystem::path Candidate = SearchForRelativePathFrom(std::filesystem::current_path(), RelativeRoot / InFileName); !Candidate.empty())
			{
				return Candidate;
			}
		}

		std::array<wchar_t, MAX_PATH> ModulePath = {};
		const DWORD CharacterCount = GetModuleFileNameW(nullptr, ModulePath.data(), static_cast<DWORD>(ModulePath.size()));
		if (CharacterCount == 0)
		{
			return {};
		}

		const std::filesystem::path ModuleDirectory = std::filesystem::path(ModulePath.data()).parent_path();
		for (const std::filesystem::path& RelativeRoot : RelativeRoots)
		{
			if (const std::filesystem::path Candidate = SearchForRelativePathFrom(ModuleDirectory, RelativeRoot / InFileName); !Candidate.empty())
			{
				return Candidate;
			}
		}

		return {};
	}

	UINT DivideAndRoundUp(UINT InValue, UINT InDivisor)
	{
		return (InValue + InDivisor - 1u) / InDivisor;
	}
}

struct alignas(16) FFrameConstants
{
	FMatrix ViewProjection = FMatrix::Identity;
};

struct alignas(16) FObjectConstants
{
	FMatrix World = FMatrix::Identity;
	float Tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct alignas(16) FHzbBuildConstants
{
	uint32 SourceMip = 0;
	uint32 OutputWidth = 0;
	uint32 OutputHeight = 0;
	uint32 Padding = 0;
};

struct alignas(16) FOcclusionCullConstants
{
	FMatrix View = FMatrix::Identity;
	FMatrix ViewProjection = FMatrix::Identity;
	uint32 CandidateCount = 0;
	uint32 MipCount = 0;
	uint32 DepthWidth = 0;
	uint32 DepthHeight = 0;
	float NearClip = 0.1f;
	float DepthEpsilon = GpuDepthEpsilon;
	float Padding[2] = {};
};

struct alignas(16) FGpuOcclusionCandidate
{
	FVector BoundsMin = FVector::ZeroVector;
	float Padding0 = 0.0f;
	FVector BoundsMax = FVector::ZeroVector;
	float Padding1 = 0.0f;
};

constexpr UINT ObjectConstantsBlockSize = 256;
constexpr UINT ObjectConstantsPerRange = ObjectConstantsBlockSize / 16;

struct alignas(16) FObjectConstantsBlock
{
	FObjectConstants Constants = {};
	std::array<uint8, ObjectConstantsBlockSize - sizeof(FObjectConstants)> Padding = {};
};

static_assert(sizeof(FObjectConstants) <= ObjectConstantsBlockSize, "FObjectConstants must fit within a single constant buffer range.");
static_assert(sizeof(FObjectConstantsBlock) == ObjectConstantsBlockSize, "FObjectConstantsBlock must match the range binding size.");

struct FDrawItem
{
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	ID3D11ShaderResourceView* TextureView = nullptr;
	UINT IndexCount = 0;
	UINT IndexStart = 0;
	UINT ObjectIndex = 0;
};

struct FSceneRenderer::FResources
{
	struct FOcclusionReadbackState
	{
		TComPtr<ID3D11Buffer> VisibilityFlagBuffer;
		TComPtr<ID3D11UnorderedAccessView> VisibilityFlagBufferUAV;
		TComPtr<ID3D11Buffer> VisibilityFlagStagingBuffer;
		TComPtr<ID3D11Query> CompletionQuery;
		TComPtr<ID3D11Query> GpuTimingDisjointQuery;
		TComPtr<ID3D11Query> OcclusionCullStartQuery;
		TComPtr<ID3D11Query> OcclusionCullEndQuery;
		UINT VisibilityFlagBufferCapacity = 0;
		FVisibilityFrameInput FrameInput;
		std::chrono::steady_clock::time_point SubmissionWallClockTime = {};
		bool bPendingReadback = false;
	};

	TComPtr<ID3D11VertexShader> VertexShader;
	TComPtr<ID3D11PixelShader> PixelShader;
	TComPtr<ID3D11InputLayout> InputLayout;
	TComPtr<ID3D11Buffer> FrameConstantBuffer;
	TComPtr<ID3D11Buffer> ObjectConstantBuffer;
	TComPtr<ID3D11SamplerState> LinearSampler;
	TComPtr<ID3D11RasterizerState> RasterizerState;
	TComPtr<ID3D11DepthStencilState> DepthStencilState;
	TComPtr<ID3D11DepthStencilState> DepthOnlyDepthStencilState;
	TComPtr<ID3D11BlendState> BlendState;
	TComPtr<ID3D11ShaderResourceView> WhiteTextureView;
	std::vector<FObjectConstantsBlock> ObjectConstantBlocks;
	std::vector<FDrawItem> RenderItems;
	UINT ObjectBufferCapacity = 0;

	TComPtr<ID3D11ComputeShader> DepthToHzbMip0ComputeShader;
	TComPtr<ID3D11ComputeShader> ReduceHzbMipComputeShader;
	TComPtr<ID3D11ComputeShader> OcclusionCullComputeShader;
	TComPtr<ID3D11Buffer> HzbBuildConstantBuffer;
	TComPtr<ID3D11Buffer> OcclusionCullConstantBuffer;

	TComPtr<ID3D11Texture2D> HzbTexture;
	TComPtr<ID3D11ShaderResourceView> HzbTextureSRV;
	std::vector<TComPtr<ID3D11ShaderResourceView>> HzbMipSRVs;
	std::vector<TComPtr<ID3D11UnorderedAccessView>> HzbMipUAVs;
	UINT HzbWidth = 0;
	UINT HzbHeight = 0;
	UINT HzbMipCount = 0;

	TComPtr<ID3D11Buffer> CandidateBuffer;
	TComPtr<ID3D11ShaderResourceView> CandidateBufferSRV;
	UINT CandidateBufferCapacity = 0;

	std::array<FOcclusionReadbackState, OcclusionReadbackSlotCount> OcclusionReadbackSlots = {};
	size_t OldestPendingReadbackSlotIndex = 0;
	size_t NextSubmissionReadbackSlotIndex = 0;
	size_t PendingReadbackCount = 0;
	uint64 LastResolvedOcclusionFrameNumber = 0;
	TComPtr<ID3D11Query> HzbBuildCompletionQuery;
	TComPtr<ID3D11Query> HzbBuildTimingDisjointQuery;
	TComPtr<ID3D11Query> HzbBuildStartQuery;
	TComPtr<ID3D11Query> HzbBuildEndQuery;
	bool bHzbValid = false;
	bool bPendingHzbBuildTiming = false;
	float LastHzbBuildGpuTimeMs = 0.0f;
};

namespace
{
	template <typename TResources>
	bool EnsureObjectConstantBufferCapacity(ID3D11Device* InDevice, TResources& InOutResources, UINT InRequiredObjectCount)
	{
		if (InRequiredObjectCount <= InOutResources.ObjectBufferCapacity)
		{
			return true;
		}

		UINT NewCapacity = std::max(InRequiredObjectCount, 1u);
		if (InOutResources.ObjectBufferCapacity > 0)
		{
			NewCapacity = InOutResources.ObjectBufferCapacity;
			while (NewCapacity < InRequiredObjectCount)
			{
				NewCapacity *= 2;
			}
		}

		TComPtr<ID3D11Buffer> NewObjectBuffer;
		if (!D3D11Utils::CreateDynamicConstantBuffer(InDevice, NewCapacity * ObjectConstantsBlockSize, NewObjectBuffer))
		{
			return false;
		}

		InOutResources.ObjectConstantBuffer = std::move(NewObjectBuffer);
		InOutResources.ObjectBufferCapacity = NewCapacity;
		return true;
	}

	template <typename TResources>
	void BuildRenderQueue(
		const FScene& InScene,
		const FVisibilityResults& InVisibilityResults,
		const FPickState& InPickState,
		ID3D11ShaderResourceView* InDefaultTextureView,
		ERenderPassType InRenderPassType,
		TResources& InOutResources)
	{
		InOutResources.ObjectConstantBlocks.clear();
		InOutResources.RenderItems.clear();

		InOutResources.ObjectConstantBlocks.reserve(InVisibilityResults.VisiblePrimitiveIndices.size());
		InOutResources.RenderItems.reserve(InVisibilityResults.VisiblePrimitiveIndices.size());

		const TArray<FScenePrimitiveRuntimeData>& PrimitiveRuntimeData = InScene.GetPrimitiveRuntimeData();
		for (size_t VisibleIndex = 0; VisibleIndex < InVisibilityResults.VisiblePrimitiveIndices.size(); ++VisibleIndex)
		{
			const uint32 PrimitiveIndex = InVisibilityResults.VisiblePrimitiveIndices[VisibleIndex];
			if (PrimitiveIndex >= PrimitiveRuntimeData.size())
			{
				continue;
			}

			const FScenePrimitiveRuntimeData& PrimitiveData = PrimitiveRuntimeData[PrimitiveIndex];
			FStaticMesh* StaticMesh = PrimitiveData.StaticMesh;
			if (StaticMesh == nullptr || !StaticMesh->IsValid())
			{
				continue;
			}

			const uint32 LODCount = StaticMesh->GetLODCount();
			if (LODCount == 0)
			{
				continue;
			}

			const uint32 CachedLODIndex =
				VisibleIndex < InVisibilityResults.VisibleLODIndices.size()
				? InVisibilityResults.VisibleLODIndices[VisibleIndex]
				: 0;
			const uint32 LODIndex = std::min(CachedLODIndex, LODCount - 1);

			FObjectConstantsBlock ObjectBlock = {};
			ObjectBlock.Constants.World = PrimitiveData.WorldMatrix;
			if (PrimitiveData.PrimitiveId == InPickState.SelectedPrimitiveId)
			{
				ObjectBlock.Constants.Tint[0] = 0.1f;
				ObjectBlock.Constants.Tint[1] = 0.1f;
				ObjectBlock.Constants.Tint[2] = 0.1f;
				ObjectBlock.Constants.Tint[3] = 1.0f;
			}

			const UINT ObjectIndex = static_cast<UINT>(InOutResources.ObjectConstantBlocks.size());
			InOutResources.ObjectConstantBlocks.push_back(ObjectBlock);

			ID3D11Buffer* VertexBuffer = StaticMesh->GetVertexBuffer(LODIndex);
			ID3D11Buffer* IndexBuffer = StaticMesh->GetIndexBuffer(LODIndex);
			if (VertexBuffer == nullptr || IndexBuffer == nullptr)
			{
				continue;
			}

			for (const FStaticMesh::FSection& Section : StaticMesh->GetSections(LODIndex))
			{
				FDrawItem RenderItem = {};
				RenderItem.VertexBuffer = VertexBuffer;
				RenderItem.IndexBuffer = IndexBuffer;
				if (InRenderPassType == ERenderPassType::BasePass)
				{
					RenderItem.TextureView = StaticMesh->GetMaterialTexture(Section.MaterialIndex);
					if (RenderItem.TextureView == nullptr)
					{
						RenderItem.TextureView = InDefaultTextureView;
					}
				}

				RenderItem.IndexCount = Section.IndexCount;
				RenderItem.IndexStart = Section.IndexStart;
				RenderItem.ObjectIndex = ObjectIndex;
				InOutResources.RenderItems.push_back(RenderItem);
			}
		}
	}

	bool UploadObjectConstantBlocks(ID3D11DeviceContext* InDeviceContext, ID3D11Buffer* InObjectConstantBuffer, const std::vector<FObjectConstantsBlock>& InObjectConstantBlocks)
	{
		if (InDeviceContext == nullptr || InObjectConstantBuffer == nullptr || InObjectConstantBlocks.empty())
		{
			return true;
		}

		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		if (FAILED(InDeviceContext->Map(InObjectConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
		{
			return false;
		}

		std::memcpy(
			MappedResource.pData,
			InObjectConstantBlocks.data(),
			InObjectConstantBlocks.size() * sizeof(FObjectConstantsBlock));
		InDeviceContext->Unmap(InObjectConstantBuffer, 0);
		return true;
	}

	template <typename T>
	int ComparePointers(T* InLeft, T* InRight)
	{
		const std::less<T*> Less = {};
		if (Less(InLeft, InRight))
		{
			return -1;
		}

		if (Less(InRight, InLeft))
		{
			return 1;
		}

		return 0;
	}

	void SortRenderItems(std::vector<FDrawItem>& InOutRenderItems, ERenderPassType InRenderPassType)
	{
		const bool bSortByTexture = InRenderPassType == ERenderPassType::BasePass;
		std::ranges::stable_sort(InOutRenderItems, [bSortByTexture](const FDrawItem& InLeft, const FDrawItem& InRight)
		{
			if (const int VertexBufferOrder = ComparePointers(InLeft.VertexBuffer, InRight.VertexBuffer); VertexBufferOrder != 0)
			{
				return VertexBufferOrder < 0;
			}

			if (const int IndexBufferOrder = ComparePointers(InLeft.IndexBuffer, InRight.IndexBuffer); IndexBufferOrder != 0)
			{
				return IndexBufferOrder < 0;
			}

			if (bSortByTexture)
			{
				if (const int TextureOrder = ComparePointers(InLeft.TextureView, InRight.TextureView); TextureOrder != 0)
				{
					return TextureOrder < 0;
				}
			}

			if (InLeft.ObjectIndex != InRight.ObjectIndex)
			{
				return InLeft.ObjectIndex < InRight.ObjectIndex;
			}

			if (InLeft.IndexStart != InRight.IndexStart)
			{
				return InLeft.IndexStart < InRight.IndexStart;
			}

			return InLeft.IndexCount < InRight.IndexCount;
		});
	}

	void BindObjectConstantRange(ID3D11DeviceContext1* InDeviceContext, ID3D11Buffer* InObjectConstantBuffer, UINT InObjectIndex)
	{
		if (InDeviceContext == nullptr || InObjectConstantBuffer == nullptr)
		{
			return;
		}

		const UINT FirstConstant = InObjectIndex * ObjectConstantsPerRange;
		const UINT NumConstants = ObjectConstantsPerRange;
		ID3D11Buffer* ObjectBuffer = InObjectConstantBuffer;
		InDeviceContext->VSSetConstantBuffers1(1, 1, &ObjectBuffer, &FirstConstant, &NumConstants);
		InDeviceContext->PSSetConstantBuffers1(1, 1, &ObjectBuffer, &FirstConstant, &NumConstants);
	}

	template <typename TResources>
	bool PrepareRenderPassResources(
		const FD3D11RHI& InRHI,
		const FScene& InScene,
		const FVisibilityResults& InVisibilityResults,
		const FPickState& InPickState,
		ERenderPassType InRenderPassType,
		TResources& InOutResources)
	{
		BuildRenderQueue(
			InScene,
			InVisibilityResults,
			InPickState,
			InOutResources.WhiteTextureView.Get(),
			InRenderPassType,
			InOutResources);
		SortRenderItems(InOutResources.RenderItems, InRenderPassType);

		if (!EnsureObjectConstantBufferCapacity(
			InRHI.GetDevice(),
			InOutResources,
			static_cast<UINT>(std::max<size_t>(InOutResources.ObjectConstantBlocks.size(), 1))))
		{
			return false;
		}

		return UploadObjectConstantBlocks(
			InRHI.GetDeviceContext1(),
			InOutResources.ObjectConstantBuffer.Get(),
			InOutResources.ObjectConstantBlocks);
	}

	template <typename TResources>
	void DrawPreparedRenderItems(ID3D11DeviceContext1* InDeviceContext, TResources& InOutResources, bool bBindTextures)
	{
		if (InDeviceContext == nullptr || InOutResources.RenderItems.empty())
		{
			return;
		}

		ID3D11Buffer* CurrentVertexBuffer = nullptr;
		ID3D11Buffer* CurrentIndexBuffer = nullptr;
		ID3D11ShaderResourceView* CurrentTextureView = nullptr;
		UINT CurrentObjectIndex = UINT_MAX;
		const UINT Stride = sizeof(FStaticMeshVertex);
		const UINT Offset = 0;

		for (const FDrawItem& RenderItem : InOutResources.RenderItems)
		{
			ID3D11Buffer* VertexBuffer = RenderItem.VertexBuffer;
			if (VertexBuffer != CurrentVertexBuffer)
			{
				InDeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
				CurrentVertexBuffer = VertexBuffer;
			}

			ID3D11Buffer* IndexBuffer = RenderItem.IndexBuffer;
			if (IndexBuffer != CurrentIndexBuffer)
			{
				InDeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
				CurrentIndexBuffer = IndexBuffer;
			}

			if (RenderItem.ObjectIndex != CurrentObjectIndex)
			{
				BindObjectConstantRange(InDeviceContext, InOutResources.ObjectConstantBuffer.Get(), RenderItem.ObjectIndex);
				CurrentObjectIndex = RenderItem.ObjectIndex;
			}

			if (bBindTextures)
			{
				ID3D11ShaderResourceView* TextureView = RenderItem.TextureView;
				if (TextureView != CurrentTextureView)
				{
					InDeviceContext->PSSetShaderResources(0, 1, &TextureView);
					CurrentTextureView = TextureView;
				}
			}

			InDeviceContext->DrawIndexed(RenderItem.IndexCount, RenderItem.IndexStart, 0);
		}
	}

	bool EnsureHzbResources(ID3D11Device* InDevice, FSceneRenderer::FResources& InOutResources, UINT InWidth, UINT InHeight)
	{
		if (InDevice == nullptr || InWidth == 0 || InHeight == 0)
		{
			return false;
		}

		if (InOutResources.HzbTexture
			&& InOutResources.HzbWidth == InWidth
			&& InOutResources.HzbHeight == InHeight)
		{
			return true;
		}

		InOutResources.HzbMipSRVs.clear();
		InOutResources.HzbMipUAVs.clear();
		InOutResources.HzbTextureSRV.Reset();
		InOutResources.HzbTexture.Reset();
		InOutResources.HzbWidth = InWidth;
		InOutResources.HzbHeight = InHeight;
		InOutResources.HzbMipCount = 0;

		UINT CurrentWidth = InWidth;
		UINT CurrentHeight = InHeight;
		while (CurrentWidth > 1 || CurrentHeight > 1)
		{
			++InOutResources.HzbMipCount;
			CurrentWidth = std::max(CurrentWidth / 2u, 1u);
			CurrentHeight = std::max(CurrentHeight / 2u, 1u);
		}
		++InOutResources.HzbMipCount;

		D3D11_TEXTURE2D_DESC TextureDesc = {};
		TextureDesc.Width = InWidth;
		TextureDesc.Height = InHeight;
		TextureDesc.MipLevels = InOutResources.HzbMipCount;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = DXGI_FORMAT_R32_FLOAT;
		TextureDesc.SampleDesc.Count = 1;
		TextureDesc.Usage = D3D11_USAGE_DEFAULT;
		TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		if (FAILED(InDevice->CreateTexture2D(&TextureDesc, nullptr, InOutResources.HzbTexture.GetAddressOf())))
		{
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = TextureDesc.Format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = TextureDesc.MipLevels;
		if (FAILED(InDevice->CreateShaderResourceView(InOutResources.HzbTexture.Get(), &SRVDesc, InOutResources.HzbTextureSRV.GetAddressOf())))
		{
			return false;
		}

		InOutResources.HzbMipSRVs.resize(InOutResources.HzbMipCount);
		InOutResources.HzbMipUAVs.resize(InOutResources.HzbMipCount);
		for (UINT MipIndex = 0; MipIndex < InOutResources.HzbMipCount; ++MipIndex)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC MipSRVDesc = {};
			MipSRVDesc.Format = TextureDesc.Format;
			MipSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			MipSRVDesc.Texture2D.MostDetailedMip = MipIndex;
			MipSRVDesc.Texture2D.MipLevels = 1;
			if (FAILED(InDevice->CreateShaderResourceView(
				InOutResources.HzbTexture.Get(),
				&MipSRVDesc,
				InOutResources.HzbMipSRVs[MipIndex].GetAddressOf())))
			{
				return false;
			}

			D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
			UAVDesc.Format = TextureDesc.Format;
			UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			UAVDesc.Texture2D.MipSlice = MipIndex;
			if (FAILED(InDevice->CreateUnorderedAccessView(
				InOutResources.HzbTexture.Get(),
				&UAVDesc,
				InOutResources.HzbMipUAVs[MipIndex].GetAddressOf())))
			{
				return false;
			}
		}

		return true;
	}

	bool EnsureCandidateBufferCapacity(ID3D11Device* InDevice, FSceneRenderer::FResources& InOutResources, UINT InRequiredCount)
	{
		const UINT RequiredCount = std::max(InRequiredCount, 1u);
		if (RequiredCount <= InOutResources.CandidateBufferCapacity)
		{
			return true;
		}

		UINT NewCapacity = std::max(RequiredCount, 1u);
		if (InOutResources.CandidateBufferCapacity > 0)
		{
			NewCapacity = InOutResources.CandidateBufferCapacity;
			while (NewCapacity < RequiredCount)
			{
				NewCapacity *= 2u;
			}
		}

		TComPtr<ID3D11Buffer> Buffer;
		TComPtr<ID3D11ShaderResourceView> SRV;
		if (!D3D11Utils::CreateStructuredBuffer(
			InDevice,
			sizeof(FGpuOcclusionCandidate),
			NewCapacity,
			D3D11_BIND_SHADER_RESOURCE,
			D3D11_USAGE_DYNAMIC,
			D3D11_CPU_ACCESS_WRITE,
			nullptr,
			Buffer)
			|| !D3D11Utils::CreateStructuredBufferSRV(InDevice, Buffer.Get(), NewCapacity, SRV))
		{
			return false;
		}

		InOutResources.CandidateBuffer = std::move(Buffer);
		InOutResources.CandidateBufferSRV = std::move(SRV);
		InOutResources.CandidateBufferCapacity = NewCapacity;
		return true;
	}

	bool EnsureVisibilityFlagBufferCapacity(
		ID3D11Device* InDevice,
		FSceneRenderer::FResources::FOcclusionReadbackState& InOutReadbackState,
		UINT InRequiredCount)
	{
		const UINT RequiredCount = std::max(InRequiredCount, 1u);
		if (RequiredCount <= InOutReadbackState.VisibilityFlagBufferCapacity)
		{
			return true;
		}

		UINT NewCapacity = std::max(RequiredCount, 1u);
		if (InOutReadbackState.VisibilityFlagBufferCapacity > 0)
		{
			NewCapacity = InOutReadbackState.VisibilityFlagBufferCapacity;
			while (NewCapacity < RequiredCount)
			{
				NewCapacity *= 2u;
			}
		}

		TComPtr<ID3D11Buffer> Buffer;
		TComPtr<ID3D11UnorderedAccessView> UAV;
		TComPtr<ID3D11Buffer> StagingBuffer;
		if (!D3D11Utils::CreateStructuredBuffer(
			InDevice,
			sizeof(uint32),
			NewCapacity,
			D3D11_BIND_UNORDERED_ACCESS,
			D3D11_USAGE_DEFAULT,
			0,
			nullptr,
			Buffer)
			|| !D3D11Utils::CreateStructuredBufferUAV(InDevice, Buffer.Get(), NewCapacity, UAV)
			|| !D3D11Utils::CreateStagingBuffer(InDevice, NewCapacity * sizeof(uint32), StagingBuffer))
		{
			return false;
		}

		InOutReadbackState.VisibilityFlagBuffer = std::move(Buffer);
		InOutReadbackState.VisibilityFlagBufferUAV = std::move(UAV);
		InOutReadbackState.VisibilityFlagStagingBuffer = std::move(StagingBuffer);
		InOutReadbackState.VisibilityFlagBufferCapacity = NewCapacity;
		return true;
	}

	bool CreateQuery(ID3D11Device* InDevice, D3D11_QUERY InType, TComPtr<ID3D11Query>& OutQuery)
	{
		if (InDevice == nullptr)
		{
			return false;
		}

		if (OutQuery)
		{
			return true;
		}

		const D3D11_QUERY_DESC QueryDesc =
		{
			InType,
			0
		};

		return SUCCEEDED(InDevice->CreateQuery(&QueryDesc, OutQuery.GetAddressOf()));
	}

	bool EnsureOcclusionQueries(
		ID3D11Device* InDevice,
		FSceneRenderer::FResources::FOcclusionReadbackState& InOutReadbackState)
	{
		return CreateQuery(InDevice, D3D11_QUERY_EVENT, InOutReadbackState.CompletionQuery)
			&& CreateQuery(InDevice, D3D11_QUERY_TIMESTAMP_DISJOINT, InOutReadbackState.GpuTimingDisjointQuery)
			&& CreateQuery(InDevice, D3D11_QUERY_TIMESTAMP, InOutReadbackState.OcclusionCullStartQuery)
			&& CreateQuery(InDevice, D3D11_QUERY_TIMESTAMP, InOutReadbackState.OcclusionCullEndQuery);
	}

	bool EnsureHzbTimingQueries(ID3D11Device* InDevice, FSceneRenderer::FResources& InOutResources)
	{
		return CreateQuery(InDevice, D3D11_QUERY_EVENT, InOutResources.HzbBuildCompletionQuery)
			&& CreateQuery(InDevice, D3D11_QUERY_TIMESTAMP_DISJOINT, InOutResources.HzbBuildTimingDisjointQuery)
			&& CreateQuery(InDevice, D3D11_QUERY_TIMESTAMP, InOutResources.HzbBuildStartQuery)
			&& CreateQuery(InDevice, D3D11_QUERY_TIMESTAMP, InOutResources.HzbBuildEndQuery);
	}

	bool TryResolveTimestampRangeMs(
		ID3D11DeviceContext* InDeviceContext,
		ID3D11Query* InStartQuery,
		ID3D11Query* InEndQuery,
		uint64 InTimestampFrequency,
		float& OutDurationMs)
	{
		OutDurationMs = 0.0f;

		if (InDeviceContext == nullptr || InStartQuery == nullptr || InEndQuery == nullptr || InTimestampFrequency == 0)
		{
			return false;
		}

		uint64 StartTimestamp = 0;
		uint64 EndTimestamp = 0;
		if (InDeviceContext->GetData(InStartQuery, &StartTimestamp, sizeof(StartTimestamp), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK
			|| InDeviceContext->GetData(InEndQuery, &EndTimestamp, sizeof(EndTimestamp), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK
			|| EndTimestamp < StartTimestamp)
		{
			return false;
		}

		OutDurationMs = static_cast<float>(1000.0 * static_cast<double>(EndTimestamp - StartTimestamp) / static_cast<double>(InTimestampFrequency));
		return true;
	}

	void ResetOcclusionReadbackState(FSceneRenderer::FResources::FOcclusionReadbackState& InOutReadbackState)
	{
		InOutReadbackState.FrameInput = FVisibilityFrameInput();
		InOutReadbackState.SubmissionWallClockTime = {};
		InOutReadbackState.bPendingReadback = false;
	}

	void RefreshOcclusionReadbackRingState(FSceneRenderer::FResources& InOutResources)
	{
		InOutResources.PendingReadbackCount = 0;
		InOutResources.OldestPendingReadbackSlotIndex = 0;

		for (size_t SlotIndex = 0; SlotIndex < OcclusionReadbackSlotCount; ++SlotIndex)
		{
			const FSceneRenderer::FResources::FOcclusionReadbackState& ReadbackState = InOutResources.OcclusionReadbackSlots[SlotIndex];
			if (!ReadbackState.bPendingReadback)
			{
				continue;
			}

			if (InOutResources.PendingReadbackCount == 0
				|| ReadbackState.FrameInput.FrameNumber < InOutResources.OcclusionReadbackSlots[InOutResources.OldestPendingReadbackSlotIndex].FrameInput.FrameNumber)
			{
				InOutResources.OldestPendingReadbackSlotIndex = SlotIndex;
			}

			++InOutResources.PendingReadbackCount;
		}
	}

	size_t FindAvailableOcclusionReadbackSlotIndex(const FSceneRenderer::FResources& InResources)
	{
		for (size_t Offset = 0; Offset < OcclusionReadbackSlotCount; ++Offset)
		{
			const size_t SlotIndex = (InResources.NextSubmissionReadbackSlotIndex + Offset) % OcclusionReadbackSlotCount;
			if (!InResources.OcclusionReadbackSlots[SlotIndex].bPendingReadback)
			{
				return SlotIndex;
			}
		}

		return OcclusionReadbackSlotCount;
	}

	bool IsOcclusionReadbackReady(
		ID3D11DeviceContext* InDeviceContext,
		const FSceneRenderer::FResources::FOcclusionReadbackState& InReadbackState,
		bool& OutReady)
	{
		OutReady = false;

		if (InDeviceContext == nullptr)
		{
			return false;
		}

		if (!InReadbackState.bPendingReadback || !InReadbackState.CompletionQuery)
		{
			return true;
		}

		BOOL QueryResult = FALSE;
		const HRESULT Result = InDeviceContext->GetData(
			InReadbackState.CompletionQuery.Get(),
			&QueryResult,
			sizeof(QueryResult),
			D3D11_ASYNC_GETDATA_DONOTFLUSH);
		if (FAILED(Result))
		{
			return false;
		}

		OutReady = Result == S_OK && QueryResult == TRUE;
		return true;
	}

	void ResolveOcclusionTimings(
		ID3D11DeviceContext* InDeviceContext,
		const FSceneRenderer::FResources::FOcclusionReadbackState& InReadbackState,
		const std::chrono::steady_clock::time_point& InSubmissionTime,
		FOcclusionTimingStats& OutTimings)
	{
		OutTimings = {};

		if (InSubmissionTime != std::chrono::steady_clock::time_point())
		{
			const auto ResolveTime = std::chrono::steady_clock::now();
			OutTimings.ReadbackLatencyTimeMs = static_cast<float>(
				std::chrono::duration<double, std::milli>(ResolveTime - InSubmissionTime).count());
		}

		if (InDeviceContext == nullptr
			|| !InReadbackState.GpuTimingDisjointQuery
			|| !InReadbackState.OcclusionCullStartQuery
			|| !InReadbackState.OcclusionCullEndQuery)
		{
			return;
		}

		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData = {};
		if (InDeviceContext->GetData(
			InReadbackState.GpuTimingDisjointQuery.Get(),
			&DisjointData,
			sizeof(DisjointData),
			0) != S_OK
			|| DisjointData.Disjoint
			|| DisjointData.Frequency == 0)
		{
			return;
		}

		TryResolveTimestampRangeMs(
			InDeviceContext,
			InReadbackState.OcclusionCullStartQuery.Get(),
			InReadbackState.OcclusionCullEndQuery.Get(),
			DisjointData.Frequency,
			OutTimings.OcclusionCullGpuTimeMs);
	}

	void ResolvePendingHzbBuildTiming(ID3D11DeviceContext* InDeviceContext, FSceneRenderer::FResources& InOutResources)
	{
		if (!InOutResources.bPendingHzbBuildTiming
			|| InDeviceContext == nullptr
			|| !InOutResources.HzbBuildCompletionQuery
			|| !InOutResources.HzbBuildTimingDisjointQuery
			|| !InOutResources.HzbBuildStartQuery
			|| !InOutResources.HzbBuildEndQuery)
		{
			return;
		}

		BOOL QueryResult = FALSE;
		if (InDeviceContext->GetData(
			InOutResources.HzbBuildCompletionQuery.Get(),
			&QueryResult,
			sizeof(QueryResult),
			D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK
			|| QueryResult != TRUE)
		{
			return;
		}

		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData = {};
		if (InDeviceContext->GetData(
			InOutResources.HzbBuildTimingDisjointQuery.Get(),
			&DisjointData,
			sizeof(DisjointData),
			D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK
			|| DisjointData.Disjoint
			|| DisjointData.Frequency == 0)
		{
			InOutResources.bPendingHzbBuildTiming = false;
			return;
		}

		TryResolveTimestampRangeMs(
			InDeviceContext,
			InOutResources.HzbBuildStartQuery.Get(),
			InOutResources.HzbBuildEndQuery.Get(),
			DisjointData.Frequency,
			InOutResources.LastHzbBuildGpuTimeMs);
		InOutResources.bPendingHzbBuildTiming = false;
	}

	void UnbindComputeResources(ID3D11DeviceContext* InDeviceContext)
	{
		if (InDeviceContext == nullptr)
		{
			return;
		}

		ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
		ID3D11UnorderedAccessView* NullUAVs[1] = { nullptr };
		ID3D11Buffer* NullBuffers[1] = { nullptr };
		UINT InitialCounts[1] = { 0 };
		InDeviceContext->CSSetShaderResources(0, 2, NullSRVs);
		InDeviceContext->CSSetUnorderedAccessViews(0, 1, NullUAVs, InitialCounts);
		InDeviceContext->CSSetConstantBuffers(0, 1, NullBuffers);
		InDeviceContext->CSSetShader(nullptr, nullptr, 0);
	}

	void UnbindPixelShaderResources(ID3D11DeviceContext* InDeviceContext)
	{
		if (InDeviceContext == nullptr)
		{
			return;
		}

		ID3D11ShaderResourceView* NullSRV[1] = { nullptr };
		InDeviceContext->PSSetShaderResources(0, 1, NullSRV);
	}

	bool UploadGpuCandidates(
		ID3D11DeviceContext* InDeviceContext,
		ID3D11Buffer* InCandidateBuffer,
		const FVisibilityFrameInput& InVisibilityFrameInput)
	{
		if (InDeviceContext == nullptr || InCandidateBuffer == nullptr)
		{
			return false;
		}

		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (FAILED(InDeviceContext->Map(InCandidateBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
		{
			return false;
		}

		FGpuOcclusionCandidate* Candidates = static_cast<FGpuOcclusionCandidate*>(Mapped.pData);
		for (size_t CandidateIndex = 0; CandidateIndex < InVisibilityFrameInput.CandidateClusterIndices.size(); ++CandidateIndex)
		{
			const uint32 ClusterIndex = InVisibilityFrameInput.CandidateClusterIndices[CandidateIndex];
			FGpuOcclusionCandidate Candidate = {};
			if (ClusterIndex < InVisibilityFrameInput.FrustumVisibleClusters.size())
			{
				const FVisibilityCluster& Cluster = InVisibilityFrameInput.FrustumVisibleClusters[ClusterIndex];
				Candidate.BoundsMin = Cluster.BoundsMin;
				Candidate.BoundsMax = Cluster.BoundsMax;
			}

			Candidates[CandidateIndex] = Candidate;
		}

		InDeviceContext->Unmap(InCandidateBuffer, 0);
		return true;
	}

	bool RunDepthOnlyPrepass(
		const FD3D11RHI& InRHI,
		const FScene& InScene,
		const FCamera& InCamera,
		const TArray<uint32>& InVisiblePrimitiveIndices,
		FSceneRenderer::FResources& InOutResources)
	{
		ID3D11DeviceContext1* DeviceContext = InRHI.GetDeviceContext1();
		if (DeviceContext == nullptr)
		{
			return false;
		}

		DeviceContext->OMSetRenderTargets(0, nullptr, InRHI.GetDepthStencilView());
		const D3D11_VIEWPORT Viewport = InRHI.GetViewport();
		DeviceContext->RSSetViewports(1, &Viewport);

		DeviceContext->IASetInputLayout(InOutResources.InputLayout.Get());
		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		DeviceContext->VSSetShader(InOutResources.VertexShader.Get(), nullptr, 0);
		DeviceContext->PSSetShader(nullptr, nullptr, 0);
		DeviceContext->RSSetState(InOutResources.RasterizerState.Get());
		DeviceContext->OMSetDepthStencilState(InOutResources.DepthOnlyDepthStencilState.Get(), 0);
		DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffffu);

		FFrameConstants FrameConstants = {};
		FrameConstants.ViewProjection = InCamera.GetViewMatrix() * InCamera.GetProjectionMatrix();
		D3D11Utils::UpdateDynamicBuffer(DeviceContext, InOutResources.FrameConstantBuffer.Get(), FrameConstants);

		ID3D11Buffer* VertexConstantBuffer = InOutResources.FrameConstantBuffer.Get();
		DeviceContext->VSSetConstantBuffers(0, 1, &VertexConstantBuffer);

		const FPickState NoPickState = {};
		FVisibilityResults VisibleResults = {};
		VisibleResults.VisiblePrimitiveIndices = InVisiblePrimitiveIndices;
		if (!PrepareRenderPassResources(
			InRHI,
			InScene,
			VisibleResults,
			NoPickState,
			ERenderPassType::DepthOnly,
			InOutResources))
		{
			return false;
		}

		DrawPreparedRenderItems(DeviceContext, InOutResources, false);
		return true;
	}

	bool BuildHzbFromDepth(const FD3D11RHI& InRHI, FSceneRenderer::FResources& InOutResources)
	{
		ID3D11DeviceContext* DeviceContext = InRHI.GetDeviceContext();
		ID3D11Device* Device = InRHI.GetDevice();
		if (DeviceContext == nullptr || Device == nullptr)
		{
			return false;
		}

		const UINT Width = static_cast<UINT>(InRHI.GetViewportWidth());
		const UINT Height = static_cast<UINT>(InRHI.GetViewportHeight());
		if (!EnsureHzbResources(Device, InOutResources, Width, Height))
		{
			return false;
		}

		DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

		FHzbBuildConstants BuildConstants = {};
		BuildConstants.SourceMip = 0;
		BuildConstants.OutputWidth = Width;
		BuildConstants.OutputHeight = Height;
		D3D11Utils::UpdateDynamicBuffer(DeviceContext, InOutResources.HzbBuildConstantBuffer.Get(), BuildConstants);

		ID3D11ShaderResourceView* DepthSRV = InRHI.GetDepthStencilShaderResourceView();
		ID3D11UnorderedAccessView* Mip0UAV = InOutResources.HzbMipUAVs[0].Get();
		ID3D11Buffer* BuildConstantBuffer = InOutResources.HzbBuildConstantBuffer.Get();
		DeviceContext->CSSetShader(InOutResources.DepthToHzbMip0ComputeShader.Get(), nullptr, 0);
		DeviceContext->CSSetConstantBuffers(0, 1, &BuildConstantBuffer);
		DeviceContext->CSSetShaderResources(0, 1, &DepthSRV);
		DeviceContext->CSSetUnorderedAccessViews(0, 1, &Mip0UAV, nullptr);
		DeviceContext->Dispatch(
			DivideAndRoundUp(Width, HzbThreadGroupSizeX),
			DivideAndRoundUp(Height, HzbThreadGroupSizeY),
			1);
		UnbindComputeResources(DeviceContext);

		for (UINT SourceMip = 0; (SourceMip + 1u) < InOutResources.HzbMipCount; ++SourceMip)
		{
			const UINT DestMip = SourceMip + 1u;
			BuildConstants.SourceMip = SourceMip;
			BuildConstants.OutputWidth = std::max(Width >> DestMip, 1u);
			BuildConstants.OutputHeight = std::max(Height >> DestMip, 1u);
			D3D11Utils::UpdateDynamicBuffer(DeviceContext, InOutResources.HzbBuildConstantBuffer.Get(), BuildConstants);

			ID3D11ShaderResourceView* HzbSRV = InOutResources.HzbMipSRVs[SourceMip].Get();
			ID3D11UnorderedAccessView* DestUAV = InOutResources.HzbMipUAVs[DestMip].Get();
			DeviceContext->CSSetShader(InOutResources.ReduceHzbMipComputeShader.Get(), nullptr, 0);
			DeviceContext->CSSetConstantBuffers(0, 1, &BuildConstantBuffer);
			DeviceContext->CSSetShaderResources(0, 1, &HzbSRV);
			DeviceContext->CSSetUnorderedAccessViews(0, 1, &DestUAV, nullptr);
			DeviceContext->Dispatch(
				DivideAndRoundUp(BuildConstants.OutputWidth, HzbThreadGroupSizeX),
				DivideAndRoundUp(BuildConstants.OutputHeight, HzbThreadGroupSizeY),
				1);
			UnbindComputeResources(DeviceContext);
		}

		return true;
	}

	bool DispatchOcclusionCull(
		const FD3D11RHI& InRHI,
		const FCamera& InCamera,
		const FVisibilityFrameInput& InVisibilityFrameInput,
		FSceneRenderer::FResources& InOutResources,
		FSceneRenderer::FResources::FOcclusionReadbackState& InOutReadbackState)
	{
		const UINT CandidateCount = static_cast<UINT>(InVisibilityFrameInput.CandidateClusterIndices.size());
		if (CandidateCount == 0)
		{
			return true;
		}

		ID3D11Device* Device = InRHI.GetDevice();
		ID3D11DeviceContext* DeviceContext = InRHI.GetDeviceContext();
		if (Device == nullptr || DeviceContext == nullptr)
		{
			return false;
		}

		if (!EnsureCandidateBufferCapacity(Device, InOutResources, CandidateCount)
			|| !EnsureVisibilityFlagBufferCapacity(Device, InOutReadbackState, CandidateCount)
			|| !EnsureOcclusionQueries(Device, InOutReadbackState)
			|| !UploadGpuCandidates(DeviceContext, InOutResources.CandidateBuffer.Get(), InVisibilityFrameInput))
		{
			return false;
		}

		FOcclusionCullConstants CullConstants = {};
		CullConstants.View = InCamera.GetViewMatrix();
		CullConstants.ViewProjection = CullConstants.View * InCamera.GetProjectionMatrix();
		CullConstants.CandidateCount = CandidateCount;
		CullConstants.MipCount = InOutResources.HzbMipCount;
		CullConstants.DepthWidth = static_cast<uint32>(InRHI.GetViewportWidth());
		CullConstants.DepthHeight = static_cast<uint32>(InRHI.GetViewportHeight());
		CullConstants.NearClip = InCamera.GetNearClip();
		CullConstants.DepthEpsilon = GpuDepthEpsilon;
		if (!D3D11Utils::UpdateDynamicBuffer(DeviceContext, InOutResources.OcclusionCullConstantBuffer.Get(), CullConstants))
		{
			return false;
		}

		ID3D11Buffer* ConstantBuffer = InOutResources.OcclusionCullConstantBuffer.Get();
		ID3D11ShaderResourceView* SRVs[2] =
		{
			InOutResources.CandidateBufferSRV.Get(),
			InOutResources.HzbTextureSRV.Get(),
		};
		ID3D11UnorderedAccessView* UAV = InOutReadbackState.VisibilityFlagBufferUAV.Get();
		DeviceContext->CSSetShader(InOutResources.OcclusionCullComputeShader.Get(), nullptr, 0);
		DeviceContext->CSSetConstantBuffers(0, 1, &ConstantBuffer);
		DeviceContext->CSSetShaderResources(0, 2, SRVs);
		DeviceContext->CSSetUnorderedAccessViews(0, 1, &UAV, nullptr);
		DeviceContext->Dispatch(DivideAndRoundUp(CandidateCount, OcclusionCullThreadGroupSizeX), 1, 1);
		UnbindComputeResources(DeviceContext);

		DeviceContext->CopyResource(
			InOutReadbackState.VisibilityFlagStagingBuffer.Get(),
			InOutReadbackState.VisibilityFlagBuffer.Get());
		DeviceContext->End(InOutReadbackState.CompletionQuery.Get());
		return true;
	}

	bool ReadbackOcclusionCullResults(
		ID3D11DeviceContext* InDeviceContext,
		const FSceneRenderer::FResources::FOcclusionReadbackState& InReadbackState,
		const FVisibilityFrameInput& InVisibilityFrameInput,
		TArray<uint32>& OutVisibleClusterIndices,
		FOcclusionTimingStats& InOutOcclusionTimings)
	{
		OutVisibleClusterIndices.clear();
		InOutOcclusionTimings.ReadbackCopyCpuTimeMs = 0.0f;

		if (InDeviceContext == nullptr)
		{
			return false;
		}

		const UINT CandidateCount = static_cast<UINT>(InVisibilityFrameInput.CandidateClusterIndices.size());
		if (CandidateCount == 0)
		{
			return true;
		}

		std::vector<uint32> VisibleFlags(CandidateCount, 0u);

		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		const auto ReadbackStart = std::chrono::high_resolution_clock::now();
		if (FAILED(InDeviceContext->Map(InReadbackState.VisibilityFlagStagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &Mapped)))
		{
			return false;
		}
		std::memcpy(VisibleFlags.data(), Mapped.pData, VisibleFlags.size() * sizeof(uint32));
		InDeviceContext->Unmap(InReadbackState.VisibilityFlagStagingBuffer.Get(), 0);
		const auto ReadbackEnd = std::chrono::high_resolution_clock::now();
		InOutOcclusionTimings.ReadbackCopyCpuTimeMs = static_cast<float>(std::chrono::duration<double, std::milli>(ReadbackEnd - ReadbackStart).count());

		OutVisibleClusterIndices.reserve(CandidateCount);
		for (size_t CandidateIndex = 0; CandidateIndex < InVisibilityFrameInput.CandidateClusterIndices.size(); ++CandidateIndex)
		{
			if (VisibleFlags[CandidateIndex] == 0u)
			{
				continue;
			}

			OutVisibleClusterIndices.push_back(InVisibilityFrameInput.CandidateClusterIndices[CandidateIndex]);
		}

		return true;
	}

}

FSceneRenderer::FSceneRenderer() = default;
FSceneRenderer::~FSceneRenderer() = default;

bool FSceneRenderer::Initialize(FD3D11RHI& InRHI)
{
	ID3D11Device* Device = InRHI.GetDevice();
	ID3D11DeviceContext1* DeviceContext = InRHI.GetDeviceContext1();
	if (Device == nullptr || DeviceContext == nullptr)
	{
		return false;
	}

	Resources = std::make_unique<FResources>();
	if (!Resources)
	{
		return false;
	}

	static constexpr char VertexShaderSource[] = R"(
cbuffer FrameCB : register(b0)
{
    row_major float4x4 ViewProjection;
};

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 World;
    float4 Tint;
};

struct VSInput
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VSOutput VSMain(VSInput Input)
{
    VSOutput Output;
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);
    Output.Position = mul(WorldPosition, ViewProjection);
    Output.TexCoord = Input.TexCoord;
    return Output;
}
)";

	static constexpr char PixelShaderSource[] = R"(
cbuffer ObjectCB : register(b1)
{
    row_major float4x4 World;
    float4 Tint;
};

Texture2D DiffuseTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 PSMain(PSInput Input) : SV_Target
{
    return DiffuseTexture.Sample(LinearSampler, Input.TexCoord) * Tint;
}
)";

	TComPtr<ID3DBlob> VertexShaderBlob;
	TComPtr<ID3DBlob> PixelShaderBlob;
	if (!D3D11Utils::CompileShaderFromSource(VertexShaderSource, "VSMain", "vs_5_0", VertexShaderBlob, "SceneRenderer vertex shader")
		|| !D3D11Utils::CompileShaderFromSource(PixelShaderSource, "PSMain", "ps_5_0", PixelShaderBlob, "SceneRenderer pixel shader"))
	{
		Resources.reset();
		return false;
	}

	if (FAILED(Device->CreateVertexShader(
		VertexShaderBlob->GetBufferPointer(),
		VertexShaderBlob->GetBufferSize(),
		nullptr,
		Resources->VertexShader.GetAddressOf()))
		|| FAILED(Device->CreatePixelShader(
			PixelShaderBlob->GetBufferPointer(),
			PixelShaderBlob->GetBufferSize(),
			nullptr,
			Resources->PixelShader.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC InputElements[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(FStaticMeshVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(FStaticMeshVertex, TexCoord)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	if (FAILED(Device->CreateInputLayout(
		InputElements,
		_countof(InputElements),
		VertexShaderBlob->GetBufferPointer(),
		VertexShaderBlob->GetBufferSize(),
		Resources->InputLayout.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	if (!D3D11Utils::CreateDynamicConstantBuffer(Device, sizeof(FFrameConstants), Resources->FrameConstantBuffer)
		|| !D3D11Utils::CreateDynamicConstantBuffer(Device, sizeof(FHzbBuildConstants), Resources->HzbBuildConstantBuffer)
		|| !D3D11Utils::CreateDynamicConstantBuffer(Device, sizeof(FOcclusionCullConstants), Resources->OcclusionCullConstantBuffer)
		|| !EnsureObjectConstantBufferCapacity(Device, *Resources, 1))
	{
		Resources.reset();
		return false;
	}

	const D3D11_SAMPLER_DESC SamplerDesc =
	{
		D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_TEXTURE_ADDRESS_WRAP,
		0.0f,
		1,
		D3D11_COMPARISON_ALWAYS,
		{ 0.0f, 0.0f, 0.0f, 0.0f },
		0.0f,
		D3D11_FLOAT32_MAX
	};

	if (FAILED(Device->CreateSamplerState(&SamplerDesc, Resources->LinearSampler.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	const D3D11_RASTERIZER_DESC RasterizerDesc =
	{
		D3D11_FILL_SOLID,
		D3D11_CULL_BACK,
		FALSE,
		0,
		0.0f,
		0.0f,
		TRUE,
		FALSE,
		FALSE,
		FALSE
	};

	if (FAILED(Device->CreateRasterizerState(&RasterizerDesc, Resources->RasterizerState.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc = {};
	DepthStencilDesc.DepthEnable = TRUE;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	// Standard-Z HZB contract:
	// - scene depth clears to 1.0
	// - raster depth uses LESS_EQUAL
	// - HZB stores the farthest depth via max reduction
	// - occlusion compares candidate min depth against HZB max depth + epsilon
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DepthStencilDesc.StencilEnable = FALSE;

#ifndef NDEBUG
	ValidateHzbStandardZContract();
	assert(DepthStencilDesc.DepthFunc == D3D11_COMPARISON_LESS_EQUAL);
#endif

	if (FAILED(Device->CreateDepthStencilState(&DepthStencilDesc, Resources->DepthStencilState.GetAddressOf()))
		|| FAILED(Device->CreateDepthStencilState(&DepthStencilDesc, Resources->DepthOnlyDepthStencilState.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	D3D11_BLEND_DESC BlendDesc = {};
	BlendDesc.RenderTarget[0].BlendEnable = FALSE;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	if (FAILED(Device->CreateBlendState(&BlendDesc, Resources->BlendState.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	if (!CreateWhiteTexture(Device, Resources->WhiteTextureView))
	{
		Resources.reset();
		return false;
	}

	const std::filesystem::path DepthToHzbShaderPath = FindHzbShaderPath(L"DepthToHzbMip0CS.hlsl");
	const std::filesystem::path ReduceHzbShaderPath = FindHzbShaderPath(L"ReduceHzbMipCS.hlsl");
	const std::filesystem::path OcclusionCullShaderPath = FindHzbShaderPath(L"OcclusionCullCS.hlsl");
	if (!D3D11Utils::CreateComputeShaderFromFile(Device, DepthToHzbShaderPath, "CSMain", Resources->DepthToHzbMip0ComputeShader, "DepthToHzbMip0CS")
		|| !D3D11Utils::CreateComputeShaderFromFile(Device, ReduceHzbShaderPath, "CSMain", Resources->ReduceHzbMipComputeShader, "ReduceHzbMipCS")
		|| !D3D11Utils::CreateComputeShaderFromFile(Device, OcclusionCullShaderPath, "CSMain", Resources->OcclusionCullComputeShader, "OcclusionCullCS"))
	{
		Resources.reset();
		return false;
	}

	return true;
}

void FSceneRenderer::Shutdown()
{
	Resources.reset();
}

void FSceneRenderer::InvalidateDelayedVisibility()
{
	if (!Resources)
	{
		return;
	}

	for (FResources::FOcclusionReadbackState& ReadbackState : Resources->OcclusionReadbackSlots)
	{
		ResetOcclusionReadbackState(ReadbackState);
	}

	Resources->OldestPendingReadbackSlotIndex = 0;
	Resources->NextSubmissionReadbackSlotIndex = 0;
	Resources->PendingReadbackCount = 0;
	Resources->LastResolvedOcclusionFrameNumber = 0;
	Resources->bHzbValid = false;
	Resources->bPendingHzbBuildTiming = false;
	Resources->LastHzbBuildGpuTimeMs = 0.0f;
}

bool FSceneRenderer::ResolveGpuVisibility(
	const FD3D11RHI& InRHI,
	const FCamera& InCamera,
	const FVisibilityFrameInput& InVisibilityFrameInput,
	TArray<uint32>& OutVisibleClusterIndices,
	FOcclusionTimingStats& OutOcclusionTimings,
	FVisibilityFrameInput& OutResolvedFrameInput,
	bool& OutResolvedDelayedResult,
	bool& OutHasPendingReadback)
{
	OutVisibleClusterIndices.clear();
	OutOcclusionTimings = {};
	OutResolvedFrameInput = FVisibilityFrameInput();
	OutResolvedDelayedResult = false;
	OutHasPendingReadback = false;

	if (!Resources)
	{
		return false;
	}

	ID3D11DeviceContext* DeviceContext = InRHI.GetDeviceContext();
	if (DeviceContext == nullptr)
	{
		return false;
	}

	ResolvePendingHzbBuildTiming(DeviceContext, *Resources);
	OutOcclusionTimings.HzbBuildGpuTimeMs = Resources->LastHzbBuildGpuTimeMs;

	if (Resources->PendingReadbackCount > 0)
	{
		std::array<bool, OcclusionReadbackSlotCount> ReadySlots = {};
		size_t LatestReadySlotIndex = OcclusionReadbackSlotCount;
		uint64 LatestReadyFrameNumber = 0;

		for (size_t SlotIndex = 0; SlotIndex < OcclusionReadbackSlotCount; ++SlotIndex)
		{
			FResources::FOcclusionReadbackState& ReadbackState = Resources->OcclusionReadbackSlots[SlotIndex];
			if (!ReadbackState.bPendingReadback)
			{
				continue;
			}

			bool bReadbackReady = false;
			if (!IsOcclusionReadbackReady(DeviceContext, ReadbackState, bReadbackReady))
			{
				return false;
			}

			ReadySlots[SlotIndex] = bReadbackReady;
			if (!bReadbackReady
				|| ReadbackState.FrameInput.FrameNumber <= Resources->LastResolvedOcclusionFrameNumber)
			{
				continue;
			}

			if (LatestReadySlotIndex == OcclusionReadbackSlotCount
				|| ReadbackState.FrameInput.FrameNumber > LatestReadyFrameNumber)
			{
				LatestReadySlotIndex = SlotIndex;
				LatestReadyFrameNumber = ReadbackState.FrameInput.FrameNumber;
			}
		}

		if (LatestReadySlotIndex != OcclusionReadbackSlotCount)
		{
			FResources::FOcclusionReadbackState& LatestReadyState =
				Resources->OcclusionReadbackSlots[LatestReadySlotIndex];
			ResolveOcclusionTimings(
				DeviceContext,
				LatestReadyState,
				LatestReadyState.SubmissionWallClockTime,
				OutOcclusionTimings);
			OutOcclusionTimings.HzbBuildGpuTimeMs = Resources->LastHzbBuildGpuTimeMs;
			if (!ReadbackOcclusionCullResults(
				DeviceContext,
				LatestReadyState,
				LatestReadyState.FrameInput,
				OutVisibleClusterIndices,
				OutOcclusionTimings))
			{
				return false;
			}

			OutResolvedFrameInput = LatestReadyState.FrameInput;
			OutResolvedDelayedResult = true;
			Resources->LastResolvedOcclusionFrameNumber = LatestReadyState.FrameInput.FrameNumber;
		}

		for (size_t SlotIndex = 0; SlotIndex < OcclusionReadbackSlotCount; ++SlotIndex)
		{
			if (!ReadySlots[SlotIndex])
			{
				continue;
			}

			ResetOcclusionReadbackState(Resources->OcclusionReadbackSlots[SlotIndex]);
		}

		RefreshOcclusionReadbackRingState(*Resources);
	}

	OutHasPendingReadback = Resources->PendingReadbackCount > 0;

	if (!Resources->bHzbValid
		|| !InVisibilityFrameInput.bOcclusionValid
		|| InVisibilityFrameInput.CandidateClusterIndices.empty())
	{
		return true;
	}

	ID3D11Device* Device = InRHI.GetDevice();
	if (Device == nullptr)
	{
		return false;
	}

	if (Resources->PendingReadbackCount >= OcclusionReadbackSlotCount)
	{
		OutHasPendingReadback = true;
		return true;
	}

	const size_t SubmissionSlotIndex = FindAvailableOcclusionReadbackSlotIndex(*Resources);
	if (SubmissionSlotIndex == OcclusionReadbackSlotCount)
	{
		return false;
	}
	FResources::FOcclusionReadbackState& SubmissionState =
		Resources->OcclusionReadbackSlots[SubmissionSlotIndex];

	if (!EnsureOcclusionQueries(Device, SubmissionState))
	{
		return false;
	}

	const auto SubmissionTime = std::chrono::steady_clock::now();
	const bool bRecordGpuTimings = SubmissionState.GpuTimingDisjointQuery
		&& SubmissionState.OcclusionCullStartQuery
		&& SubmissionState.OcclusionCullEndQuery;
	if (bRecordGpuTimings)
	{
		DeviceContext->Begin(SubmissionState.GpuTimingDisjointQuery.Get());
		DeviceContext->End(SubmissionState.OcclusionCullStartQuery.Get());
	}

	if (!DispatchOcclusionCull(
		InRHI,
		InCamera,
		InVisibilityFrameInput,
		*Resources,
		SubmissionState))
	{
		if (bRecordGpuTimings)
		{
			DeviceContext->End(SubmissionState.GpuTimingDisjointQuery.Get());
		}
		return false;
	}
	if (bRecordGpuTimings)
	{
		DeviceContext->End(SubmissionState.OcclusionCullEndQuery.Get());
		DeviceContext->End(SubmissionState.GpuTimingDisjointQuery.Get());
	}

	SubmissionState.FrameInput = InVisibilityFrameInput;
	SubmissionState.SubmissionWallClockTime = SubmissionTime;
	SubmissionState.bPendingReadback = true;
	Resources->NextSubmissionReadbackSlotIndex =
		(SubmissionSlotIndex + 1u) % OcclusionReadbackSlotCount;
	RefreshOcclusionReadbackRingState(*Resources);
	OutHasPendingReadback = Resources->PendingReadbackCount > 0;
	return true;
}

bool FSceneRenderer::RenderVisibleScene(
	const FD3D11RHI& InRHI,
	const FScene& InScene,
	const FCamera& InCamera,
	const FVisibilityResults& InVisibilityResults,
	const FPickState& InPickState)
{
	if (!Resources)
	{
		return false;
	}

	ID3D11DeviceContext1* DeviceContext = InRHI.GetDeviceContext1();
	if (DeviceContext == nullptr)
	{
		return false;
	}

	DeviceContext->ClearDepthStencilView(InRHI.GetDepthStencilView(), D3D11_CLEAR_DEPTH, SceneDepthClearValue, 0);

	ID3D11RenderTargetView* RenderTargets[] = { InRHI.GetBackBufferRTV() };
	const D3D11_VIEWPORT Viewport = InRHI.GetViewport();
	DeviceContext->OMSetRenderTargets(1, RenderTargets, InRHI.GetDepthStencilView());
	DeviceContext->RSSetViewports(1, &Viewport);

	DeviceContext->IASetInputLayout(Resources->InputLayout.Get());
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->VSSetShader(Resources->VertexShader.Get(), nullptr, 0);
	DeviceContext->PSSetShader(Resources->PixelShader.Get(), nullptr, 0);
	DeviceContext->RSSetState(Resources->RasterizerState.Get());
	DeviceContext->OMSetDepthStencilState(Resources->DepthStencilState.Get(), 0);
	DeviceContext->OMSetBlendState(Resources->BlendState.Get(), nullptr, 0xffffffffu);

	ID3D11SamplerState* Samplers[] = { Resources->LinearSampler.Get() };
	DeviceContext->PSSetSamplers(0, 1, Samplers);

	FFrameConstants FrameConstants = {};
	FrameConstants.ViewProjection = InCamera.GetViewMatrix() * InCamera.GetProjectionMatrix();
	D3D11Utils::UpdateDynamicBuffer(DeviceContext, Resources->FrameConstantBuffer.Get(), FrameConstants);

	ID3D11Buffer* VertexConstantBuffer = Resources->FrameConstantBuffer.Get();
	DeviceContext->VSSetConstantBuffers(0, 1, &VertexConstantBuffer);

	if (!PrepareRenderPassResources(
		InRHI,
		InScene,
		InVisibilityResults,
		InPickState,
		ERenderPassType::BasePass,
		*Resources))
	{
		return false;
	}

	DrawPreparedRenderItems(DeviceContext, *Resources, true);
	UnbindPixelShaderResources(DeviceContext);

	ID3D11Device* Device = InRHI.GetDevice();
	ID3D11DeviceContext* RawDeviceContext = InRHI.GetDeviceContext();
	const bool bCanRecordHzbTiming = Device != nullptr && RawDeviceContext != nullptr && EnsureHzbTimingQueries(Device, *Resources);
	if (bCanRecordHzbTiming)
	{
		RawDeviceContext->Begin(Resources->HzbBuildTimingDisjointQuery.Get());
		RawDeviceContext->End(Resources->HzbBuildStartQuery.Get());
	}

	const bool bBuiltHzb = BuildHzbFromDepth(InRHI, *Resources);
	if (bCanRecordHzbTiming)
	{
		RawDeviceContext->End(Resources->HzbBuildEndQuery.Get());
		RawDeviceContext->End(Resources->HzbBuildTimingDisjointQuery.Get());
		RawDeviceContext->End(Resources->HzbBuildCompletionQuery.Get());
	}

	Resources->bHzbValid = bBuiltHzb;
	Resources->bPendingHzbBuildTiming = bBuiltHzb && bCanRecordHzbTiming;
	if (!bBuiltHzb)
	{
		Resources->LastHzbBuildGpuTimeMs = 0.0f;
	}

	return bBuiltHzb;
}
