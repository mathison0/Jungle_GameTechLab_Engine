# Week9 D 독립 구현 결과 정리

## 구현 범위

이번 작업에서는 A/B/C 팀 작업 결과가 없어도 독립적으로 구현 가능한 D 담당 기반 코드를 추가했다.

구현한 항목:

- Lua script 파일 경로/생성/열기 유틸
- 기본 `template.lua`
- Sol2에 직접 의존하지 않는 coroutine scheduler 뼈대
- Visual Studio 프로젝트 파일 등록

아직 구현하지 않은 항목:

- `ULuaScriptComponent`와 실제 연결
- Sol2 `sol::state`, `sol::coroutine` 연동
- 에디터 Property 창의 `Create Script / Edit Script / Reload Script` 버튼 연결
- overlap/hit delegate를 Lua callback으로 전달하는 통합 코드

위 항목들은 C팀의 Lua component 구조, A팀 delegate 시그니처, B팀 collision event 발생 구조가 확정된 뒤 연결해야 한다.

## 추가한 파일

### `NipsEngine/Source/Engine/Scripting/ScriptUtils.h`

Lua script 파일 관리를 위한 유틸 API를 선언했다.

주요 구조:

```cpp
struct FScriptCreateResult
{
    bool bSuccess = false;
    bool bCreated = false;
    bool bAlreadyExists = false;
    FString ScriptPath;
    FString ErrorMessage;
};
```

주요 API:

```cpp
static FString GetScriptDirectory();
static FString GetTemplateScriptPath();
static FString SanitizeFileName(const FString& Name);
static FString MakeScriptFileName(const FString& SceneName, const FString& ActorName);
static FString MakeActorScriptPath(const FString& SceneName, const FString& ActorName);
static bool DoesFileExist(const FString& Path);
static bool EnsureTemplateScript(FString* OutError = nullptr);
static FScriptCreateResult CreateScriptFromTemplate(const FString& SceneName, const FString& ActorName);
static bool OpenScript(const FString& ScriptPath, FString* OutError = nullptr);
```

### `NipsEngine/Source/Engine/Scripting/ScriptUtils.cpp`

`ScriptUtils`의 실제 구현이다.

구현 내용:

- script 저장 디렉토리: `Asset/Scripts`
- 기본 템플릿 경로: `Asset/Scripts/template.lua`
- actor별 script 경로: `Asset/Scripts/{SceneName}_{ActorName}.lua`
- script 생성 시 `template.lua`를 복사
- 대상 script가 이미 있으면 덮어쓰지 않고 `bAlreadyExists = true` 반환
- script 파일 열기는 `ShellExecuteW(..., L"open", ...)` 사용
- scene/actor 이름의 Windows 금지 문자는 `_`로 치환
- 한글 등 UTF-8 비ASCII 문자는 유지

사용 예:

```cpp
FScriptCreateResult Result =
    FScriptUtils::CreateScriptFromTemplate("Scene_01", "Player");

if (Result.bSuccess)
{
    FScriptUtils::OpenScript(Result.ScriptPath);
}
```

예상 생성 파일:

```text
NipsEngine/Asset/Scripts/Scene_01_Player.lua
```

## 기본 Lua 템플릿

### `NipsEngine/Asset/Scripts/template.lua`

발제 예시와 맞춰 기본 lifecycle/event 함수를 넣었다.

포함 함수:

```lua
function BeginPlay()
end

function EndPlay()
end

function OnOverlap(OtherActor)
end

function OnHit(HitResult)
end

function Tick(dt)
end
```

현재 템플릿은 발제 예시대로 `obj` 전역 객체를 사용한다.

```lua
function Tick(dt)
    obj.Location = obj.Location + obj.Velocity * dt
    obj:PrintLocation()
end
```

C팀에서 Lua binding 변수명을 `actor`로 확정하면, 나중에 템플릿도 `actor` 기준으로 바꾸거나 `obj`와 `actor`를 둘 다 제공하는 방식으로 맞추면 된다.

## Coroutine Scheduler

### `NipsEngine/Source/Engine/Scripting/LuaCoroutineScheduler.h`

Sol2 타입을 직접 include하지 않고, callback 기반으로 동작하는 scheduler API를 선언했다.

핵심 타입:

```cpp
struct FLuaCoroutineHandle
{
    int32 Id = 0;
    bool IsValid() const;
};

struct FLuaCoroutineYield
{
    bool bFinished = false;
    float WaitSeconds = 0.0f;
};
```

핵심 API:

```cpp
class FLuaCoroutineScheduler
{
public:
    using FResumeCallback = std::function<FLuaCoroutineYield()>;

    FLuaCoroutineHandle Start(FResumeCallback ResumeCallback, float InitialDelay = 0.0f);
    bool Cancel(FLuaCoroutineHandle Handle);
    bool IsRunning(FLuaCoroutineHandle Handle) const;
    void Tick(float DeltaTime);
    void Clear();
    int32 Num() const;
};
```

