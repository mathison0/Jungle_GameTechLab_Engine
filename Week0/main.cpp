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


struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
};

#include "Sphere.h"

FVertexSimple triangle_vertices[] =
{
	{  0.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top vertex (red)
	{  1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right vertex (green)
	{ -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f }  // Bottom-left vertex (blue)
};

FVector3 gravity;


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