#include "URenderer.h"


void URenderer::Create(HWND hWindow)
{
	// Direct3D 장치 및 스왑 체인 생성
	CreateDeviceAndSwapChain(hWindow);

	CreateFrameBuffer();

	CreateRasterizerState();
	
	CreateBlendState();
	// 깊이 스텐실 버퍼 및 블렌드 상태는 이 코드에서는 다루지 않음
}

void URenderer::CreateDeviceAndSwapChain(HWND hWindow)
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

void URenderer::ReleaseDeviceAndSwapChain()
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

void URenderer::CreateFrameBuffer()
{
	// 스왑체인에서 백 버퍼 텍스처 가져오기 
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

	// 렌더 타겟 뷰 생성
	D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
	framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 색상 포맷
	framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // 2D 텍스처

	Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
}

void URenderer::ReleaseFrameBuffer()
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

void URenderer::CreateRasterizerState()
{
	D3D11_RASTERIZER_DESC rasterizerdesc = {};
	rasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
	rasterizerdesc.CullMode = D3D11_CULL_BACK; // 백 페이스 컬링

	Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
}

void URenderer::ReleaseRasterizerState()
{
	if (RasterizerState)
	{
		RasterizerState->Release();
		RasterizerState = nullptr;
	}
}

void URenderer::ReleaseTextures()
{
	for (auto& pair : TextureSRVs)
	{
		if (pair.second)
		{
			pair.second->Release();
			pair.second = nullptr;
		}
	}
	TextureSRVs.clear();

	// SamplerState Release
	if (SamplerState)
	{
		SamplerState->Release();
		SamplerState = nullptr;
	}
}

void URenderer::Release()
{
	ReleaseTextures();
	ReleaseBlendState();

	RasterizerState->Release();

	// 렌더 타겟을 초기화
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	ReleaseFrameBuffer();
	ReleaseDeviceAndSwapChain();
}

void URenderer::SwapBuffer()
{
	SwapChain->Present(1, 0); // 1: VSync 활성화
}

void URenderer::CreateShader()
{
	ID3DBlob* vertexshaderCSO;
	ID3DBlob* pixelshaderCSO;

	ID3DBlob* backgroundVertexShaderCSO;
	ID3DBlob* backgroundPixelShaderCSO;


	ID3DBlob* spriteVertexShaderCSO;
	ID3DBlob* spritePixelShaderCSO;

		

	D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

	Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &SimpleVertexShader);

	D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

	Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // UV 좌표 추가!
	};
	Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);


	D3DCompileFromFile(L"Background.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &backgroundVertexShaderCSO, nullptr);

	Device->CreateVertexShader(backgroundVertexShaderCSO->GetBufferPointer(), backgroundVertexShaderCSO->GetBufferSize(), nullptr, &backgroundVertexShader);


	D3DCompileFromFile(L"Background.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &backgroundPixelShaderCSO, nullptr);
	
	Device->CreatePixelShader(backgroundPixelShaderCSO->GetBufferPointer(), backgroundPixelShaderCSO->GetBufferSize(), nullptr, &backgroundPixelShader);

	D3DCompileFromFile(L"UI.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &spriteVertexShaderCSO, nullptr);

	Device->CreateVertexShader(spriteVertexShaderCSO->GetBufferPointer(), spriteVertexShaderCSO->GetBufferSize(), nullptr, &UIVertexShader);


	D3DCompileFromFile(L"UI.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &spritePixelShaderCSO, nullptr);

	Device->CreatePixelShader(spritePixelShaderCSO->GetBufferPointer(), spritePixelShaderCSO->GetBufferSize(), nullptr, &UIPixelShader);


	vertexshaderCSO->Release();
	pixelshaderCSO->Release();
	spriteVertexShaderCSO->Release();
	spritePixelShaderCSO->Release();
	backgroundPixelShaderCSO->Release();
	backgroundVertexShaderCSO->Release();


	Stride = sizeof(FVertexSimple);

}

void URenderer::ReleaseShader()
{
	if (SimpleInputLayout)
	{
		SimpleInputLayout->Release();
		SimpleInputLayout = nullptr;
	}

	if (SimpleVertexShader)
	{
		SimpleVertexShader->Release();
		SimpleVertexShader = nullptr;
	}

	if (SimplePixelShader)
	{
		SimplePixelShader->Release();
		SimplePixelShader = nullptr;
	}

	if(backgroundVertexShader)
	{
		backgroundVertexShader->Release();
		backgroundVertexShader = nullptr;
	}

	if(backgroundPixelShader)
	{
		backgroundPixelShader->Release();
		backgroundPixelShader = nullptr;
	}

	if(UIVertexShader)
	{
		UIVertexShader->Release();
		UIVertexShader = nullptr;
	}

	if (UIPixelShader)
	{
		UIPixelShader->Release();
		UIPixelShader = nullptr;
	}
}

void URenderer::Prepare()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetViewports(1, &ViewportInfo);
	DeviceContext->RSSetState(RasterizerState);

	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void URenderer::PrepareShader()
{
	DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
	DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
	DeviceContext->IASetInputLayout(SimpleInputLayout);

	if (ConstantBuffer)
	{
		DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
		DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer);
	}
}

void URenderer::RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
{
	UINT offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);

	DeviceContext->Draw(numVertices, 0);
}

ID3D11Buffer* URenderer::CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
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

void URenderer::ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
{
	vertexBuffer->Release();
}

void URenderer::CreateConstantBuffer()
{
	D3D11_BUFFER_DESC constantbufferdesc = {};
	constantbufferdesc.ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
	constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
	constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);

	Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstnantBufferPerFrame);
}

