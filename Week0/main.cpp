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

#include "URenderer.h"
#include "UBall.h"
#include "PrimitivesManager.h"
#include "planet.h"
#include "GravityPlanet.h"
#include "SoundManager.h"

ID3D11Buffer* UBall::SphereVertexBuffer = nullptr;
UINT UBall::NumVerticesSphere = 0;
ID3D11Buffer* UBall::CubeVertexBuffer = nullptr;
UINT UBall::NumVerticesCube = 0;
int UBall::TotalNumBalls = 0;
bool UBall::bApplyGravity = true;
bool UBall::bApplyAttraction = false;

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
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
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
	renderer.CreateSampler();
	
	// 텍스처 로드
	renderer.LoadTexture("Earth", L"earth.jpg");
	renderer.LoadTexture("Mars", L"mars.jpg");
	
	renderer.CreateConstantBuffer();

	// ImGui Setup
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);

	bool bIsExit = false;

	const int targetFPS = 60;
	const double targetFrameTime = 1000.0 / targetFPS;

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER startTime, endTime;
	double elapsedTime = 0.0;

	POINT mousePos;
	FVector3 mouseWorldPos;

	FPrimitivesManager primitivesManager;

	UBall::InitializeBuffer(renderer);

	//SoundManager
	SoundManager::Get().Initialize();

	//여기에 원하는 음원 등록후 사용하고 싶은 곳에 헤더파일 include후 사용
	SoundManager::Get().LoadSound("Explosion", L"Audio/explosion.WAV"); 
	SoundManager::Get().LoadSound("bgm", L"Audio/bgm.WAV"); 

	// 배경음악
	SoundManager::Get().SetBGMVolume(0.5f);
	SoundManager::Get().PlayBGM("bgm");


	//Player 생성
	UBall* player = new UBall();
	player->Location = { 0.0f, 0.0f, 0.0f };
	player->Radius = 0.05f;
	player->TextureName = "Earth";
	primitivesManager.AddObject(player);

	//TestPlanet 생성
	GravityPlanet* testPlanet = new GravityPlanet({ -0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.15f, "Mars", PlanetType::push);
	primitivesManager.AddObject(testPlanet);

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
			}
		}

		SoundManager::Get().Update();

		if (player != nullptr)
		{
			testPlanet->Gravity(player, elapsedTime);
		}

		// 플레이어 조작
		if (player != nullptr)
		{
			bool bCurrentLeftPressed = (GetAsyncKeyState('A') & 0x8000) != 0;
			bool bCurrentRightPressed = (GetAsyncKeyState('D') & 0x8000) != 0;

			if (bCurrentLeftPressed || bCurrentRightPressed)
			{
				player->ApplyThrust(bCurrentLeftPressed, bCurrentRightPressed, elapsedTime);
			}
		}

		renderer.Prepare();
		renderer.PrepareShader();

		GetCursorPos(&mousePos);
		ScreenToClient(hWnd, &mousePos);

		mouseWorldPos.x = (mousePos.x / 512.0f) - 1.0f;
		mouseWorldPos.y = -((mousePos.y / 512.0f) - 1.0f);

		primitivesManager.Update(elapsedTime, mouseWorldPos);
		primitivesManager.Render(renderer);

		testPlanet->HandleCollision(player);
		

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

	SoundManager::Get().Release();

	UBall::ReleaseBuffer(renderer);
	renderer.ReleaseConstantBuffer();
	renderer.ReleaseShader();
	renderer.Release();

	return 0;
}