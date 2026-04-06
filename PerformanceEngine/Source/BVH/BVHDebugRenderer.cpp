#include "BVH/BVHDebugRenderer.h"

#include <cstring>
#include <d3dcompiler.h>

#include "Camera/Camera.h"
#include "Graphics/D3D11/D3D11RHI.h"
#include "Math/Matrix.h"
#include "Scene/Scene.h"
#include "Scene/SceneTypes.h"
#include "StaticMesh/StaticMesh.h"

// -----------------------------------------------------------------------
// 한 AABB 박스의 12개 엣지 = 24개 라인 버텍스
// -----------------------------------------------------------------------
static constexpr int32 VerticesPerBox  = 24;
static constexpr int32 MaxBoxCount     = 4096;
static constexpr int32 MaxVertexCount  = VerticesPerBox * MaxBoxCount;

// -----------------------------------------------------------------------
// 내부 헬퍼
// -----------------------------------------------------------------------
namespace
{
	struct FLineVertex
	{
		float X, Y, Z;
	};

	struct alignas(16) FDebugFrameConstants
	{
		FMatrix ViewProjection;
	};

	struct alignas(16) FDebugColorConstants
	{
		float Color[4];
	};

	/** AABB 박스 엣지 24개 버텍스를 Out 버퍼에 추가합니다. */
	void AppendBoxLines(FVector InBoxMin, FVector InBoxMax, FLineVertex* InOut, int32& InOutCount)
	{
		if (InOutCount + VerticesPerBox > MaxVertexCount)
		{
			return;
		}

		const float MinX = InBoxMin.X, MinY = InBoxMin.Y, MinZ = InBoxMin.Z;
		const float MaxX = InBoxMax.X, MaxY = InBoxMax.Y, MaxZ = InBoxMax.Z;

		// 8개 코너
		const FLineVertex C[8] =
		{
			{ MinX, MinY, MinZ }, // 0
			{ MaxX, MinY, MinZ }, // 1
			{ MaxX, MaxY, MinZ }, // 2
			{ MinX, MaxY, MinZ }, // 3
			{ MinX, MinY, MaxZ }, // 4
			{ MaxX, MinY, MaxZ }, // 5
			{ MaxX, MaxY, MaxZ }, // 6
			{ MinX, MaxY, MaxZ }, // 7
		};

		// 12 엣지 (LINELIST이므로 각 엣지 = 버텍스 2개)
		static constexpr int32 Edges[12][2] =
		{
			// 아랫면
			{0,1}, {1,2}, {2,3}, {3,0},
			// 윗면
			{4,5}, {5,6}, {6,7}, {7,4},
			// 기둥
			{0,4}, {1,5}, {2,6}, {3,7},
		};

		FLineVertex* Dst = InOut + InOutCount;
		for (const auto& E : Edges)
		{
			*Dst++ = C[E[0]];
			*Dst++ = C[E[1]];
		}
		InOutCount += VerticesPerBox;
	}

	bool CompileShader(const char* InSource, const char* InEntry, const char* InTarget, TComPtr<ID3DBlob>& OutBlob)
	{
		UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
		Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
		TComPtr<ID3DBlob> ErrorBlob;
		return SUCCEEDED(D3DCompile(
			InSource, std::strlen(InSource), nullptr, nullptr, nullptr,
			InEntry, InTarget, Flags, 0,
			OutBlob.GetAddressOf(), ErrorBlob.GetAddressOf()));
	}

	template <typename T>
	bool UpdateDynamic(ID3D11DeviceContext* InDevcieContext, ID3D11Buffer* InBuf, const T& InData)
	{
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (FAILED(InDevcieContext->Map(InBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped))) return false;
		std::memcpy(Mapped.pData, &InData, sizeof(T));
		InDevcieContext->Unmap(InBuf, 0);
		return true;
	}

	bool UpdateDynamicRaw(ID3D11DeviceContext* InDevcieContext, ID3D11Buffer* InBuf, const void* InData, UINT InBytes)
	{
		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (FAILED(InDevcieContext->Map(InBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped))) return false;
		std::memcpy(Mapped.pData, InData, InBytes);
		InDevcieContext->Unmap(InBuf, 0);
		return true;
	}
}