void URenderer::ReleaseConstantBuffer()
{
	if (ConstantBuffer)
	{
		ConstantBuffer->Release();
		ConstantBuffer = nullptr;
	}

	if(ConstnantBufferPerFrame)
	{
		ConstnantBufferPerFrame->Release();
		ConstnantBufferPerFrame = nullptr;
	}
}

void URenderer::UpdateConstant(FVector3 Offset, float Angle, FVector3 scale, XMFLOAT4 color, XMFLOAT2 uvOffset, XMFLOAT2 uvScale, int flag, float spinAngle)
{
	if (ConstantBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

		DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
		FConstants* constants = (FConstants*)constantbufferMSR.pData;
		{
			constants->Offset = Offset;
			constants->Angle = Angle;
			constants->Scale = scale;
			constants->Flag = flag;
			constants->uvOffset = uvOffset;
			constants->uvScale = uvScale;
			constants->Color = color;
			constants->spinAngle = spinAngle;
		}
		DeviceContext->Unmap(ConstantBuffer, 0);
	}
}

void URenderer::UpdateConstantPerFrame(float cameraY)
{
	if (ConstnantBufferPerFrame)
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
		DeviceContext->Map(ConstnantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
		FConstantPerFrame* constants = (FConstantPerFrame*)constantbufferMSR.pData;
		{
			constants->cameraY = cameraY;
		}
		DeviceContext->Unmap(ConstnantBufferPerFrame, 0);

		DeviceContext->VSSetConstantBuffers(1, 1, &ConstnantBufferPerFrame);
		DeviceContext->PSSetConstantBuffers(1, 1, &ConstnantBufferPerFrame);
	}
}

void URenderer::CreateBlendState()
{
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	Device->CreateBlendState(&blendDesc, &AlphaBlendState);
}

void URenderer::ReleaseBlendState()
{
	if (AlphaBlendState)
	{
		AlphaBlendState->Release();
		AlphaBlendState = nullptr;
	}
}

void URenderer::EnableAlphaBlending(bool enable)
{
	if (enable && AlphaBlendState)
	{
		float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		DeviceContext->OMSetBlendState(AlphaBlendState, blendFactor, 0xffffffff);
	}
	else
	{
		DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	}
}
