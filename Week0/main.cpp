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

ID3D11Buffer* UBall::SphereVertexBuffer = nullptr;
ID3D11Buffer* UBall::CubeVertexBuffer = nullptr;
UINT UBall::NumVerticesSphere = 0;
UINT UBall::NumVerticesCube = 0;
int UBall::TotalNumBalls = 0;
bool UBall::bApplyGravity = true;
bool UBall::bApplyAttraction = false;

//Global Variables
URenderer renderer;
FPrimitivesManager primitivesManager;
bool bIsExit = false;

UBall* player;
Moon* testPlanet;
GravityPlanet* testPlanet2;
Camera* camera;

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

void InitializeRenderer(HWND hWnd);
void InitializeGameObjects();
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
	InitializeGameObjects();

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
			}
		}

		deltaTime = (float)elapsedTime / 1000.0f;


		ProcessInput();
		WinMainUpdate(hWnd);
		WinMainRender();

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

void InitializeRenderer(HWND hWnd)
{
	renderer.Create(hWnd);
	renderer.CreateShader();
	renderer.CreateSampler();

	// 텍스처 로드
	std::string textureNames[] = { "Earth", "Mars", "Moon", "Jupiter", "Venus",  "mercury",  "Neptune" };
	std::wstring textureFiles[] = { L"earth.jpg", L"mars.jpg", L"moon.jpg", L"jupiter.jpg", L"venus.jpg", L"mercury.jpg", L"neptune.jpg" };
	for (int i = 0; i < textureNames->size(); ++i)
	{
		renderer.LoadTexture(textureNames[i], textureFiles[i].c_str());
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
	testPlanet2->InitRangeResources(renderer.Device);
}

void InitializeGameObjects()
{
	//Player 생성
	player = new UBall();
	player->Location = { 0.0f, 0.0f, 0.0f };
	player->Radius = 0.05f;
	player->TextureName = "Earth";
	primitivesManager.AddObject(player);

	//TestPlanet 생성
	testPlanet = new Moon({ 0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, player->Radius * 0.7f, player, "Moon");
	primitivesManager.AddObject(testPlanet);

	testPlanet2 = new GravityPlanet({ -0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.1f, "Mars");
	primitivesManager.AddObject(testPlanet2);

	//Camera 생성
	camera = new Camera();
}

void ProcessInput()
{
	if (player != nullptr)
	{
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
	testPlanet->HandleCollision(player);

	GetCursorPos(&mousePos);
	ScreenToClient(hWnd, &mousePos);
	mouseWorldPos.x = (mousePos.x / 512.0f) - 1.0f;
	mouseWorldPos.y = -((mousePos.y / 512.0f) - 1.0f);
	primitivesManager.Update(deltaTime, mouseWorldPos);

	camera->Update(deltaTime, player);
	renderer.UpdateConstantPerFrame(camera->GetCurrentCameraY());

	testPlanet2->Gravity(player, deltaTime);
	testPlanet2->HandleCollision(player);
}

void WinMainRender()
{
	primitivesManager.Render(renderer);
	renderer.SwapBuffer();
}