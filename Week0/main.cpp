#include <windows.h>
#include "dx11math.h"
struct FVector3;

// D3D Library Linking
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

// D3D headers Includes
#include <d3d11.h>
#include <d3dcompiler.h>

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"


// 1. Define the triangle vertices
struct FVertexSimple
{
	float x, y, z;    // Position
	float r, g, b, a; // Color
};

#include "Sphere.h"

FVertexSimple triangle_vertices[] =
{
	{  0.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top vertex (red)
	{  1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right vertex (green)
	{ -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f }  // Bottom-left vertex (blue)
};

FVertexSimple cube_vertices[] =
{
	// Front face (Z+)
	{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Bottom-left (red)
	{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-left (yellow)
	{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)
	{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-left (yellow)
	{  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-right (blue)
	{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)

	// Back face (Z-)
	{ -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f, 1.0f }, // Bottom-left (cyan)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // Bottom-right (magenta)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // Bottom-right (magenta)
	{  0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-right (yellow)

	// Left face (X-)
	{ -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // Bottom-left (purple)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
	{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-right (yellow)
	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)

	// Right face (X+)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f }, // Bottom-left (orange)
	{  0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 1.0f }, // Bottom-right (gray)
	{  0.5f,  0.5f, -0.5f,  0.5f, 0.0f, 0.5f, 1.0f }, // Top-left (purple)
	{  0.5f,  0.5f, -0.5f,  0.5f, 0.0f, 0.5f, 1.0f }, // Top-left (purple)
	{  0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 1.0f }, // Bottom-right (gray)
	{  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 0.5f, 1.0f }, // Top-right (dark blue)

	// Top face (Y+)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.5f, 1.0f }, // Bottom-left (light green)
	{ -0.5f,  0.5f,  0.5f,  0.0f, 0.5f, 1.0f, 1.0f }, // Top-left (cyan)
	{  0.5f,  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 1.0f }, // Bottom-right (white)
	{ -0.5f,  0.5f,  0.5f,  0.0f, 0.5f, 1.0f, 1.0f }, // Top-left (cyan)
	{  0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.0f, 1.0f }, // Top-right (brown)
	{  0.5f,  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 1.0f }, // Bottom-right (white)

	// Bottom face (Y-)
	{ -0.5f, -0.5f, -0.5f,  0.5f, 0.5f, 0.0f, 1.0f }, // Bottom-left (brown)
	{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top-left (red)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.5f, 1.0f }, // Bottom-right (purple)
	{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top-left (red)
	{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Top-right (green)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.5f, 1.0f }, // Bottom-right (purple)
};


class URenderer
{
public:
	// Direct3D 11 장치와 장치 컨텍스트 및 스왑 체인을 관리하기 위한 포인터들
	ID3D11Device* Device = nullptr; // GPU와 통신하기 위한 Direct3D 장치
	ID3D11DeviceContext* DeviceContext = nullptr; // GPU 명령 실행을 담당하는 컨텍스트
	IDXGISwapChain* SwapChain = nullptr; // 프레임 버퍼를 교체하는 데 사용되는 스왑 체인

	// 렌더링에 필요한 리소스 및 상태를 관리하기 위한 변수들
	ID3D11Texture2D* FrameBuffer = nullptr; // 화면 출력용 텍스쳐
	ID3D11RenderTargetView* FrameBufferRTV = nullptr; // 텍스처를 렌더 타겟으로 사용하는 뷰
	ID3D11RasterizerState* RasterizerState = nullptr; // 래스터라이저 상태(컬링, 채우기 모드 등 정의)
	ID3D11Buffer* ConstantBuffer = nullptr; // 쉐이더에 데이터를 전달하기 위한 상수 버퍼

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f }; // 화면을 초기화할 때 사용할 색상 (RGBA)
	D3D11_VIEWPORT ViewportInfo; // 렌더링 영역을 정의하는 뷰포트 정보


	//Shader
	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
	unsigned int Stride;

public:
	// 렌더러 초기화 함수
	void Create(HWND hWindow)
	{
		// Direct3D 장치 및 스왑 체인 생성
		CreateDeviceAndSwapChain(hWindow);

		CreateFrameBuffer();

		CreateRasterizerState();

		// 깊이 스텐실 버퍼 및 블렌드 상태는 이 코드에서는 다루지 않음
	}

	void CreateDeviceAndSwapChain(HWND hWindow)
	{
		// 지원하는 Direct3D 기능 레벨 정의
		D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

		// 스왑 체인 설정 구조체 초기화
		DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
		swapchaindesc.BufferDesc.Width = 0; // 창 크기에 맞게 자동으로 설정
		swapchaindesc.BufferDesc.Height = 0; // 창 크기에 맞게 자동으로 설정
		swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 색상 포맷
		swapchaindesc.SampleDesc.Count = 1; // 멀티 샘플링 비활성화
		swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더 타겟으로 사용
		swapchaindesc.BufferCount = 2; // 더블 버퍼링
		swapchaindesc.OutputWindow = hWindow; // 렌더링할 창 핸들
		swapchaindesc.Windowed = TRUE; // 창 모드
		swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 방식

		// Direct3D 장치와 스왑 체인 생성
		D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
			featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
			&swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

		// 생성된 스왑 체인의 정보 가져오기
		SwapChain->GetDesc(&swapchaindesc);

		// 뷰포트 정보 설정
		ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
	}

	// Direct3D 장치 및 스왑 체인 해제
	void ReleaseDeviceAndSwapChain()
	{
		if (DeviceContext)
		{
			DeviceContext->Flush(); // 남아있는 GPU 명령 실행
		}

		if (SwapChain)
		{
			SwapChain->Release();
			SwapChain = nullptr;
		}

		if (Device)
		{
			Device->Release();
			Device = nullptr;
		}

		if (DeviceContext)
		{
			DeviceContext->Release();
			DeviceContext = nullptr;
		}
	}


	void CreateFrameBuffer()
	{
		// 스왑체인에서 백 버퍼 텍스처 가져오기 
		SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

		// 렌더 타겟 뷰 생성
		D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
		framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // 색상 포맷
		framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // 2D 텍스처

		Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
	}

	// 프레임 버퍼 해제
	void ReleaseFrameBuffer()
	{
		if (FrameBuffer)
		{
			FrameBuffer->Release();
			FrameBuffer = nullptr;
		}

		if (FrameBufferRTV)
		{
			FrameBufferRTV->Release();
			FrameBufferRTV = nullptr;
		}
	}

	// 래스터라이저 상태 생성
	void CreateRasterizerState()
	{
		D3D11_RASTERIZER_DESC rasterizerdesc = {};
		rasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
		rasterizerdesc.CullMode = D3D11_CULL_BACK; // 백 페이스 컬링

		Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
	}

	// 래스터라이저 상태 해제
	void ReleaseRasterizerState()
	{
		if (RasterizerState)
		{
			RasterizerState->Release();
			RasterizerState = nullptr;
		}
	}

	// 렌더러에 사용된 모든 리소스를 해제
	void Release()
	{
		RasterizerState->Release();

		// 렌더 타겟을 초기화
		DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

		ReleaseFrameBuffer();
		ReleaseDeviceAndSwapChain();
	}

	// 스왑 체인의 백 버퍼와 프론트 버퍼를 교체하여 화면에 출력
	void SwapBuffer()
	{
		SwapChain->Present(1, 0); // 1: VSync 활성화
	}

	void CreateShader()
	{
		ID3DBlob* vertexshaderCSO;
		ID3DBlob* pixelshaderCSO;

		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &SimpleVertexShader);

		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);

		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);

		Stride = sizeof(FVertexSimple);

		vertexshaderCSO->Release();
		pixelshaderCSO->Release();
	}

	void ReleaseShader()
	{
		if (SimpleInputLayout)
		{
			SimpleInputLayout->Release();
			SimpleInputLayout = nullptr;
		}

		if (SimplePixelShader)
		{
			SimplePixelShader->Release();
			SimplePixelShader = nullptr;
		}

		if (SimpleVertexShader)
		{
			SimpleVertexShader->Release();
			SimpleVertexShader = nullptr;
		}
	}

	void Prepare()
	{
		DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

		DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		DeviceContext->RSSetViewports(1, &ViewportInfo);
		DeviceContext->RSSetState(RasterizerState);

		DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
		DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}

	void PrepareShader()
	{
		DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(SimpleInputLayout);

		if (ConstantBuffer)
		{
			DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
		}
	}

	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
	{
		UINT offset = 0;
		DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);

		DeviceContext->Draw(numVertices, 0);
	}

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
	{
		// 2. Create a vertex buffer
		D3D11_BUFFER_DESC vertexbufferdesc = {};
		vertexbufferdesc.ByteWidth = byteWidth;
		vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated 
		vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

		ID3D11Buffer* vertexBuffer;

		Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

		return vertexBuffer;
	}

	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
	{
		vertexBuffer->Release();
	}

	struct FConstants
	{
		FVector3 Offset; // x: X 위치, y: Y 위치, z: 크기(Radius)
		float Angle;     // Pad를 Angle로 변경하여 회전값 전달
	};

	void CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC constantbufferdesc = {};
		constantbufferdesc.ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
		constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
		constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
	}

	void ReleaseConstantBuffer()
	{
		if (ConstantBuffer)
		{
			ConstantBuffer->Release();
			ConstantBuffer = nullptr;
		}
	}

	// UpdateConstant 함수가 Angle도 받도록 수정
	void UpdateConstant(FVector3 Offset, float Angle)
	{
		if (ConstantBuffer)
		{
			D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
			DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);
			FConstants* constants = (FConstants*)constantbufferMSR.pData;

			constants->Offset = Offset;
			constants->Angle = Angle; // 추가된 부분

			DeviceContext->Unmap(ConstantBuffer, 0);
		}
	}
};


FVector3 gravity;

class UPrimitive
{
public:
	virtual void Update(float t) = 0;
	virtual void UpdateRenderer(URenderer& renderer) = 0;
	virtual void HandleCollision(UPrimitive* other) = 0;
	virtual void D(const FVector3& v) = 0;
	virtual void ApplyAttraction(const FVector3& point, float strength) = 0;
	virtual ID3D11Buffer* GetVertexBuffer() = 0;
	virtual ~UPrimitive() {}
	virtual void Render(URenderer& renderer) = 0;
};

class UBall : public UPrimitive
{
private:
	static constexpr float MaxSpeed = 0.01f;
	static constexpr float MaxSpeedBothThrusters = 0.0015f; // 둘 다 누를 때 더 낮은 상한
	static constexpr float MaxAttractionForce = 0.001f;

	static ID3D11Buffer* SphereVertexBuffer;
	static UINT NumVerticesSphere;

	static ID3D11Buffer* CubeVertexBuffer;
	static UINT NumVerticesCube;
	static int TotalNumBalls;

	FVector3 Location{};
	FVector3 Velocity{};
	float Radius{};
	float Mass{};


	float Index{};

	float Angle = 0.0f;           // 현재 회전 각도 (라디안)
	float AngularVelocity = 0.0f; // 회전하는 속도 (각속도)

	int NumHits{};
public:
	static bool bApplyGravity;
	static bool bApplyAttraction;

	static int GetTotalNumBalls() { return TotalNumBalls; }
	static void InitializeBuffer(URenderer& renderer)
	{
		SphereVertexBuffer = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
		NumVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);

		CubeVertexBuffer = renderer.CreateVertexBuffer(cube_vertices, sizeof(cube_vertices));
		NumVerticesCube = sizeof(cube_vertices) / sizeof(FVertexSimple);
	}

	static void ReleaseBuffer(URenderer& renderer)
	{
		renderer.ReleaseVertexBuffer(SphereVertexBuffer);
		SphereVertexBuffer = nullptr;

		renderer.ReleaseVertexBuffer(CubeVertexBuffer);
		CubeVertexBuffer = nullptr;
	}

	virtual ID3D11Buffer* GetVertexBuffer() override
	{
		return SphereVertexBuffer;
	}

	FVector3 GetVelocity() const { return Velocity; }
	void SetVelocity(FVector3 val) { Velocity = val; }

	UBall()
	{
		TotalNumBalls++;
		Radius = rand() % 100 * 0.001f + 0.01f;
		Mass = Radius * Radius;
		Location.x = ((float)(rand() % 200 - 100)) * 0.01f;
		Location.y = ((float)(rand() % 200 - 100)) * 0.01f;
		float initialSpeed = 0.0005f;
		float randomAngle = (float)(rand() % 360) * (3.141592f / 180.0f);

		Velocity.x = cosf(randomAngle) * initialSpeed;
		Velocity.y = sinf(randomAngle) * initialSpeed;
		Velocity.z = 0.0f;
	}

	~UBall()
	{

		TotalNumBalls--;
	}


	void Move(float t)
	{
		Location.x += Velocity.x * t;
		Location.y += Velocity.y * t;

		if (Location.x < -1.0f + Radius)
		{
			Location.x = -1.0f + Radius;
			Velocity.x *= -0.5f;
		}
		else if (Location.x > 1.0f - Radius)
		{
			Location.x = 1.0f - Radius;
			Velocity.x *= -0.5f;
		}
		if (Location.y < -1.0f + Radius)
		{
			Location.y = -1.0f + Radius;
			Velocity.y *= -0.8f;
		}
	}

	void Render(URenderer& renderer) override
	{
		// 1. 메인 공(구체) 렌더링
		FVector3 sphereTransform = { this->Location.x, this->Location.y, this->Radius };
		renderer.UpdateConstant(sphereTransform, this->Angle);
		renderer.RenderPrimitive(SphereVertexBuffer, NumVerticesSphere);

		// --- 시각적 장식(추진체) 추가 ---

		// 2. 사각형의 크기를 키웁니다 (기존 0.4f -> 0.8f)
		float thrusterScale = this->Radius * 0.8f;

		// 3. 겹침 방지를 위해 약간의 여백(Padding)을 줍니다.
		float padding = this->Radius * 0.1f; // 원 반경의 10%만큼 틈새를 만듦

		// 4. 공 중심에서 날개가 떨어져 있을 거리 = 원의 반지름 + 여백 + 사각형 절반
		float offsetDist = this->Radius + padding + (thrusterScale * 0.5f);

		// 5. 현재 회전 각도(Angle)를 기준으로 로컬 Right 방향(양옆 방향) 계산
		float rightX = cosf(this->Angle);
		float rightY = sinf(this->Angle);

		// 왼쪽 날개 렌더링 (중심에서 -Right 방향으로 이동)
		FVector3 leftPos = {
			this->Location.x - rightX * offsetDist,
			this->Location.y - rightY * offsetDist,
			thrusterScale
		};
		renderer.UpdateConstant(leftPos, this->Angle);
		renderer.RenderPrimitive(CubeVertexBuffer, NumVerticesCube);

		// 오른쪽 날개 렌더링 (중심에서 +Right 방향으로 이동)
		FVector3 rightPos = {
			this->Location.x + rightX * offsetDist,
			this->Location.y + rightY * offsetDist,
			thrusterScale
		};
		renderer.UpdateConstant(rightPos, this->Angle);
		renderer.RenderPrimitive(CubeVertexBuffer, NumVerticesCube);
	}

	// 추진체 가동 함수
// 추진체 가동 함수
// 추진체 가동 함수 (순간적인 충격량 방식)
// 추진체 가동 함수 (순간적인 충격량 방식)
	void ApplyThrust(bool bLeftThruster, bool bRightThruster, float deltaTime)
	{
		float thrustPower = 0.0001f;
		float torquePower = 0.0001f;

		FVector3 forwardDir(-sinf(Angle), cosf(Angle), 0.0f);
		FVector3 rightDir(cosf(Angle), sinf(Angle), 0.0f);

		if (bLeftThruster) // A키 (왼쪽 추진체 탭!)
		{
			FVector3 pushDir = forwardDir * 0.8f + rightDir * 0.5f;
			Velocity = Velocity + (pushDir * thrustPower);

			// [추가] 이미 오른쪽(도려는 방향)으로 기울어진 상태라면 회전력을 줄임
			float modifier = 1.0f;
			if (sinf(Angle) < 0.0f)
			{
				// 똑바로 서 있을 때(cos=1)는 100%, 눕거나 뒤집힐수록 최소 20%까지 힘을 제한
				float currentCos = cosf(Angle);
				modifier = currentCos < 0.2f ? 0.2f : currentCos;
			}
			AngularVelocity -= torquePower * modifier;
		}

		if (bRightThruster) // D키 (오른쪽 추진체 탭!)
		{
			FVector3 pushDir = forwardDir * 0.8f - rightDir * 0.5f;
			Velocity = Velocity + (pushDir * thrustPower);

			// [추가] 이미 왼쪽(도려는 방향)으로 기울어진 상태라면 회전력을 줄임
			float modifier = 1.0f;
			if (sinf(Angle) > 0.0f)
			{
				float currentCos = cosf(Angle);
				modifier = currentCos < 0.2f ? 0.2f : currentCos;
			}
			AngularVelocity += torquePower * modifier;
		}

		if (bLeftThruster && bRightThruster)
		{
			ClampSpeed2(MaxSpeedBothThrusters);
		}
		else
		{
			ClampSpeed2(MaxSpeed);
		}
	}

	void Update(float t) override
	{
		// 1. 중력도 스케일에 맞춰 낮춤 (천천히 가속되며 떨어짐)
		Velocity.y -= 0.000002f * t;

		// 2. 우주의 마찰력 (조작하지 않을 때 미끄러지듯 감속)
		Velocity.x *= 0.995f;
		Velocity.y *= 0.995f;

		// 회전 마찰력 
		AngularVelocity *= 0.95f;

		// [추가] 3. 오뚝이(Auto-Balance) 기능
		// sinf(Angle)의 반대 방향으로 아주 미세한 힘을 가해 항상 0도(위쪽)를 향하도록 유도합니다.
		float autoBalancePower = 0.000001f; // 복원력 강도 (너무 세면 덜렁거리니 미세하게 조절)
		AngularVelocity += -sinf(Angle) * autoBalancePower * t;

		// 4. 각도 업데이트
		Angle += AngularVelocity * t;

		ClampSpeed();
		Move(t);
	}

	void UpdateRenderer(URenderer& renderer) override
	{
		FVector3 transform = { this->Location.x, this->Location.y, this->Radius };
		// 이제 위치/크기 정보와 함께 Angle(회전각)도 전달합니다.
		renderer.UpdateConstant(transform, this->Angle);
	}
	void HandleCollision(UPrimitive* other)override
	{
		UBall* otherBall = static_cast<UBall*>(other);

		float deltaX = otherBall->Location.x - Location.x;
		float deltaY = otherBall->Location.y - Location.y;

		float distanceSquared = deltaX * deltaX + deltaY * deltaY;

		float radiusSum = Radius + otherBall->Radius;
		float radiusSumSquared = radiusSum * radiusSum;

		if (distanceSquared <= radiusSumSquared)
		{
			float distance = sqrtf(distanceSquared);
			distance = distance < 0.0001f ? 0.0001f : distance;
			float normalX = deltaX / distance;
			float normalY = deltaY / distance;

			float relativeVelocityX = otherBall->Velocity.x - Velocity.x;
			float relativeVelocityY = otherBall->Velocity.y - Velocity.y;

			float velocityAlongNormal = relativeVelocityX * normalX + relativeVelocityY * normalY;



			float overlap = radiusSum - distance;
			float totalInverseMass = (1.0f / Mass) + (1.0f / otherBall->Mass);

			float percent = 0.2f;
			float correctionMagnitude = (overlap / totalInverseMass) * percent;
			float moveX = correctionMagnitude * normalX;
			float moveY = correctionMagnitude * normalY;

			this->Location.x -= moveX * (1.0f / Mass);
			this->Location.y -= moveY * (1.0f / Mass);
			otherBall->Location.x += moveX * (1.0f / otherBall->Mass);
			otherBall->Location.y += moveY * (1.0f / otherBall->Mass);

			if (velocityAlongNormal > 0)
			{
				return;
			}

			float elasticity = 1.0f;

			float impulseMagnitude = -(1 + elasticity) * velocityAlongNormal;
			impulseMagnitude /= (1 / Mass) + (1 / otherBall->Mass);

			FVector3 impulseThis = { -(impulseMagnitude / Mass) * normalX, -(impulseMagnitude / Mass) * normalY, 0.0f };
			FVector3 impulseOther = { (impulseMagnitude / otherBall->Mass) * normalX, (impulseMagnitude / otherBall->Mass) * normalY, 0.0f };

			D(impulseThis);
			other->D(impulseOther);
		}
	}

	virtual void D(const FVector3& v)override
	{
		this->Velocity.x += v.x;
		this->Velocity.y += v.y;
	}

	void ClampSpeed()
	{
		float speedSquared = Velocity.x * Velocity.x + Velocity.y * Velocity.y;
		if (speedSquared > MaxSpeed * MaxSpeed)
		{
			float speed = sqrtf(speedSquared);
			Velocity.x = (Velocity.x / speed) * MaxSpeed;
			Velocity.y = (Velocity.y / speed) * MaxSpeed;
		}
	}

	void ClampSpeed2(float maxSpeed)
	{
		float speedSquared = Velocity.x * Velocity.x + Velocity.y * Velocity.y;
		if (speedSquared > maxSpeed * maxSpeed)
		{
			float speed = sqrtf(speedSquared);
			Velocity.x = (Velocity.x / speed) * maxSpeed;
			Velocity.y = (Velocity.y / speed) * maxSpeed;
		}
	}

	void ApplyAttraction(const FVector3& point, float strength)
	{
		if (bApplyAttraction == false)
		{
			return;
		}

		float deltaX = point.x - Location.x;
		float deltaY = point.y - Location.y;

		float distanceSquared = deltaX * deltaX + deltaY * deltaY + 0.001f;
		distanceSquared = distanceSquared < 0.0001f ? 0.0001f : distanceSquared;

		float distance = sqrtf(distanceSquared);
		float forceMagnitude = strength / distanceSquared;
		forceMagnitude = forceMagnitude > MaxAttractionForce ? MaxAttractionForce : forceMagnitude;
		float forceX = (deltaX / distance) * forceMagnitude;
		float forceY = (deltaY / distance) * forceMagnitude;

		FVector3 force = { forceX, forceY, 0.0f };
		D(force);
	}
};

ID3D11Buffer* UBall::SphereVertexBuffer = nullptr;
UINT UBall::NumVerticesSphere = 0;
ID3D11Buffer* UBall::CubeVertexBuffer = nullptr;
UINT UBall::NumVerticesCube = 0;
int UBall::TotalNumBalls = 0;
bool UBall::bApplyGravity = true;
bool UBall::bApplyAttraction = false;

class FPrimitivesManager
{
private:
	UPrimitive** PrimitiveList = nullptr;
	int fillPrimitiveCount = 0;
	int capacity = 100;

public:
	FPrimitivesManager()
	{
		PrimitiveList = new UPrimitive * [capacity];
	}

	void addElement(UPrimitive* element)
	{
		if (UBall::GetTotalNumBalls() >= capacity)
		{
			// Resize
			int newCapacity = capacity * 2;
			UPrimitive** newList = new UPrimitive * [newCapacity];
			memcpy(newList, PrimitiveList, fillPrimitiveCount * sizeof(UPrimitive*));

			delete[] PrimitiveList;
			PrimitiveList = newList;
			capacity = newCapacity;
		}

		PrimitiveList[fillPrimitiveCount] = element;
		fillPrimitiveCount++;
	}

	void RemoveRandomElement()
	{
		if (fillPrimitiveCount == 0)
		{
			return;
		}

		int indexToRemove = rand() % fillPrimitiveCount;
		delete PrimitiveList[indexToRemove];

		if (indexToRemove != fillPrimitiveCount - 1)
		{
			PrimitiveList[indexToRemove] = PrimitiveList[fillPrimitiveCount - 1];
		}

		fillPrimitiveCount--;
		PrimitiveList[fillPrimitiveCount] = nullptr;
	}


	~FPrimitivesManager()
	{
		for (int i = 0; i < fillPrimitiveCount; ++i)
		{
			if (PrimitiveList[i] != nullptr)
			{
				delete PrimitiveList[i];
			}
		}
		delete[] PrimitiveList;
	}

	UPrimitive* GetPrimitive(int index)
	{
		if (index >= 0 && index < fillPrimitiveCount) return PrimitiveList[index];
		return nullptr;
	}

	void SyncBallCountWithUI(int& targetBallNum)
	{
		while (UBall::GetTotalNumBalls() < targetBallNum)
		{
			UBall* newBall = new UBall();
			addElement(newBall);
		}

		while (UBall::GetTotalNumBalls() > targetBallNum)
		{
			RemoveRandomElement();
		}

		targetBallNum = UBall::GetTotalNumBalls();
	}

	void Update(const float deltaTime, const FVector3& ExternalForcePos)
	{
		for (int i = 0; i < fillPrimitiveCount; ++i)
		{
			UPrimitive* primitive = PrimitiveList[i];
			if (primitive != nullptr)
			{
				primitive->Update(deltaTime);
			}

			primitive->ApplyAttraction(ExternalForcePos, 0.000001f);

		}
		for (int i = 0; i < fillPrimitiveCount; ++i)
		{
			for (int j = i + 1; j < fillPrimitiveCount; ++j)
			{
				if (PrimitiveList[i] != nullptr && PrimitiveList[j] != nullptr)
				{
					PrimitiveList[i]->HandleCollision(PrimitiveList[j]);
				}
			}
		}
	}

	void Render(URenderer& renderer)
	{
		for (int i = 0; i < fillPrimitiveCount; ++i)
		{
			UPrimitive* primitive = PrimitiveList[i];
			if (primitive != nullptr)
			{
				primitive->Render(renderer); // 객체에게 렌더링을 위임!
			}
		}
	}
};



extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";

	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&wndclass);

	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInstance, nullptr);

	URenderer renderer;
	renderer.Create(hWnd);
	renderer.CreateShader();

	renderer.CreateConstantBuffer();

	// ImGui Setup
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);

	UINT numVerticesTriangle = sizeof(triangle_vertices) / sizeof(FVertexSimple);
	UINT numVerticesCube = sizeof(cube_vertices) / sizeof(FVertexSimple);

	float scaleMod = 0.1f;

	/*for (UINT i = 0; i < numVerticesSphere; ++i)
	{
		sphere_vertices[i].x *= scaleMod;
		sphere_vertices[i].y *= scaleMod;
		sphere_vertices[i].z *= scaleMod;
	}*/

	ID3D11Buffer* vertexBufferTriangle = renderer.CreateVertexBuffer(triangle_vertices, sizeof(triangle_vertices));


	/*for (UINT i = 0; i < numVerticesSphere; ++i)
	{
		sphere_vertices[i].x *= scaleMod;
		sphere_vertices[i].y *= scaleMod;
		sphere_vertices[i].z *= scaleMod;
	}*/

	bool bIsExit = false;

	enum ETypePrimitive
	{
		EPT_Triangle,
		EPT_Cube,
		EPT_Sphere,
		EPT_Max,
	};

	ETypePrimitive typePrimitive = EPT_Sphere;
	FVector3	offset(0.0f);
	FVector3 velocity(0.0f);

	const float leftBorder = -1.0f;
	const float rightBorder = 1.0f;
	const float topBorder = -1.0f;
	const float bottomBorder = 1.0f;
	const float sphereRadius = 1.0f;

	bool bBoundBallToScreen = true;
	bool bPinballMovement = true;

	velocity.x = ((float)(rand() % 100 - 50)) * 0.001f;
	velocity.y = ((float)(rand() % 100 - 50)) * 0.001f;

	const int targetFPS = 60;
	const double targetFrameTime = 1000.0 / targetFPS;
	int targetBallNum = 1;

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER startTime, endTime;
	double elapsedTime = 0.0;

	POINT mousePos;
	FVector3 mouseWorldPos;

	FPrimitivesManager primitivesManager;

	UINT numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);
	for (UINT i = 0; i < numVerticesSphere; ++i)
	{
		// 로컬 Y좌표가 0.6 이상인 부분을 빨간색으로 변경 (구체 반경 기준에 맞춰 0.5~0.8 등 수치 조절)
		if (sphere_vertices[i].y > 0.6f)
		{
			sphere_vertices[i].r = 1.0f; // Red
			sphere_vertices[i].g = 0.0f;
			sphere_vertices[i].b = 0.0f;
		}
	}

	UBall::InitializeBuffer(renderer);

	bool bPrevLeftPressed = false;
	bool bPrevRightPressed = false;
	// Main Loop 
	while (bIsExit == false)
	{
		QueryPerformanceCounter(&startTime);
		MSG msg;

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			//키 입력 메시지 번역
			TranslateMessage(&msg);
			// 메시지를 적절한 윈도우 프로시저에 전달, 메시지가 위에서 등록한 WndProc로 전달됨
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
			if (msg.message == WM_KEYDOWN)
			{
				if (msg.wParam == 'Q') bIsExit = true;
			}
		}
			// 키가 눌려있다면 추진체 가동!
		UBall* targetBall = static_cast<UBall*>(primitivesManager.GetPrimitive(0));
		if (targetBall != nullptr)
		{
			// 1. 현재 프레임의 키 상태 확인
			bool bCurrentLeftPressed = (GetAsyncKeyState('A') & 0x8000) != 0;
			bool bCurrentRightPressed = (GetAsyncKeyState('D') & 0x8000) != 0;

			// 3. 꾹 누르는 게 아니라, 새로 눌렸을 때(Tap)만 추진체 가동!
			if (bCurrentLeftPressed || bCurrentRightPressed)
			{
				targetBall->ApplyThrust(bCurrentLeftPressed, bCurrentRightPressed, elapsedTime);
			}
		}

			//if (targetBall && (msg.wParam == 'A' || msg.wParam == 'D'))
			//{
			//	float angle = 0.10f; // 회전 속도 조절
			//	if (msg.wParam == 'D') angle = -angle;

			//	float cosA = cosf(angle);
			//	float sinA = sinf(angle);

			//	FVector3 currentVel = targetBall->GetVelocity(); // UBall에 GetVelocity() 함수 추가 필요
			//	float newX = currentVel.x * cosA - currentVel.y * sinA;
			//	float newY = currentVel.x * sinA + currentVel.y * cosA;

			//	targetBall->SetVelocity(FVector3(newX, newY, 0.0f)); // UBall에 SetVelocity() 함수 추가 필요
			//}

		renderer.Prepare();
		renderer.PrepareShader();

		GetCursorPos(&mousePos);
		ScreenToClient(hWnd, &mousePos);

		mouseWorldPos.x = (mousePos.x / 512.0f) - 1.0f;
		mouseWorldPos.y = -((mousePos.y / 512.0f) - 1.0f);

		primitivesManager.SyncBallCountWithUI(targetBallNum);
		primitivesManager.Update(elapsedTime, mouseWorldPos);
		primitivesManager.Render(renderer);


		//ImGui_ImplDX11_NewFrame();
		//ImGui_ImplWin32_NewFrame();
		//ImGui::NewFrame();

		//ImGui::Begin("Jungle Property Window");

		//ImGui::InputInt("Number of Balls", &targetBallNum);
		//targetBallNum = targetBallNum < 0 ? 0 : targetBallNum;

		//ImGui::Checkbox("Gravity", &UBall::bApplyGravity);
		//ImGui::Checkbox("Attraction", &UBall::bApplyAttraction);

		//if (ImGui::Button("Quit this app"))
		//{
		//	PostMessage(hWnd, WM_QUIT, 0, 0);
		//}

		//ImGui::End();


		//ImGui::Render();
		//ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		renderer.SwapBuffer();

		do
		{
			Sleep(0);

			QueryPerformanceCounter(&endTime);
			elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;
		} while (elapsedTime < targetFrameTime);
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	renderer.ReleaseVertexBuffer(vertexBufferTriangle);

	UBall::ReleaseBuffer(renderer);

	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();

	return 0;
}