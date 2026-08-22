#include "Gizmo/GizmoRenderer.h"

#include <cstddef>

#include "Camera/Camera.h"
#include "Gizmo/Gizmo.h"
#include "Gizmo/GizmoMeshFactory.h"
#include "Graphics/D3D11/D3D11RHI.h"
#include "Graphics/D3D11/D3D11Utils.h"

struct alignas(16) FGizmoFrameConstants
{
	FMatrix ViewProjection = FMatrix::Identity;
};

struct alignas(16) FGizmoObjectConstants
{
	FMatrix World = FMatrix::Identity;
	FVector4 Tint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float ColorBlend = 0.0f;
	float Padding[3] = {};
};

struct FGizmoRenderer::FResources
{
	TComPtr<ID3D11VertexShader> VertexShader;
	TComPtr<ID3D11PixelShader> PixelShader;
	TComPtr<ID3D11InputLayout> InputLayout;
	TComPtr<ID3D11Buffer> FrameConstantBuffer;
	TComPtr<ID3D11Buffer> ObjectConstantBuffer;
	TComPtr<ID3D11RasterizerState> RasterizerState;
	TComPtr<ID3D11DepthStencilState> DepthStencilState;
	TComPtr<ID3D11BlendState> BlendState;
};

FGizmoRenderer::FGizmoRenderer() = default;
FGizmoRenderer::~FGizmoRenderer() = default;

bool FGizmoRenderer::Initialize(FD3D11RHI& InRHI)
{
	ID3D11Device* Device = InRHI.GetDevice();
	if (Device == nullptr)
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
    float ColorBlend;
    float3 Padding;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
};

VSOutput VSMain(VSInput Input)
{
    VSOutput Output;
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);
    Output.Position = mul(WorldPosition, ViewProjection);
    Output.Color = lerp(Input.Color, Tint, saturate(ColorBlend));
    return Output;
}
)";

	static constexpr char PixelShaderSource[] = R"(
struct PSInput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
};

float4 PSMain(PSInput Input) : SV_Target
{
    return Input.Color;
}
)";

	TComPtr<ID3DBlob> VertexShaderBlob;
	TComPtr<ID3DBlob> PixelShaderBlob;
	if (!D3D11Utils::CompileShaderFromSource(VertexShaderSource, "VSMain", "vs_5_0", VertexShaderBlob, "GizmoRenderer vertex shader")
		|| !D3D11Utils::CompileShaderFromSource(PixelShaderSource, "PSMain", "ps_5_0", PixelShaderBlob, "GizmoRenderer pixel shader"))
	{
		Resources.reset();
		return false;
	}

	if (FAILED(Device->CreateVertexShader(VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), nullptr, Resources->VertexShader.GetAddressOf()))
		|| FAILED(Device->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), nullptr, Resources->PixelShader.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC InputElements[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(FGizmoVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(FGizmoVertex, Normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(FGizmoVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
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

	if (!D3D11Utils::CreateDynamicConstantBuffer(Device, sizeof(FGizmoFrameConstants), Resources->FrameConstantBuffer)
		|| !D3D11Utils::CreateDynamicConstantBuffer(Device, sizeof(FGizmoObjectConstants), Resources->ObjectConstantBuffer))
	{
		Resources.reset();
		return false;
	}

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	RasterizerDesc.DepthClipEnable = TRUE;
	if (FAILED(Device->CreateRasterizerState(&RasterizerDesc, Resources->RasterizerState.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc = {};
	DepthStencilDesc.DepthEnable = FALSE;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	if (FAILED(Device->CreateDepthStencilState(&DepthStencilDesc, Resources->DepthStencilState.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	D3D11_BLEND_DESC BlendDesc = {};
	BlendDesc.RenderTarget[0].BlendEnable = TRUE;
	BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if (FAILED(Device->CreateBlendState(&BlendDesc, Resources->BlendState.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	return true;
}

void FGizmoRenderer::Shutdown()
{
	Resources.reset();
}

void FGizmoRenderer::Render(const FD3D11RHI& InRHI, const FCamera& InCamera, const std::vector<FGizmoDrawCommand>& InDrawCommands)
{
	if (!Resources || InDrawCommands.empty())
	{
		return;
	}

	ID3D11DeviceContext* DeviceContext = InRHI.GetDeviceContext();
	if (DeviceContext == nullptr)
	{
		return;
	}

	FGizmoFrameConstants FrameConstants;
	FrameConstants.ViewProjection = InCamera.GetViewMatrix() * InCamera.GetProjectionMatrix();
	if (!D3D11Utils::UpdateDynamicBuffer(DeviceContext, Resources->FrameConstantBuffer.Get(), FrameConstants))
	{
		return;
	}

	DeviceContext->IASetInputLayout(Resources->InputLayout.Get());
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->VSSetShader(Resources->VertexShader.Get(), nullptr, 0);
	DeviceContext->PSSetShader(Resources->PixelShader.Get(), nullptr, 0);

	ID3D11Buffer* FrameBuffer = Resources->FrameConstantBuffer.Get();
	DeviceContext->VSSetConstantBuffers(0, 1, &FrameBuffer);
	DeviceContext->PSSetConstantBuffers(0, 1, &FrameBuffer);

	DeviceContext->RSSetState(Resources->RasterizerState.Get());
	DeviceContext->OMSetDepthStencilState(Resources->DepthStencilState.Get(), 0);

	const float BlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	DeviceContext->OMSetBlendState(Resources->BlendState.Get(), BlendFactor, 0xFFFFFFFFu);

	for (const FGizmoDrawCommand& DrawCommand : InDrawCommands)
	{
		if (DrawCommand.VertexBuffer == nullptr || DrawCommand.IndexBuffer == nullptr || DrawCommand.IndexCount == 0)
		{
			continue;
		}

		FGizmoObjectConstants ObjectConstants;
		ObjectConstants.World = DrawCommand.WorldMatrix;
		ObjectConstants.Tint = DrawCommand.Tint;
		ObjectConstants.ColorBlend = DrawCommand.ColorBlend;
		if (!D3D11Utils::UpdateDynamicBuffer(DeviceContext, Resources->ObjectConstantBuffer.Get(), ObjectConstants))
		{
			continue;
		}

		ID3D11Buffer* ObjectBuffer = Resources->ObjectConstantBuffer.Get();
		DeviceContext->VSSetConstantBuffers(1, 1, &ObjectBuffer);
		DeviceContext->PSSetConstantBuffers(1, 1, &ObjectBuffer);

		constexpr UINT Stride = sizeof(FGizmoVertex);
		constexpr UINT Offset = 0;
		ID3D11Buffer* VertexBuffer = DrawCommand.VertexBuffer;
		DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
		DeviceContext->IASetIndexBuffer(DrawCommand.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		DeviceContext->DrawIndexed(DrawCommand.IndexCount, 0, 0);
	}
}
