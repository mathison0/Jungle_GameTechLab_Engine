#pragma once

#include <windows.h>
#include "dx11math.h"

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

#include "Type.h"

class URenderer
{
public:
	// Direct3D 11 장치와 장치 컨텍스트 및 스왑 체인을 관리하기 위한 포인터들
	ID3D11Device* Device = nullptr; // GPU와 통신하기 위한 Direct3D 장치
	ID3D11DeviceContext* DeviceContext = nullptr; // GPU 명령 실행을 담당하는 컨텍스트
	IDXGISwapChain* SwapChain = nullptr; // 프레임 버퍼를 교체하는 데 사용되는 스왑 체인

	//텍스처 맵핑5
	std::map<std::string, ID3D11ShaderResourceView*> TextureSRVs;
	ID3D11SamplerState* SamplerState = nullptr;

	// 렌더링에 필요한 리소스 및 상태를 관리하기 위한 변수들
	ID3D11Texture2D* FrameBuffer = nullptr; // 화면 출력용 텍스쳐
	ID3D11RenderTargetView* FrameBufferRTV = nullptr; // 텍스처를 렌더 타겟으로 사용하는 뷰
	ID3D11RasterizerState* RasterizerState = nullptr; // 래스터라이저 상태(컬링, 채우기 모드 등 정의)
	ID3D11BlendState* AlphaBlendState = nullptr;//블렌딩

	ID3D11Buffer* ConstantBuffer = nullptr; // 쉐이더에 데이터를 전달하기 위한 상수 버퍼
	ID3D11Buffer* ConstnantBufferPerFrame = nullptr; // 매 프레임마다 업데이트되는 상수 버퍼

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f }; // 화면을 초기화할 때 사용할 색상 (RGBA)
	D3D11_VIEWPORT ViewportInfo; // 렌더링 영역을 정의하는 뷰포트 정보

	// Shader
	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;

	ID3D11VertexShader* backgroundVertexShader = nullptr;
	ID3D11PixelShader* backgroundPixelShader = nullptr;

	ID3D11VertexShader* UIVertexShader = nullptr;
	ID3D11PixelShader* UIPixelShader = nullptr;

	unsigned int Stride;

	struct FConstants
	{
		FVector3 Offset;
		float Angle;

		FVector3 Scale;
		int Flag;

		XMFLOAT4 Color;

		XMFLOAT2 uvOffset;
		XMFLOAT2 uvScale;

		float spinAngle;
		float padding[3];
	};

	struct FConstantPerFrame
	{
		float cameraY;
		float padding[3];
	};


public:
	void Create(HWND hWindow);
	void CreateDeviceAndSwapChain(HWND hWindow);
	void ReleaseDeviceAndSwapChain();
	void CreateFrameBuffer();
	void ReleaseFrameBuffer();
	void CreateRasterizerState();
	void ReleaseRasterizerState();
	void Release();
	void SwapBuffer();
	void CreateShader();
	void ReleaseShader();
	void Prepare();
	void PrepareShader();
	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices);
	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);
	void CreateConstantBuffer();
	void ReleaseConstantBuffer();
	void UpdateConstant(FVector3 Offset, float Angle, FVector3 scale = { 0.f,0.f,0.f }, XMFLOAT4 color = { 1.0f,1.0f ,1.0f ,1.0f }, XMFLOAT2 uvOffset = {0.f,0.f}, XMFLOAT2 uvScale = {1.0f, 1.0f}, int flag = 1, float spinAngle = 0.f);
	void UpdateConstantPerFrame(float cameraY);

	void ReleaseTextures();
	bool LoadTexture(const std::string& name, const wchar_t* filename);
	void CreateSampler();
	ID3D11ShaderResourceView* GetTexture(const std::string& name);

	void CreateBlendState();
	void ReleaseBlendState();
	void EnableAlphaBlending(bool enable);
};

inline bool URenderer::LoadTexture(const std::string& name, const wchar_t* filename)
{
	if (TextureSRVs.find(name) != TextureSRVs.end())
	{
		return true;
	}
	std::wstring fullPath = L"Images\\";
	fullPath += filename;
	ID3D11ShaderResourceView* srv = nullptr;
	HRESULT hr = CreateWICTextureFromFile(
		Device, DeviceContext, fullPath.c_str(), nullptr,
		&srv
	);

	if (FAILED(hr))
	{
		std::string errorMsg = "Failed to load texture: ";
		// wchar_t* → string 변환
		std::wstring wFilename(filename);
		errorMsg += std::string(wFilename.begin(), wFilename.end());
		errorMsg += "\nHRESULT: 0x" + std::to_string(hr);

		MessageBoxA(nullptr, errorMsg.c_str(), "Texture Load Error", MB_OK | MB_ICONERROR);
		return false;
	}

	TextureSRVs[name] = srv;
	return true;
}

inline void URenderer::CreateSampler()
{
	if (SamplerState)
	{
		return;
	}

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	Device->CreateSamplerState(&sampDesc, &SamplerState);
}

inline ID3D11ShaderResourceView* URenderer::GetTexture(const std::string& name)
{
	auto it = TextureSRVs.find(name);
	if (it != TextureSRVs.end())
	{
		return it->second;
	}
	return nullptr;
}