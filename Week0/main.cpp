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

UBall* player;
Moon* moon;
Camera* camera;
Sprite* background;

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

//Game
struct PlanetData
{
	std::string name;
	float relativeRadius;
};

PlanetData planetDataList[] = {
	{"Mercury", 1.383f},
	{"Venus", 1.949f},
	{"Mars", 1.532f},
	{"Jupiter", 7.21f},
	{"Neptune", 4.883f},
	{"Uranus", 2.331f},
	{"Pluto", 2.745f},
	{"Meteor", 0.883f},
};

float highestPlayerY = 0.0f;
float nextSpawnY = 1.0f;
float spawnInterval = 1.5f;
// ----------------------------------

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

		/* 게임 끝 */
		if (player->Location.y >= 100.f)
		{
			break;
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

	//player, moon은 primitivesManager에서 관리하므로 delete할 필요 없습니다
	delete camera;
	delete background;
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

void SpawnRandomPlanet(float spawnBaseY)
{
	const float baseRadius = 0.05f;
	const float minSpeed = 0.01f;
	const float maxSpeed = 0.03f;

	int randIndex = rand() % (sizeof(planetDataList) / sizeof(PlanetData));
	float radius = baseRadius * planetDataList[randIndex].relativeRadius;

	FVector3 newPos;
	bool bPositionValid = false;
	const int maxAttempts = 50;

	for (int attempt = 0; attempt < maxAttempts; ++attempt)
	{
		float newX = ((rand() % 1000) / 1000.0f) * 2.0f - 1.0f; // -1.0 ~ 1.0 사이
		// 한 번에 여러 개 스폰될 때 Y축도 조금씩 다르게 퍼지도록 랜덤값 추가
		float offsetY = ((rand() % 1000) / 1000.0f) * 0.5f;
		float newY = spawnBaseY + 1.5f + offsetY;
		newPos = FVector3(newX, newY, 0.0f);

		bPositionValid = true;

		for (UPrimitive* obj : primitivesManager.GetObjects())
		{
			if (obj == nullptr) continue;
			UBall* bobj = dynamic_cast<UBall*>(obj);

			float dx = newPos.x - bobj->Location.x;
			float dy = newPos.y - bobj->Location.y;
			float distance = sqrtf(dx * dx + dy * dy);

			float minSpacing = (radius + bobj->Radius) * 1.5f;

			if (distance < minSpacing)
			{
				bPositionValid = false;
				break;
			}
		}
		if (bPositionValid) break;
	}

	//무작위 속도 및 방향 설정
	float randomAngle = (rand() % 360) * (FVector3::PI / 180.0f);
	float randomSpeed = minSpeed + ((rand() % 1000) / 1000.0f) * (maxSpeed - minSpeed);

	float sizeMultiplier = 0.05f / sqrtf(radius);
	randomSpeed *= sizeMultiplier;

	FVector3 randomVelocity;

	if (planetDataList[randIndex].name == "Meteor")
	{
		randomVelocity.x = (((rand() % 100) / 100.0f) - 0.5f) * 0.02f; //수직낙하에 가깝게
		randomVelocity.y = -(randomSpeed * 2.0f);
		randomVelocity.z = 0.0f;
	}
	else
	{
		randomVelocity.x = cosf(randomAngle) * randomSpeed;
		randomVelocity.y = sinf(randomAngle) * randomSpeed;
		randomVelocity.z = 0.0f;
	}

	Planet* newPlanet = nullptr;
	std::string planetName = planetDataList[randIndex].name;

	if (planetName == "Meteor")
	{
		newPlanet = new Meteor(newPos, randomVelocity, radius, planetName);
	}
	else if (planetName == "Jupiter")
	{
		newPlanet = new GravityPlanet(newPos, randomVelocity, radius, planetName, PlanetType::pull);
		newPlanet->brightness = 1.5f;
	}
	else if (planetName == "Mars")
	{
		newPlanet = new GravityPlanet(newPos, randomVelocity, radius, planetName, PlanetType::push);
		newPlanet->brightness = 1.5f;
	}
	else
	{
		newPlanet = new Planet(newPos, randomVelocity, radius, planetName);
		newPlanet->brightness = 0.7f + (rand() % 40) / 100.0f;
	}

	primitivesManager.AddObject(newPlanet);
}

void ProcessInput()
{
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

void InitializeGameObjects()
{
	//Player 생성
	player = new UBall();
	player->Location = { 0.0f, 0.0f, 0.0f };
	player->Radius = 0.05f;
	player->TextureName = "Earth";
	player->brightness = 1.0f;
	primitivesManager.AddObject(player);

	//Moon 생성
	moon = new Moon({ 0.0f, -1000.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, player->Radius * 0.7f, player, "Moon");
	moon->brightness = 1.0f;
	primitivesManager.AddObject(moon);

	//Camera 생성
	camera = new Camera();

	//Background 생성
	background = new Sprite("Background");
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

	camera->Update(deltaTime, player);

	if (player != nullptr)
	{
		//플레이어가 가장 높이 올라가면.... window는 max 저주가 있다
		highestPlayerY = std::max<float>(highestPlayerY, player->Location.y);
		while (highestPlayerY > nextSpawnY)
		{
			int spawnCount = (rand() % 3) + 1;
			for (int i = 0; i < spawnCount; ++i)
			{
				SpawnRandomPlanet(nextSpawnY);
			}
			nextSpawnY += spawnInterval;
		}
	}

	renderer.UpdateConstantPerFrame(camera->GetCurrentCameraY());
}

void WinMainRender()
{
	background->Render(renderer);
	primitivesManager.Render(renderer);

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