// -----------------------------------------------------------------------
// FResources
// -----------------------------------------------------------------------
struct FBVHDebugRenderer::FResources
{
	TComPtr<ID3D11VertexShader>    VertexShader;
	TComPtr<ID3D11PixelShader>     PixelShader;
	TComPtr<ID3D11InputLayout>     InputLayout;
	TComPtr<ID3D11Buffer>          VertexBuffer;     // 동적, MaxVertexCount * sizeof(FLineVertex)
	TComPtr<ID3D11Buffer>          FrameCB;           // ViewProjection
	TComPtr<ID3D11Buffer>          ColorCB;           // RGBA 색상
	TComPtr<ID3D11RasterizerState> RasterizerState;
	TComPtr<ID3D11DepthStencilState> DepthStencilState;
};

// -----------------------------------------------------------------------
// 생성/소멸
// -----------------------------------------------------------------------
FBVHDebugRenderer::FBVHDebugRenderer()  = default;
FBVHDebugRenderer::~FBVHDebugRenderer() = default;

// -----------------------------------------------------------------------
// Initialize
// -----------------------------------------------------------------------
bool FBVHDebugRenderer::Initialize(FD3D11RHI& InRHI)
{
	ID3D11Device* Device = InRHI.GetDevice();
	if (!Device) return false;

	Resources = std::make_unique<FResources>();

	// ---- 셰이더 --------------------------------------------------------
	static constexpr char VSSrc[] = R"(
cbuffer FrameCB : register(b0)
{
    row_major float4x4 ViewProjection;
};

struct VSIn  { float3 Pos : POSITION; };
struct VSOut { float4 Pos : SV_POSITION; };

VSOut VSMain(VSIn In)
{
    VSOut Out;
    Out.Pos = mul(float4(In.Pos, 1.0f), ViewProjection);
    return Out;
}
)";

	static constexpr char PSSrc[] = R"(
cbuffer ColorCB : register(b1)
{
    float4 Color;
};

struct PSIn { float4 Pos : SV_POSITION; };

