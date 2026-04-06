#include "Renderer/SceneRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <sstream>
#include <vector>

#include <Windows.h>

#include "Camera/Camera.h"
#include "Graphics/D3D11/D3D11Utils.h"
#include "Graphics/D3D11/D3D11RHI.h"
#include "Math/MathUtility.h"
#include "Picking/PickingSystem.h"
#include "Scene/Scene.h"
#include "StaticMesh/StaticMesh.h"
#include "Visibility/VisibilitySystem.h"

namespace
{
	constexpr std::array<float, 2> ScreenSizeLODCutoffs = { 0.2f, 0.08f };

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
	TComPtr<ID3D11VertexShader> VertexShader;
	TComPtr<ID3D11PixelShader> PixelShader;
	TComPtr<ID3D11InputLayout> InputLayout;
	TComPtr<ID3D11Buffer> FrameConstantBuffer;
	TComPtr<ID3D11Buffer> ObjectConstantBuffer;
	TComPtr<ID3D11SamplerState> LinearSampler;
	TComPtr<ID3D11RasterizerState> RasterizerState;
	TComPtr<ID3D11DepthStencilState> DepthStencilState;
	TComPtr<ID3D11BlendState> BlendState;
	TComPtr<ID3D11ShaderResourceView> WhiteTextureView;
	std::vector<FObjectConstantsBlock> ObjectConstantBlocks;
	std::vector<FDrawItem> RenderItems;
	std::array<uint32, 8> LODSelectionCounts = {};
	uint32 VisiblePrimitiveCount = 0;
	UINT ObjectBufferCapacity = 0;
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

	uint32 SelectLODIndex(const FScenePrimitiveRuntimeData& InPrimitiveData, const FCamera& InCamera)
	{
		const FBoundingSphere& WorldSphere = InPrimitiveData.WorldBoundsSphere;
		if (WorldSphere.Radius <= 0.0f)
		{
			return 0;
		}

		const FVector CameraToCenter = WorldSphere.Center - InCamera.GetLocation();
		const float Depth = FVector::DotProduct(CameraToCenter, InCamera.GetRotation().GetForwardVector());
		const float SafeDepth = std::max(Depth, InCamera.GetNearClip());
		if (SafeDepth <= 0.0f)
		{
			return 0;
		}

		const float ProjectionScaleY = 1.0f / std::tan(FMath::DegreesToRadians(InCamera.GetFOV()) * 0.5f);
		const float ScreenDiameterRatio = (2.0f * WorldSphere.Radius * ProjectionScaleY) / SafeDepth;
		if (ScreenDiameterRatio >= ScreenSizeLODCutoffs[0])
		{
			return 0;
		}

		if (ScreenDiameterRatio >= ScreenSizeLODCutoffs[1])
		{
			return 1;
		}

		return 2;
	}

