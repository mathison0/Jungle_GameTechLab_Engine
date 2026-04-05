#include "Renderer/SceneRenderer.h"

#include <algorithm>
#include <array>
#include <chrono>
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
	constexpr size_t DelayedReadbackSlotCount = 2;
	constexpr float GpuDepthEpsilon = 1.e-3f;

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
	struct FVisibilityReadbackSlot
	{
		TComPtr<ID3D11Buffer> VisibilityFlagBuffer;
		TComPtr<ID3D11UnorderedAccessView> VisibilityFlagBufferUAV;
		TComPtr<ID3D11Buffer> VisibilityFlagStagingBuffer;
		TComPtr<ID3D11Query> CompletionQuery;
		UINT VisibilityFlagBufferCapacity = 0;
		FVisibilityFrameInput FrameInput;
		uint32 CandidateCount = 0;
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

	std::array<FVisibilityReadbackSlot, DelayedReadbackSlotCount> VisibilityReadbackSlots = {};
	size_t OldestPendingReadbackSlotIndex = 0;
	size_t NextSubmissionSlotIndex = 0;
	size_t PendingReadbackCount = 0;
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
		const TArray<uint32>& InVisiblePrimitiveIndices,
		const FPickState& InPickState,
		ID3D11ShaderResourceView* InDefaultTextureView,
		TResources& InOutResources)
	{
		InOutResources.ObjectConstantBlocks.clear();
		InOutResources.RenderItems.clear();

		InOutResources.ObjectConstantBlocks.reserve(InVisiblePrimitiveIndices.size());
		InOutResources.RenderItems.reserve(InVisiblePrimitiveIndices.size());

		const TArray<FScenePrimitiveRuntimeData>& PrimitiveRuntimeData = InScene.GetPrimitiveRuntimeData();
		for (uint32 PrimitiveIndex : InVisiblePrimitiveIndices)
		{
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

			ID3D11Buffer* VertexBuffer = StaticMesh->GetVertexBuffer();
			ID3D11Buffer* IndexBuffer = StaticMesh->GetIndexBuffer();
			for (const FStaticMesh::FSection& Section : StaticMesh->GetSections())
			{
				FDrawItem RenderItem = {};
				RenderItem.VertexBuffer = VertexBuffer;
				RenderItem.IndexBuffer = IndexBuffer;
				RenderItem.TextureView = StaticMesh->GetMaterialTexture(Section.MaterialIndex);
				if (RenderItem.TextureView == nullptr)
				{
					RenderItem.TextureView = InDefaultTextureView;
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

	void SortRenderItems(std::vector<FDrawItem>& InOutRenderItems)
	{
		std::ranges::stable_sort(InOutRenderItems, [](const FDrawItem& InLeft, const FDrawItem& InRight)
		{
			if (const int VertexBufferOrder = ComparePointers(InLeft.VertexBuffer, InRight.VertexBuffer); VertexBufferOrder != 0)
			{
				return VertexBufferOrder < 0;
			}

			if (const int IndexBufferOrder = ComparePointers(InLeft.IndexBuffer, InRight.IndexBuffer); IndexBufferOrder != 0)
			{
				return IndexBufferOrder < 0;
			}

			if (const int TextureOrder = ComparePointers(InLeft.TextureView, InRight.TextureView); TextureOrder != 0)
			{
				return TextureOrder < 0;
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
		const TArray<uint32>& InVisiblePrimitiveIndices,
		const FPickState& InPickState,
		TResources& InOutResources)
	{
		BuildRenderQueue(InScene, InVisiblePrimitiveIndices, InPickState, InOutResources.WhiteTextureView.Get(), InOutResources);
		SortRenderItems(InOutResources.RenderItems);

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
		FSceneRenderer::FResources::FVisibilityReadbackSlot& InOutReadbackSlot,
		UINT InRequiredCount)
	{
		const UINT RequiredCount = std::max(InRequiredCount, 1u);
		if (RequiredCount <= InOutReadbackSlot.VisibilityFlagBufferCapacity)
		{
			return true;
		}

		UINT NewCapacity = std::max(RequiredCount, 1u);
		if (InOutReadbackSlot.VisibilityFlagBufferCapacity > 0)
		{
			NewCapacity = InOutReadbackSlot.VisibilityFlagBufferCapacity;
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

		InOutReadbackSlot.VisibilityFlagBuffer = std::move(Buffer);
		InOutReadbackSlot.VisibilityFlagBufferUAV = std::move(UAV);
		InOutReadbackSlot.VisibilityFlagStagingBuffer = std::move(StagingBuffer);
		InOutReadbackSlot.VisibilityFlagBufferCapacity = NewCapacity;
		return true;
	}

	bool CreateVisibilityReadbackQuery(
		ID3D11Device* InDevice,
		FSceneRenderer::FResources::FVisibilityReadbackSlot& InOutReadbackSlot)
	{
		if (InDevice == nullptr)
		{
			return false;
		}

		if (InOutReadbackSlot.CompletionQuery)
		{
			return true;
		}

		const D3D11_QUERY_DESC QueryDesc =
		{
			D3D11_QUERY_EVENT,
			0
		};

		return SUCCEEDED(InDevice->CreateQuery(&QueryDesc, InOutReadbackSlot.CompletionQuery.GetAddressOf()));
	}

	void ResetReadbackSlot(FSceneRenderer::FResources::FVisibilityReadbackSlot& InOutReadbackSlot)
	{
		InOutReadbackSlot.FrameInput = FVisibilityFrameInput();
		InOutReadbackSlot.CandidateCount = 0;
		InOutReadbackSlot.bPendingReadback = false;
	}

	bool IsReadbackSlotReady(
		ID3D11DeviceContext* InDeviceContext,
		const FSceneRenderer::FResources::FVisibilityReadbackSlot& InReadbackSlot,
		bool& OutReady)
	{
		OutReady = false;

		if (InDeviceContext == nullptr)
		{
			return false;
		}

		if (!InReadbackSlot.bPendingReadback || !InReadbackSlot.CompletionQuery)
		{
			return true;
		}

		BOOL QueryResult = FALSE;
		const HRESULT Result = InDeviceContext->GetData(
			InReadbackSlot.CompletionQuery.Get(),
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
		const FScene& InScene,
		const TArray<uint32>& InCandidateIndices)
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
		const TArray<FRenderItem>& RenderItems = InScene.GetRenderItems();
		for (size_t CandidateIndex = 0; CandidateIndex < InCandidateIndices.size(); ++CandidateIndex)
		{
			const uint32 PrimitiveIndex = InCandidateIndices[CandidateIndex];
			FGpuOcclusionCandidate Candidate = {};
			if (PrimitiveIndex < RenderItems.size())
			{
				Candidate.BoundsMin = RenderItems[PrimitiveIndex].WorldBoundsMin;
				Candidate.BoundsMax = RenderItems[PrimitiveIndex].WorldBoundsMax;
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
		if (!PrepareRenderPassResources(InRHI, InScene, InVisiblePrimitiveIndices, NoPickState, InOutResources))
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
		const FScene& InScene,
		const FCamera& InCamera,
		const TArray<uint32>& InCandidatePrimitiveIndices,
		FSceneRenderer::FResources& InOutResources,
		FSceneRenderer::FResources::FVisibilityReadbackSlot& InOutReadbackSlot)
	{
		const UINT CandidateCount = static_cast<UINT>(InCandidatePrimitiveIndices.size());
		if (CandidateCount == 0)
		{
			ResetReadbackSlot(InOutReadbackSlot);
			return true;
		}

		ID3D11Device* Device = InRHI.GetDevice();
		ID3D11DeviceContext* DeviceContext = InRHI.GetDeviceContext();
		if (Device == nullptr || DeviceContext == nullptr)
		{
			return false;
		}

		if (!EnsureCandidateBufferCapacity(Device, InOutResources, CandidateCount)
			|| !EnsureVisibilityFlagBufferCapacity(Device, InOutReadbackSlot, CandidateCount)
			|| !CreateVisibilityReadbackQuery(Device, InOutReadbackSlot)
			|| !UploadGpuCandidates(DeviceContext, InOutResources.CandidateBuffer.Get(), InScene, InCandidatePrimitiveIndices))
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
		ID3D11UnorderedAccessView* UAV = InOutReadbackSlot.VisibilityFlagBufferUAV.Get();
		DeviceContext->CSSetShader(InOutResources.OcclusionCullComputeShader.Get(), nullptr, 0);
		DeviceContext->CSSetConstantBuffers(0, 1, &ConstantBuffer);
		DeviceContext->CSSetShaderResources(0, 2, SRVs);
		DeviceContext->CSSetUnorderedAccessViews(0, 1, &UAV, nullptr);
		DeviceContext->Dispatch(DivideAndRoundUp(CandidateCount, OcclusionCullThreadGroupSizeX), 1, 1);
		UnbindComputeResources(DeviceContext);

		DeviceContext->End(InOutReadbackSlot.CompletionQuery.Get());
		return true;
	}

	bool ReadbackOcclusionCullResults(
		ID3D11DeviceContext* InDeviceContext,
		const FSceneRenderer::FResources::FVisibilityReadbackSlot& InReadbackSlot,
		TArray<uint32>& OutVisiblePrimitiveIndices,
		float& OutReadbackTimeMs)
	{
		OutVisiblePrimitiveIndices.clear();
		OutReadbackTimeMs = 0.0f;

		if (InDeviceContext == nullptr)
		{
			return false;
		}

		const UINT CandidateCount = InReadbackSlot.CandidateCount;
		if (CandidateCount == 0)
		{
			return true;
		}

		std::vector<uint32> VisibleFlags(CandidateCount, 0u);
		const auto ReadbackStart = std::chrono::high_resolution_clock::now();
		if (!D3D11Utils::ReadbackBuffer(
			InDeviceContext,
			InReadbackSlot.VisibilityFlagBuffer.Get(),
			InReadbackSlot.VisibilityFlagStagingBuffer.Get(),
			VisibleFlags.data(),
			VisibleFlags.size() * sizeof(uint32)))
		{
			return false;
		}
		const auto ReadbackEnd = std::chrono::high_resolution_clock::now();
		OutReadbackTimeMs = static_cast<float>(std::chrono::duration<double, std::milli>(ReadbackEnd - ReadbackStart).count());

		OutVisiblePrimitiveIndices.reserve(CandidateCount);
		for (size_t CandidateIndex = 0; CandidateIndex < InReadbackSlot.FrameInput.CandidatePrimitiveIndices.size(); ++CandidateIndex)
		{
			if (VisibleFlags[CandidateIndex] == 0u)
			{
				continue;
			}

			OutVisiblePrimitiveIndices.push_back(InReadbackSlot.FrameInput.CandidatePrimitiveIndices[CandidateIndex]);
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
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DepthStencilDesc.StencilEnable = FALSE;

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

	for (FResources::FVisibilityReadbackSlot& ReadbackSlot : Resources->VisibilityReadbackSlots)
	{
		ResetReadbackSlot(ReadbackSlot);
	}

	Resources->OldestPendingReadbackSlotIndex = 0;
	Resources->NextSubmissionSlotIndex = 0;
	Resources->PendingReadbackCount = 0;
}

bool FSceneRenderer::ResolveGpuVisibility(
	const FD3D11RHI& InRHI,
	const FScene& InScene,
	const FCamera& InCamera,
	const FVisibilityFrameInput& InVisibilityFrameInput,
	TArray<uint32>& OutVisiblePrimitiveIndices,
	FVisibilityFrameInput& OutResolvedFrameInput,
	uint32& OutCandidateCount,
	uint32& OutVisibleCount,
	float& OutReadbackTimeMs,
	bool& OutResolvedDelayedResult,
	bool& OutHasPendingReadback)
{
	OutVisiblePrimitiveIndices.clear();
	OutResolvedFrameInput = FVisibilityFrameInput();
	OutCandidateCount = 0;
	OutVisibleCount = 0;
	OutReadbackTimeMs = 0.0f;
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

	if (Resources->PendingReadbackCount > 0)
	{
		FResources::FVisibilityReadbackSlot& ReadbackSlot = Resources->VisibilityReadbackSlots[Resources->OldestPendingReadbackSlotIndex];
		bool bReadbackReady = false;
		if (!IsReadbackSlotReady(DeviceContext, ReadbackSlot, bReadbackReady))
		{
			return false;
		}

		if (bReadbackReady)
		{
			if (!ReadbackOcclusionCullResults(DeviceContext, ReadbackSlot, OutVisiblePrimitiveIndices, OutReadbackTimeMs))
			{
				return false;
			}

			OutResolvedFrameInput = ReadbackSlot.FrameInput;
			OutCandidateCount = ReadbackSlot.CandidateCount;
			OutVisibleCount = static_cast<uint32>(OutVisiblePrimitiveIndices.size());
			OutResolvedDelayedResult = true;

			ResetReadbackSlot(ReadbackSlot);
			Resources->OldestPendingReadbackSlotIndex = (Resources->OldestPendingReadbackSlotIndex + 1u) % DelayedReadbackSlotCount;
			--Resources->PendingReadbackCount;
		}
	}

	OutHasPendingReadback = Resources->PendingReadbackCount > 0;

	const uint32 CurrentFrameCandidateCount = static_cast<uint32>(InVisibilityFrameInput.CandidatePrimitiveIndices.size());
	if (CurrentFrameCandidateCount == 0 || InVisibilityFrameInput.SeedPrimitiveIndices.empty())
	{
		return true;
	}

	if (Resources->PendingReadbackCount >= DelayedReadbackSlotCount)
	{
		return true;
	}

	if (!RunDepthOnlyPrepass(InRHI, InScene, InCamera, InVisibilityFrameInput.SeedPrimitiveIndices, *Resources))
	{
		return false;
	}

	if (!BuildHzbFromDepth(InRHI, *Resources))
	{
		return false;
	}

	FResources::FVisibilityReadbackSlot& SubmissionSlot = Resources->VisibilityReadbackSlots[Resources->NextSubmissionSlotIndex];
	if (SubmissionSlot.bPendingReadback)
	{
		return false;
	}
	if (!DispatchOcclusionCull(
		InRHI,
		InScene,
		InCamera,
		InVisibilityFrameInput.CandidatePrimitiveIndices,
		*Resources,
		SubmissionSlot))
	{
		return false;
	}

	SubmissionSlot.FrameInput = InVisibilityFrameInput;
	SubmissionSlot.CandidateCount = CurrentFrameCandidateCount;
	SubmissionSlot.bPendingReadback = true;
	if (Resources->PendingReadbackCount == 0)
	{
		Resources->OldestPendingReadbackSlotIndex = Resources->NextSubmissionSlotIndex;
	}
	Resources->NextSubmissionSlotIndex = (Resources->NextSubmissionSlotIndex + 1u) % DelayedReadbackSlotCount;
	++Resources->PendingReadbackCount;
	OutHasPendingReadback = true;
	return true;
}

void FSceneRenderer::RenderVisibleScene(
	const FD3D11RHI& InRHI,
	const FScene& InScene,
	const FCamera& InCamera,
	const FVisibilityResults& InVisibilityResults,
	const FPickState& InPickState)
{
	if (!Resources)
	{
		return;
	}

	ID3D11DeviceContext1* DeviceContext = InRHI.GetDeviceContext1();
	if (DeviceContext == nullptr)
	{
		return;
	}

	DeviceContext->ClearDepthStencilView(InRHI.GetDepthStencilView(), D3D11_CLEAR_DEPTH, 1.0f, 0);

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
		InVisibilityResults.VisiblePrimitiveIndices,
		InPickState,
		*Resources))
	{
		return;
	}

	DrawPreparedRenderItems(DeviceContext, *Resources, true);
	UnbindPixelShaderResources(DeviceContext);
}