float4 PSMain(PSIn In) : SV_Target
{
    return Color;
}
)";

	TComPtr<ID3DBlob> VSBlob, PSBlob;
	if (!CompileShader(VSSrc, "VSMain", "vs_5_0", VSBlob) ||
	    !CompileShader(PSSrc, "PSMain", "ps_5_0", PSBlob))
	{
		Resources.reset();
		return false;
	}

	if (FAILED(Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, Resources->VertexShader.GetAddressOf())) ||
	    FAILED(Device->CreatePixelShader (PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, Resources->PixelShader.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	// ---- 입력 레이아웃 --------------------------------------------------
	const D3D11_INPUT_ELEMENT_DESC InputElem[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(Device->CreateInputLayout(
		InputElem, 1,
		VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(),
		Resources->InputLayout.GetAddressOf())))
	{
		Resources.reset();
		return false;
	}

	// ---- 동적 버텍스 버퍼 -----------------------------------------------
	{
		const D3D11_BUFFER_DESC VBDesc =
		{
			static_cast<UINT>(MaxVertexCount * sizeof(FLineVertex)),
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_VERTEX_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
			0, 0
		};
		if (FAILED(Device->CreateBuffer(&VBDesc, nullptr, Resources->VertexBuffer.GetAddressOf())))
		{
			Resources.reset();
			return false;
		}
	}

	// ---- 상수 버퍼 ------------------------------------------------------
	auto MakeCB = [&](UINT Size, TComPtr<ID3D11Buffer>& OutBuf) -> bool
	{
		const D3D11_BUFFER_DESC CBDesc =
		{
			Size, D3D11_USAGE_DYNAMIC,
			D3D11_BIND_CONSTANT_BUFFER,
			D3D11_CPU_ACCESS_WRITE, 0, 0
		};
		return SUCCEEDED(Device->CreateBuffer(&CBDesc, nullptr, OutBuf.GetAddressOf()));
	};

	if (!MakeCB(sizeof(FDebugFrameConstants), Resources->FrameCB) ||
	    !MakeCB(sizeof(FDebugColorConstants), Resources->ColorCB))
	{
		Resources.reset();
		return false;
	}

	// ---- 래스터라이저 (와이어프레임 라인용) --------------------------------
	{
		const D3D11_RASTERIZER_DESC RDesc =
		{
			D3D11_FILL_WIREFRAME,
			D3D11_CULL_NONE,
			FALSE, 0, 0.0f, 0.0f,
			TRUE, FALSE, FALSE, FALSE
		};
		if (FAILED(Device->CreateRasterizerState(&RDesc, Resources->RasterizerState.GetAddressOf())))
		{
			Resources.reset();
			return false;
		}
	}

	{
		D3D11_DEPTH_STENCIL_DESC DSDesc = {};
		DSDesc.DepthEnable    = TRUE;
		DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // depth 쓰기 끄기
		DSDesc.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
		DSDesc.StencilEnable  = FALSE;
		if (FAILED(Device->CreateDepthStencilState(&DSDesc, Resources->DepthStencilState.GetAddressOf())))
		{
			Resources.reset();
			return false;
		}
	}

	return true;
}

// -----------------------------------------------------------------------
// Shutdown
// -----------------------------------------------------------------------
void FBVHDebugRenderer::Shutdown()
{
	Resources.reset();
}

bool FBVHDebugRenderer::BeginRenderPass(
	const FD3D11RHI& InRHI,
	const FCamera& InCamera,
	ID3D11DeviceContext*& OutDeviceContext)
{
	if (!Resources)
	{
		OutDeviceContext = nullptr;
		return false;
	}

	OutDeviceContext = InRHI.GetDeviceContext();
	if (!OutDeviceContext)
	{
		return false;
	}

	FDebugFrameConstants FrameConst = {};
	FrameConst.ViewProjection = InCamera.GetViewMatrix() * InCamera.GetProjectionMatrix();
	if (!UpdateDynamic(OutDeviceContext, Resources->FrameCB.Get(), FrameConst))
	{
		return false;
	}

	const UINT Stride = sizeof(FLineVertex);
	const UINT Offset = 0;
	OutDeviceContext->IASetInputLayout(Resources->InputLayout.Get());
	OutDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	OutDeviceContext->IASetVertexBuffers(0, 1, Resources->VertexBuffer.GetAddressOf(), &Stride, &Offset);
	OutDeviceContext->VSSetShader(Resources->VertexShader.Get(), nullptr, 0);
	OutDeviceContext->PSSetShader(Resources->PixelShader.Get(), nullptr, 0);
	OutDeviceContext->RSSetState(Resources->RasterizerState.Get());
	OutDeviceContext->OMSetDepthStencilState(Resources->DepthStencilState.Get(), 0);

	ID3D11Buffer* CBs[] = { Resources->FrameCB.Get(), Resources->ColorCB.Get() };
	OutDeviceContext->VSSetConstantBuffers(0, 2, CBs);
	OutDeviceContext->PSSetConstantBuffers(0, 2, CBs);
	return true;
}

void FBVHDebugRenderer::EndRenderPass(ID3D11DeviceContext* InDeviceContext)
{
	if (!InDeviceContext)
	{
		return;
	}

	InDeviceContext->RSSetState(nullptr);
	InDeviceContext->OMSetDepthStencilState(nullptr, 0);
}

// -----------------------------------------------------------------------
// Render  — Scene의 모든 RenderItem BVH를 순회하며 렌더링
// -----------------------------------------------------------------------
void FBVHDebugRenderer::Render(
	const FD3D11RHI& InRHI,
	const FCamera& InCamera,
	const FScene& InScene)
{
	ID3D11DeviceContext* DeviceContext = nullptr;
	if (!BeginRenderPass(InRHI, InCamera, DeviceContext))
	{
		return;
	}

	// ViewProjection 업로드

	// 파이프라인 바인딩

	// Scene의 모든 RenderItem을 순회하며 BVH 렌더링
	for (const FRenderItem& RenderItem : InScene.GetRenderItems())
	{
		if (!RenderItem.StaticMesh) continue;

		const FBVHSpatialData* SpatialData =
			static_cast<const FBVHSpatialData*>(RenderItem.StaticMesh->GetSpatialData().get());
		if (!SpatialData || SpatialData->Nodes.empty()) continue;

		RenderNodes(DeviceContext, SpatialData->Nodes);
	}

	// 파이프라인 상태 리셋
	EndRenderPass(DeviceContext);
}

void FBVHDebugRenderer::RenderWorldNodeBoxes(
	const FD3D11RHI& InRHI,
	const FCamera& InCamera,
	const TArray<AABBNode>& InNodes)
{
	if (InNodes.empty())
	{
		return;
	}

	ID3D11DeviceContext* DeviceContext = nullptr;
	if (!BeginRenderPass(InRHI, InCamera, DeviceContext))
	{
		return;
	}

	RenderWorldNodeBoxes(DeviceContext, InNodes);
	EndRenderPass(DeviceContext);
}

// -----------------------------------------------------------------------
// RenderNodes  — 단일 BVH 노드 배열을 깊이별 색상으로 Draw
// -----------------------------------------------------------------------
void FBVHDebugRenderer::RenderNodes(
	ID3D11DeviceContext* InDeviceContext,
	const TArray<FBVHNode>& InNodes)
{
	// 깊이별 색상 팔레트
	static constexpr float DepthColors[][4] =
	{
		{ 1.0f, 0.0f, 0.0f, 1.0f }, // 0: 빨강 (루트)
		{ 1.0f, 0.5f, 0.0f, 1.0f }, // 1: 주황
		{ 1.0f, 1.0f, 0.0f, 1.0f }, // 2: 노랑
		{ 0.0f, 1.0f, 0.0f, 1.0f }, // 3: 초록
		{ 0.0f, 1.0f, 1.0f, 1.0f }, // 4: 청록
		{ 0.0f, 0.0f, 1.0f, 1.0f }, // 5: 파랑
		{ 0.5f, 0.0f, 1.0f, 1.0f }, // 6: 보라
		{ 1.0f, 0.0f, 0.5f, 1.0f }, // 7: 분홍
	};
	static constexpr int32 PaletteSize = static_cast<int32>(_countof(DepthColors));
	static constexpr int32 MaxDepth    = 32;

	// DFS로 깊이별 노드 인덱스 수집
	TArray<int32> NodesByDepth[MaxDepth];

	struct FStackEntry { int32 NodeIndex; int32 Depth; };
	TArray<FStackEntry> Stack;
	Stack.push_back({ 0, 0 });

	while (!Stack.empty())
	{
		FStackEntry Entry = Stack.back();
		Stack.pop_back();

		if (Entry.NodeIndex < 0 || Entry.NodeIndex >= static_cast<int32>(InNodes.size())) continue;

		const FBVHNode& Node = InNodes[Entry.NodeIndex];

		if (Entry.Depth < MaxDepth)
		{
			NodesByDepth[Entry.Depth].push_back(Entry.NodeIndex);
		}

		if (Node.PrimitiveCount == 0) // 내부 노드
		{
			Stack.push_back({ Node.LeftFirst,     Entry.Depth + 1 });
			Stack.push_back({ Node.LeftFirst + 1, Entry.Depth + 1 });
		}
	}

	// 깊이별 Draw
	static FLineVertex VertexScratch[MaxVertexCount];

	for (int32 Depth = 0; Depth < MaxDepth; ++Depth)
	{
		if (NodesByDepth[Depth].empty()) continue;

		int32 VertexCount = 0;
		for (int32 NodeIdx : NodesByDepth[Depth])
		{
			AppendBoxLines(InNodes[NodeIdx].BoundMin, InNodes[NodeIdx].BoundMax, VertexScratch, VertexCount);
		}
		if (VertexCount == 0) continue;

		if (!UpdateDynamicRaw(InDeviceContext, Resources->VertexBuffer.Get(),
		                      VertexScratch, static_cast<UINT>(VertexCount * sizeof(FLineVertex))))
		{
			continue;
		}

		FDebugColorConstants ColorConst;
		const float* C = DepthColors[Depth % PaletteSize];
		ColorConst.Color[0] = C[0];
		ColorConst.Color[1] = C[1];
		ColorConst.Color[2] = C[2];
		ColorConst.Color[3] = C[3];
		UpdateDynamic(InDeviceContext, Resources->ColorCB.Get(), ColorConst);

		InDeviceContext->Draw(static_cast<UINT>(VertexCount), 0);
	}
}

void FBVHDebugRenderer::RenderWorldNodeBoxes(
	ID3D11DeviceContext* InDeviceContext,
	const TArray<AABBNode>& InNodes)
{
	static FLineVertex VertexScratch[MaxVertexCount];

	int32 VertexCount = 0;
	for (const AABBNode& Node : InNodes)
	{
		AppendBoxLines(Node.Min, Node.Max, VertexScratch, VertexCount);
	}

	if (VertexCount == 0)
	{
		return;
	}

	if (!UpdateDynamicRaw(
		InDeviceContext,
		Resources->VertexBuffer.Get(),
		VertexScratch,
		static_cast<UINT>(VertexCount * sizeof(FLineVertex))))
	{
		return;
	}

	FDebugColorConstants ColorConst = {};
	ColorConst.Color[0] = 0.15f;
	ColorConst.Color[1] = 1.0f;
	ColorConst.Color[2] = 0.2f;
	ColorConst.Color[3] = 1.0f;
	UpdateDynamic(InDeviceContext, Resources->ColorCB.Get(), ColorConst);

	InDeviceContext->Draw(static_cast<UINT>(VertexCount), 0);
}