	template <typename TResources>
	void BuildRenderQueue(
		const FScene& InScene,
		const FCamera& InCamera,
		const FVisibilityResults& InVisibilityResults,
		const FPickState& InPickState,
		ID3D11ShaderResourceView* InDefaultTextureView,
		TResources& InOutResources)
	{
		InOutResources.ObjectConstantBlocks.clear();
		InOutResources.RenderItems.clear();
		InOutResources.LODSelectionCounts.fill(0);
		InOutResources.VisiblePrimitiveCount = 0;

		InOutResources.ObjectConstantBlocks.reserve(InVisibilityResults.VisiblePrimitiveIndices.size());
		InOutResources.RenderItems.reserve(InVisibilityResults.VisiblePrimitiveIndices.size());

		const TArray<FScenePrimitiveRuntimeData>& PrimitiveRuntimeData = InScene.GetPrimitiveRuntimeData();
		for (uint32 PrimitiveIndex : InVisibilityResults.VisiblePrimitiveIndices)
		{
			if (PrimitiveIndex >= PrimitiveRuntimeData.size())
			{
				continue;
			}

			++InOutResources.VisiblePrimitiveCount;

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

			const uint32 LODIndex = std::min(SelectLODIndex(PrimitiveData, InCamera), LODCount - 1);
			if (LODIndex < InOutResources.LODSelectionCounts.size())
			{
				++InOutResources.LODSelectionCounts[LODIndex];
			}

			ID3D11Buffer* VertexBuffer = StaticMesh->GetVertexBuffer(LODIndex);
			ID3D11Buffer* IndexBuffer = StaticMesh->GetIndexBuffer(LODIndex);
			if (VertexBuffer == nullptr || IndexBuffer == nullptr)
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

			for (const FStaticMesh::FSection& Section : StaticMesh->GetSections(LODIndex))
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

	template <typename TResources>
	void LogLODSelectionCounts(const FScene& InScene, const TResources& InResources)
	{
		std::ostringstream LogStream;
		uint32 SubmittedPrimitiveCount = 0;
		for (size_t LODIndex = 0; LODIndex < InResources.LODSelectionCounts.size(); ++LODIndex)
		{
			SubmittedPrimitiveCount += InResources.LODSelectionCounts[LODIndex];
		}

		LogStream
			<< "LOD Selection - Total: " << InScene.GetPrimitiveCount()
			<< ", Visible: " << InResources.VisiblePrimitiveCount
			<< ", Submitted: " << SubmittedPrimitiveCount;

		bool bHasAnyCount = false;
		for (size_t LODIndex = 0; LODIndex < InResources.LODSelectionCounts.size(); ++LODIndex)
		{
			const uint32 Count = InResources.LODSelectionCounts[LODIndex];
			if (Count == 0)
			{
				continue;
			}

			bHasAnyCount = true;
			LogStream << " - LOD" << LODIndex << ": " << Count;
		}

		if (!bHasAnyCount)
		{
			LogStream << " - LOD0: 0";
		}

		LogStream << '\n';
		OutputDebugStringA(LogStream.str().c_str());
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

	if (FAILED(Device->CreateDepthStencilState(&DepthStencilDesc, Resources->DepthStencilState.GetAddressOf())))
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

	return true;
}

void FSceneRenderer::Shutdown()
{
	Resources.reset();
}

void FSceneRenderer::Render(
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

	BuildRenderQueue(InScene, InCamera, InVisibilityResults, InPickState, Resources->WhiteTextureView.Get(), *Resources);
	LogLODSelectionCounts(InScene, *Resources);
	SortRenderItems(Resources->RenderItems);
	if (!EnsureObjectConstantBufferCapacity(
		InRHI.GetDevice(),
		*Resources,
		static_cast<UINT>(std::max<size_t>(Resources->ObjectConstantBlocks.size(), 1))))
	{
		return;
	}

	if (!UploadObjectConstantBlocks(DeviceContext, Resources->ObjectConstantBuffer.Get(), Resources->ObjectConstantBlocks))
	{
		return;
	}

	ID3D11Buffer* VertexConstantBuffer = Resources->FrameConstantBuffer.Get();
	DeviceContext->VSSetConstantBuffers(0, 1, &VertexConstantBuffer);

	ID3D11Buffer* CurrentVertexBuffer = nullptr;
	ID3D11Buffer* CurrentIndexBuffer = nullptr;
	ID3D11ShaderResourceView* CurrentTextureView = nullptr;
	UINT CurrentObjectIndex = UINT_MAX;
	const UINT Stride = sizeof(FStaticMeshVertex);
	const UINT Offset = 0;

	for (const FDrawItem& RenderItem : Resources->RenderItems)
	{
		ID3D11Buffer* VertexBuffer = RenderItem.VertexBuffer;
		if (VertexBuffer != CurrentVertexBuffer)
		{
			DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
			CurrentVertexBuffer = VertexBuffer;
		}

		ID3D11Buffer* IndexBuffer = RenderItem.IndexBuffer;
		if (IndexBuffer != CurrentIndexBuffer)
		{
			DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
			CurrentIndexBuffer = IndexBuffer;
		}

		if (RenderItem.ObjectIndex != CurrentObjectIndex)
		{
			BindObjectConstantRange(DeviceContext, Resources->ObjectConstantBuffer.Get(), RenderItem.ObjectIndex);
			CurrentObjectIndex = RenderItem.ObjectIndex;
		}

		ID3D11ShaderResourceView* TextureView = RenderItem.TextureView;
		if (TextureView != CurrentTextureView)
		{
			DeviceContext->PSSetShaderResources(0, 1, &TextureView);
			CurrentTextureView = TextureView;
		}

		DeviceContext->DrawIndexed(RenderItem.IndexCount, RenderItem.IndexStart, 0);
	}
}