### `NipsEngine/Source/Engine/Scripting/LuaCoroutineScheduler.cpp`

구현 내용:

- `Start()`로 callback 등록
- `Tick(DeltaTime)`마다 대기 시간 감소
- 대기 시간이 0 이하가 되면 callback resume
- callback이 `{ bFinished = true }`를 반환하면 task 제거
- callback이 `{ bFinished = false, WaitSeconds = 1.0f }`를 반환하면 1초 뒤 다시 resume
- `Cancel()`로 특정 coroutine 제거
- `Clear()`로 reload/end play 시 전체 제거 가능

사용 예:

```cpp
FLuaCoroutineScheduler Scheduler;

Scheduler.Start([]()
{
    // 나중에는 여기서 sol::coroutine resume 호출
    return FLuaCoroutineYield{
        .bFinished = false,
        .WaitSeconds = 1.0f
    };
});

Scheduler.Tick(DeltaTime);
```

Sol2 통합 시 예상 연결:

```cpp
Scheduler.Start([Coroutine]()
{
    sol::protected_function_result Result = Coroutine();

    if (!Result.valid())
    {
        return FLuaCoroutineYield{ true, 0.0f };
    }

    if (Coroutine.status() == sol::call_status::ok)
    {
        return FLuaCoroutineYield{ true, 0.0f };
    }

    float WaitSeconds = Result.get<float>();
    return FLuaCoroutineYield{ false, WaitSeconds };
});
```

실제 Sol2 API 형태는 C팀 구현에 맞춰 조정해야 한다.

## 프로젝트 파일 등록

수정한 파일:

- `NipsEngine/NipsEngine.vcxproj`
- `NipsEngine/NipsEngine.vcxproj.filters`

등록한 소스:

```xml
<ClCompile Include="Source\Engine\Scripting\LuaCoroutineScheduler.cpp" />
<ClCompile Include="Source\Engine\Scripting\ScriptUtils.cpp" />
```

등록한 헤더:

```xml
<ClInclude Include="Source\Engine\Scripting\LuaCoroutineScheduler.h" />
<ClInclude Include="Source\Engine\Scripting\ScriptUtils.h" />
```

Visual Studio 필터:

```text
Source/Engine/Scripting
```

## 검증 결과

새로 추가한 C++ 파일 두 개는 MSVC 문법 검사로 확인했다.

실행한 검증:

```bat
cl /nologo /std:c++20 /EHsc /Zs ^
  /I NipsEngine\Source\Engine ^
  /I NipsEngine\Source ^
  /I NipsEngine\ThirdParty ^
  NipsEngine\Source\Engine\Scripting\ScriptUtils.cpp ^
  NipsEngine\Source\Engine\Scripting\LuaCoroutineScheduler.cpp
```

결과:

```text
ScriptUtils.cpp
LuaCoroutineScheduler.cpp
```

즉, 추가한 두 `.cpp`는 문법 검사 기준으로 통과했다.

전체 솔루션 빌드는 실패했다. 원인은 새 코드가 아니라 DirectXTK NuGet 패키지 누락이다.

빌드 실패 원인:

```text
packages\directxtk_desktop_win10.2025.10.28.2\build\native\directxtk_desktop_win10.targets
```

해당 패키지가 로컬에 복구되면 전체 빌드를 다시 확인해야 한다.

## 다음 연결 작업

### C팀 LuaScriptComponent 준비 후

`ULuaScriptComponent`에 다음 방식으로 연결하면 된다.

```cpp
bool ULuaScriptComponent::CreateScriptFromTemplate()
{
    FScriptCreateResult Result =
        FScriptUtils::CreateScriptFromTemplate(SceneName, GetOwner()->GetFName().ToString());

    if (Result.bSuccess)
    {
        ScriptPath = Result.ScriptPath;
    }

    return Result.bSuccess;
}
```

```cpp
bool ULuaScriptComponent::OpenScriptInEditor()
{
    FString Error;
    return FScriptUtils::OpenScript(ScriptPath, &Error);
}
```

```cpp
void ULuaScriptComponent::TickComponent(float DeltaTime)
{
    CallLuaTick(DeltaTime);
    CoroutineScheduler.Tick(DeltaTime);
}
```

### EditorPropertyWidget 연결

`RenderComponentProperties()`에서 selected component가 `ULuaScriptComponent`일 때만 다음 버튼을 노출하면 된다.

- `Create Script`
- `Edit Script`
- `Reload Script`

현재는 `ULuaScriptComponent` 클래스가 아직 없어서 UI 연결은 보류했다.

### A/B 이벤트 연결

A/B 작업이 완료되면 `LuaScriptComponent`가 owner actor의 primitive component event를 구독하고, 아래 Lua 함수로 전달하면 된다.

```lua
function OnOverlap(OtherActor)
end

function OnHit(HitResult)
end
```

최종 목표는 overlap event 안에서 coroutine을 시작하고, `wait(sec)`로 지연 처리되는 흐름까지 검증하는 것이다.
