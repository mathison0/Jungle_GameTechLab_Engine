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
#include "UIManager.h"

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

FVector3 gravity;

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

UBall* player;
Moon* testPlanet;
Camera* camera;
Sprite* background;
UIManager* uiManager;


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
		{"Earth", L"earth.jpg"},
		{"Mars", L"mars.jpg"},
		{"Moon", L"moon.jpg"},
		{"Jupiter", L"jupiter.jpg"},
		{"Venus", L"venus.jpg"},
		{"Mercury", L"mercury.jpg"},
		{"Neptune", L"neptune.jpg"},
		{"Meteor", L"meteor.png"},
		{"Background", L"background.jpg"},
		{"StartButton", L"startButton.png" },
		{"RestartButton", L"restartButton.png" }
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

	struct PlanetData
	{
		std::string name;
		float relativeRadius; // 지구를 1.0 기준
	};

	PlanetData planetDataList[] = {
		{"Mercury", 1.383f},
		{"Venus", 1.949f},
		{"Mars", 1.532f},
		{"Jupiter", 7.21f},
		{"Neptune", 4.883f},
		{"Meteor", 4.883f},
	};

	const float baseRadius = 0.05f; // 지구 기준 반지름
	const int maxAttempts = 50;     // 배치 시도 횟수
	const float minSpeed = 0.01f; // 최소 속도
	const float maxSpeed = 0.03f; // 최대 속도
	std::vector<UBall*> placedBalls;

	for (int i = 0; i < sizeof(planetDataList) / sizeof(PlanetData); ++i)
	{
		float radius = baseRadius * planetDataList[i].relativeRadius;
		FVector3 newPos;
		bool bPlaced = false;

		for (int attempt = 0; attempt < maxAttempts; ++attempt)
		{
			newPos.x = ((rand() % 1000) / 1000.0f) * 1.6f - 0.8f;
			newPos.y = ((rand() % 1000) / 1000.0f) * 1.6f - 0.8f;
			newPos.z = 0.0f;

			bool bOverlap = false;
			for (auto& ball : placedBalls)
			{
				float dx = newPos.x - ball->Location.x;
				float dy = newPos.y - ball->Location.y;
				float distance = sqrtf(dx * dx + dy * dy);
				float minDistance = (radius + ball->Radius) * 2.0f;

				if (distance < minDistance)
				{
					bOverlap = true;
					break;
				}
			}

			if (!bOverlap)
			{
				bPlaced = true;
				break;
			}
		}

		// 랜덤 속도 생성
		float randomAngle = (rand() % 360) * (3.141592f / 180.0f); // 0~360도 랜덤 각도
		float randomSpeed = minSpeed + ((rand() % 1000) / 1000.0f) * (maxSpeed - minSpeed); // minSpeed ~ maxSpeed 사이 랜덤 속도
		FVector3 randomVelocity;
		randomVelocity.x = cosf(randomAngle) * randomSpeed;
		randomVelocity.y = sinf(randomAngle) * randomSpeed;
		randomVelocity.z = 0.0f;

		// 50번 시도 후에도 배치 못했으면 그냥 마지막 위치에 배치
		Planet* newPlanet = nullptr;
		if (planetDataList[i].name != "Meteor")
		{
			newPlanet = new Planet(newPos, randomVelocity, radius, planetDataList[i].name);
		}
		else
		{
			newPlanet = new Meteor(newPos + 1.0f, randomVelocity, radius, planetDataList[i].name);
		}
		primitivesManager.AddObject(newPlanet);
		placedBalls.push_back(newPlanet);
	}

	//Camera 생성
	camera = new Camera();

	//Background 생성
	background = new Sprite("Background");

	uiManager = new UIManager();
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

	GetCursorPos(&mousePos);
	ScreenToClient(hWnd, &mousePos);
	mouseWorldPos.x = (mousePos.x / 512.0f) - 1.0f;
	mouseWorldPos.y = -((mousePos.y / 512.0f) - 1.0f);
	primitivesManager.Update(deltaTime, mouseWorldPos);

	uiManager->Update(mouseWorldPos.x, mouseWorldPos.y, false);

	camera->Update(deltaTime, player);
	renderer.UpdateConstantPerFrame(camera->GetCurrentCameraY());
}

void WinMainRender()
{
	background->Render(renderer);
	primitivesManager.Render(renderer);

	uiManager->Render(renderer);

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Player Info");
	if (player != nullptr)
	{
		ImGui::Text("Earth Position:");
		ImGui::Text("X: %.3f", player->Location.x);
		ImGui::Text("Y: %.3f", player->Location.y);
		ImGui::Text("Z: %.3f", player->Location.z);
	}
	ImGui::End();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	
	renderer.SwapBuffer();
}