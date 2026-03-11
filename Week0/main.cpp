#define NOMINMAX
#include <windows.h>
#include "dx11math.h"
struct FVector3;
/*
리팩토링 필
*/
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

#include "URenderer.h"
#include "UBall.h"
#include "PrimitivesManager.h"
#include "planet.h"
#include "Camera.h"
#include "SoundManager.h"
#include "Sprite.h"


struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
};

FVertexSimple triangle_vertices[] =
{
	{  0.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top vertex (red)
	{  1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right vertex (green)
	{ -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f }  // Bottom-left vertex (blue)
};

ID3D11Buffer* UBall::SphereVertexBuffer = nullptr;
ID3D11Buffer* UBall::CubeVertexBuffer = nullptr;
ID3D11Buffer* UBall::PNGSphereVertexBuffer = nullptr;
UINT UBall::NumVerticesSphere = 0;
UINT UBall::NumVerticesCube = 0;
UINT UBall::NumVerticesPNGSphere = 0;
int UBall::TotalNumBalls = 0;
bool UBall::bApplyGravity = true;
bool UBall::bApplyAttraction = false;

//Global Variables
URenderer renderer;
FPrimitivesManager primitivesManager;
bool bIsExit = false;

//FPS, time
const int targetFPS = 60;
const double targetFrameTime = 1000.0 / targetFPS;
LARGE_INTEGER frequency;
LARGE_INTEGER startTime, endTime;
double elapsedTime = 0.0;
float deltaTime = 0.0f;

//mouse
POINT mousePos;
FVector3 mouseWorldPos;

// ----------------------------------

void InitializeRenderer(HWND hWnd);
void ProcessInput();
void WinMainUpdate(HWND hWnd);
void WinMainRender();

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

//_In, _In_opt_ 을 매개변수 앞에 추가해 warn C28251 제거
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";

	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };
	RegisterClassW(&wndclass);

	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInstance, nullptr);

	InitializeRenderer(hWnd);
	primitivesManager.InitializeGameObjects();

	//SoundManager
	SoundManager::Get().Init();
	
	// 원하는 음원을 아래처럼 등록해서 원하는 곳에서 헤더만 포함해서 사용
	SoundManager::Get().LoadSound("Explosion", L"Audio/explosion.WAV");
	SoundManager::Get().LoadSound("bgm", L"Audio/bgm.WAV");
	SoundManager::Get().PlayBGM("bgm", true);

	// Main Loop 
	while (bIsExit == false)
	{
		QueryPerformanceCounter(&startTime);
		MSG msg;

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
			if (msg.message == WM_KEYDOWN)
			{
				if (msg.wParam == 'Q') bIsExit = true;
				//R을 눌러서 리셋 가능하도록 만들어뒀습니다.
				//명칭은 clear로 하라고 하셨는지 기억이 잘 안네요
				if (msg.wParam == 'R')
				{
					primitivesManager.Reset();
					primitivesManager.InitializeGameObjects();
				}
			}
		}

		deltaTime = (float)elapsedTime / 1000.0f;


		ProcessInput();
		WinMainUpdate(hWnd);
		WinMainRender();

		//아래처럼 리셋 되도록 만들어뒀습니다.
		UBall* player = primitivesManager.GetPlayer();
		if (player && player->Location.y >= 100.f)
		{
			primitivesManager.Reset();
			primitivesManager.InitializeGameObjects();
		}

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

	SoundManager::Get().Release();

	UBall::ReleaseBuffer(renderer);
	Sprite::ReleaseQuadVertexBuffer(renderer);
	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();

	return 0;
}

void InitializeRenderer(HWND hWnd)
{
	renderer.Create(hWnd);
	renderer.CreateShader();
	renderer.CreateSampler();

	// 텍스처 로드
	struct TextureInfo
	{
		std::string name;
		std::wstring file;
	};

	TextureInfo textures[] = {
		{"Background", L"background.jpg"},
		{"Earth", L"earth.jpg"},
		{"Mars", L"mars.jpg"},
		{"Moon", L"moon.jpg"},
		{"Jupiter", L"jupiter.jpg"},
		{"Venus", L"venus.jpg"},
		{"Mercury", L"mercury.jpg"},
		{"Neptune", L"neptune.jpg"},
		{"Uranus", L"uranus.jpg"},
		{"Pluto", L"pluto.jpg"},
		{"Meteor", L"me.png"}
	};

	for (const auto& tex : textures)
	{
		renderer.LoadTexture(tex.name, tex.file.c_str());
	}

	renderer.CreateConstantBuffer();
	
	// ImGui Setup
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);

	QueryPerformanceFrequency(&frequency);

	UBall::InitializeBuffer(renderer);
	Sprite::CreateQuadVertexBuffer(renderer);
}

void ProcessInput()
{
	UBall* player = primitivesManager.GetPlayer();
	if (player != nullptr)
	{
		if (player->inputLockTimer > 0.0f)
		{
			player->inputLockTimer -= deltaTime;
			return;
		}

		bool bCurrentLeftPressed = (GetAsyncKeyState('A') & 0x8000) != 0;
		bool bCurrentRightPressed = (GetAsyncKeyState('D') & 0x8000) != 0;

		if (bCurrentLeftPressed || bCurrentRightPressed)
		{
			player->ApplyThrust(bCurrentLeftPressed, bCurrentRightPressed, deltaTime);
		}
	}
}

void WinMainUpdate(HWND hWnd)
{
	renderer.Prepare();
	renderer.PrepareShader();

	GetCursorPos(&mousePos);
	ScreenToClient(hWnd, &mousePos);
	mouseWorldPos.x = (mousePos.x / 512.0f) - 1.0f;
	mouseWorldPos.y = -((mousePos.y / 512.0f) - 1.0f);
	primitivesManager.Update(deltaTime, mouseWorldPos);

	Camera* camera = primitivesManager.GetCamera();
	if (camera)
	{
		renderer.UpdateConstantPerFrame(camera->GetCurrentCameraY());
	}
}

void WinMainRender()
{
	primitivesManager.Render(renderer);

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Player Info");
	
	UBall* player = primitivesManager.GetPlayer();
	if (player != nullptr)
	{
		ImGui::Text("Earth Position:");
		ImGui::Text("X: %.3f", player->Location.x);
		ImGui::Text("Y: %.3f", player->Location.y);
		ImGui::Text("Z: %.3f", player->Location.z);
		
		if (ImGui::Button("Reset Game (R)"))
		{
			primitivesManager.Reset();
			primitivesManager.InitializeGameObjects();
		}
	}
	ImGui::Text("Press R to Restart");
	ImGui::End();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	
	renderer.SwapBuffer();
}