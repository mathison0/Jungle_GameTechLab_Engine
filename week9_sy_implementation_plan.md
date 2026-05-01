# Week9 D 역할 구현 계획

## 결론

`week9_project_sy.md`에 정리된 내 담당 범위는 대체로 적절하다. 다만 D 역할은 `Coroutine / Editor UX / Integration`이라서 독립적으로 완성하기 어렵고, C의 Lua 실행 컴포넌트와 A/B의 이벤트 계약이 어느 정도 고정된 뒤에 최종 연결할 수 있다.

내가 먼저 진행할 수 있는 부분은 다음이다.

- Lua coroutine scheduler의 C++ 구조 설계와 기본 구현
- `wait(sec)`, `StartCoroutine(...)` 같은 Lua 노출 API 초안
- 에디터의 `Create Script / Edit Script / Reload Script` 버튼 위치와 호출 흐름 구현
- 스크립트 파일 경로 규칙, `template.lua` 복사 규칙, `ShellExecute` 기반 편집기 열기 유틸 구현

다른 팀원 결과가 필요해지는 부분은 다음이다.

- C의 `LuaScriptComponent`, `LuaScriptSystem`, `sol::state` 소유 구조
- A의 `TDelegate`와 overlap/hit delegate 시그니처
- B의 collision component 및 begin/end overlap, hit 발생 타이밍

따라서 D 작업은 "선구현 가능한 유틸/스케줄러"와 "C/A/B 결과를 받은 뒤 붙이는 통합 코드"로 나누어 진행하는 것이 맞다.

## 현재 코드에서 확인한 진입점

- 컴포넌트 생명주기: `NipsEngine/Source/Engine/Component/ActorComponent.h`
  - `BeginPlay()`, `EndPlay()`, `ExecuteTick(float)`, `TickComponent(float)`가 이미 있다.
  - coroutine scheduler는 `LuaScriptComponent::TickComponent()` 또는 `LuaScriptSystem::Tick()`에서 매 프레임 갱신하는 형태가 자연스럽다.

- 액터 생명주기: `NipsEngine/Source/Engine/GameFramework/AActor.cpp`
  - actor가 `BeginPlay`, `Tick`, `EndPlay`에서 owned component를 순회한다.
  - `LuaScriptComponent`가 일반 component로 붙으면 lifecycle 호출 흐름을 그대로 탈 수 있다.

- 월드 Tick: `NipsEngine/Source/Engine/GameFramework/World.cpp`
  - game/editor preview tick 이후 `SyncSpatialIndex()`가 호출된다.
  - collision 이벤트가 world tick 중 어디에서 발생하는지는 B 작업과 맞춰야 한다.

- 에디터 컴포넌트 추가 메뉴: `NipsEngine/Source/Editor/Utility/EditorComponentFactory.cpp`
  - `GetMenuRegistry()`에 새 컴포넌트 메뉴를 추가하는 구조다.
  - `LuaScriptComponent`가 생기면 이 registry에 `"Lua Script Component"`를 추가하면 된다.

- Property 창 버튼 추가 위치: `NipsEngine/Source/Editor/UI/EditorPropertyWidget.cpp`
  - `RenderComponentProperties()`가 선택된 컴포넌트의 세부 UI를 그린다.
  - `LuaScriptComponent` 선택 시에만 `Create Script`, `Edit Script`, `Reload Script` 버튼을 추가하는 방식이 적합하다.

- 외부 파일 열기 사례: `NipsEngine/Source/Editor/UI/EditorToolbarWidget.cpp`
  - 이미 `ShellExecuteW(..., L"open", ...)` 사용 사례가 있다.
  - `.lua` 파일 열기도 같은 방식으로 구현하면 된다.

## 구현 단계

### 1. C팀과 최소 인터페이스 먼저 합의

D가 바로 의존하는 C 인터페이스를 먼저 고정해야 한다.

필요한 최소 API:

```cpp
class ULuaScriptComponent : public UActorComponent
{
public:
    const FString& GetScriptPath() const;
    void SetScriptPath(const FString& InPath);

    bool CreateScriptFromTemplate();
    bool OpenScriptInEditor();
    bool ReloadScript();

    void StartCoroutine(const FString& FunctionName);
};
```

합의해야 할 항목:

- `sol::state`를 component마다 가질지, `LuaScriptSystem`에서 공유/관리할지
- script reload 시 기존 coroutine을 모두 중단할지, 유지할지
- Lua 전역 `obj`를 component owner actor로 둘지, 더 명확하게 `actor` 이름으로 둘지
- Lua 함수 호출 실패 시 로그 처리 방식

권장안:

- 1차 구현은 component별 `sol::state`가 단순하다.
- reload 시 coroutine은 전부 clear한다.
- Lua에는 `obj`와 `actor`를 둘 다 제공해서 발제 예시와 명확성을 같이 맞춘다.

### 2. ScriptUtils 구현

파일 생성/열기/경로 처리는 Lua 실행 로직과 분리한다.

생성 후보:

- `NipsEngine/Source/Engine/Scripting/ScriptUtils.h`
- `NipsEngine/Source/Engine/Scripting/ScriptUtils.cpp`

담당 기능:

- script 저장 디렉토리 결정
- scene name + actor name 기반 파일명 생성
- `template.lua`가 없으면 기본 템플릿 생성 또는 실패 로그
- template을 actor script로 복사
- 이미 파일이 있으면 덮어쓰지 않고 기존 파일 유지
- `ShellExecuteW`로 `.lua` 파일 열기

권장 경로 규칙:

```text
NipsEngine/Asset/Scripts/{SceneName}_{ActorName}.lua
NipsEngine/Asset/Scripts/template.lua
```

주의점:

- actor 이름에 파일명으로 부적절한 문자가 들어갈 수 있으므로 sanitize가 필요하다.
- scene name을 아직 안정적으로 얻기 어렵다면 1차는 world/context name 또는 `"DefaultScene"` fallback을 둔다.

### 3. LuaCoroutineScheduler 구현

생성 후보:

- `NipsEngine/Source/Engine/Scripting/LuaCoroutineScheduler.h`
- `NipsEngine/Source/Engine/Scripting/LuaCoroutineScheduler.cpp`

1차 목표:

- Lua 함수 또는 Lua thread를 coroutine으로 등록
- `wait(sec)` 호출 시 현재 coroutine을 yield
- C++ tick에서 남은 대기 시간을 줄이고, 0 이하가 되면 resume
- coroutine 완료/에러 시 목록에서 제거

권장 데이터 구조:

```cpp
struct FLuaCoroutineHandle
{
    int32 Id = 0;
};

struct FLuaCoroutineTask
{
    int32 Id = 0;
    float WaitTime = 0.0f;
    bool bFinished = false;
    // sol::thread / sol::coroutine 등 C팀 sol2 구조에 맞춰 보관
};
```

필요 API:

```cpp
class FLuaCoroutineScheduler
{
public:
    void BindWaitFunction(sol::state& Lua);
    FLuaCoroutineHandle Start(sol::function Function);
    void Tick(float DeltaTime);
    void Clear();
};
```

C팀과 맞출 부분:

- `sol::state` include 위치와 ThirdParty 구성
- `sol::coroutine`, `sol::thread` 중 어떤 타입을 사용할지
- `wait(sec)`에서 yield 값으로 delay를 넘길지, scheduler 상태에 직접 기록할지

### 4. LuaScriptComponent와 scheduler 연결

C팀의 `LuaScriptComponent`가 준비되면 D 구현을 붙인다.

연결 방식:

- `BeginPlay()`: script load 후 Lua `BeginPlay()` 호출
- `TickComponent(float DeltaTime)`: Lua `Tick(dt)` 호출 후 scheduler tick
- `EndPlay()`: Lua `EndPlay()` 호출 후 scheduler clear
- `ReloadScript()`: 기존 Lua state와 coroutine clear 후 script reload
- `StartCoroutine("EnemyAI.start")`: Lua function lookup 후 scheduler에 등록

권장 Tick 순서:

1. 일반 Lua `Tick(dt)` 호출
2. coroutine scheduler `Tick(dt)` 호출

이유:

- Lua Tick에서 새 coroutine을 시작한 경우 같은 프레임에 scheduler가 한 번 처리할 수 있다.

### 5. Editor UX 연결

수정 후보:

- `NipsEngine/Source/Editor/Utility/EditorComponentFactory.cpp`
- `NipsEngine/Source/Editor/UI/EditorPropertyWidget.cpp`

작업:

- Add Component 메뉴에 `Lua Script Component` 추가
- `RenderComponentProperties()`에서 selected component가 `ULuaScriptComponent`일 때 버튼 표시
- 버튼 3개 연결
  - `Create Script`: script path가 없으면 생성하고 component에 경로 저장
  - `Edit Script`: script 파일을 `ShellExecuteW`로 열기
  - `Reload Script`: component reload 호출

버튼 조건:

- script path 없음: `Create Script` 활성, `Edit/Reload`는 비활성 또는 생성 유도
- script path 있음: `Create Script`는 "이미 있음" 처리, `Edit/Reload` 활성
- 파일 삭제됨: `Edit/Reload` 전에 존재 확인 후 에러 로그

### 6. A/B/C 결과와 최종 통합

A 결과 필요:

- `OnComponentBeginOverlap`
- `OnComponentEndOverlap`
- `OnComponentHit`
- delegate `AddDynamic`, `Broadcast` 시그니처

B 결과 필요:

- overlap/hit이 실제 어느 tick 단계에서 broadcast되는지
- `FOverlapResult`, `FHitResult` 필드 확정
- `UPrimitiveComponent`에 collision 관련 property가 들어가는지 확인

C 결과 필요:

- `AActor`, `UObject`, `FVector`, Transform API Lua binding
- Lua function 호출 wrapper
- script load/reload 실패 처리

D 통합 작업:

- `LuaScriptComponent`가 owner actor 또는 primitive component event를 구독
- C++ event를 Lua `OnOverlap(OtherActor)`, `OnHit(HitResult)`로 전달
- overlap 이벤트 안에서 coroutine 시작이 가능한지 검증
- sample script 작성

## 작업 의존성

| 내 작업 | 선행 필요 작업 | 병렬 가능 여부 | 메모 |
| --- | --- | --- | --- |
| ScriptUtils | 없음 | 가능 | 지금 바로 구현 가능 |
| template.lua 정리 | C의 binding 이름 일부 | 부분 가능 | `obj`만 쓰는 기본 템플릿은 먼저 가능 |
| Editor 버튼 UI | C의 `ULuaScriptComponent` 클래스명/API | 부분 가능 | UI 위치는 지금 확정 가능 |
| Add Component 메뉴 등록 | C의 component 클래스 | 필요 | include와 factory 등록은 클래스 생성 뒤 |
| CoroutineScheduler | C의 sol2 통합 방식 | 부분 가능 | 헤더/구조 초안은 가능, sol 타입은 C와 맞춰야 함 |
| `wait(sec)` Lua 노출 | C의 `sol::state` 소유권 | 필요 | state 접근 지점이 정해져야 함 |
| `StartCoroutine` | C의 Lua function lookup | 필요 | 문자열 경로 처리 규칙 합의 필요 |
| `OnOverlap` Lua 전달 | A/B delegate와 collision event | 필요 | 가장 뒤에 붙이는 통합 작업 |
| 최종 샘플 시나리오 | A/B/C 완료 | 필요 | 실제 overlap -> Lua -> coroutine 흐름 검증 |

## 권장 일정

### 1차: 독립 구현

- `ScriptUtils` 작성
- script 경로 규칙 확정
- `template.lua` 기본본 작성
- editor 버튼 위치와 UX 흐름 설계
- coroutine scheduler API 초안 작성

### 2차: C팀 Lua 컴포넌트와 연결

- `ULuaScriptComponent`에 create/edit/reload 연결
- `BeginPlay/Tick/EndPlay`에서 Lua lifecycle 호출 확인
- `wait(sec)`와 coroutine resume 확인
- reload 시 coroutine clear 확인

### 3차: A/B팀 이벤트와 연결

- begin overlap, end overlap, hit delegate 구독
- Lua `OnOverlap`, `OnHit` 호출
- collision event 안에서 coroutine 시작 테스트

### 4차: 최종 검증

- actor에 script component 추가
- script 생성 버튼으로 파일 생성
- edit 버튼으로 `.lua` 열기
- reload 버튼으로 변경사항 반영
- 게임 실행 중 Lua `Tick(dt)` 동작 확인
- overlap 발생 시 Lua callback 호출 확인
- callback에서 `StartCoroutine` 또는 `wait(sec)` 흐름 확인

## 리스크와 조정 포인트

- LuaJIT/Sol2 빌드 통합이 늦어지면 coroutine 구현은 컴파일 가능한 형태로 마무리하기 어렵다.
- `sol::state`를 공유 state로 갈 경우 scheduler가 component별 coroutine을 구분해야 한다.
- hot reload 시 기존 Lua 함수와 coroutine stack을 유지하려 하면 복잡도가 커진다. 1차는 reload 시 전부 중단하는 정책이 안전하다.
- editor에서 script 파일을 생성할 때 scene name을 얻는 API가 없으면 우선 `"DefaultScene"` fallback을 두고, 저장 시스템과 나중에 맞춘다.
- overlap/hit 이벤트가 editor world에서도 발생할지 game world에서만 발생할지 B팀과 맞춰야 한다.

## 팀원에게 먼저 확인할 질문

- A: delegate handler 제거가 필요한가, 아니면 `Clear()`만 1차로 제공되는가?
- A: component event 시그니처는 Unreal식으로 갈 것인가, 단순화할 것인가?
- B: overlap event는 `World::Tick()`의 actor tick 전/후 중 어디에서 발생하는가?
- B: hit과 overlap을 같은 frame에 둘 다 발생시킬 수 있는가, block이면 hit만 발생시키는가?
- C: `LuaScriptComponent`의 실제 클래스명과 파일 위치는 무엇인가?
- C: script reload API 이름과 실패 로그 방식은 무엇인가?
- C: Lua에 노출되는 actor 변수 이름은 `obj`, `actor` 중 무엇인가?

## 내가 맡은 일의 최종 완료 기준

- 에디터에서 actor에 Lua script component를 추가할 수 있다.
- 선택한 Lua script component에서 script 생성, 편집, reload가 가능하다.
- Lua script에서 `BeginPlay`, `Tick`, `EndPlay`가 호출된다.
- Lua script에서 `wait(sec)`를 사용할 수 있고, coroutine이 tick 기반으로 재개된다.
- overlap 또는 hit 이벤트가 Lua 함수로 전달된다.
- 최종 샘플에서 overlap 발생 후 Lua coroutine으로 지연 처리되는 흐름을 보여줄 수 있다.
