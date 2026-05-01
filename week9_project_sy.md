# 발제 및 역할

- **Game Jam #3 - Make sure to delegate (확실히 위임하라)**게임 예시
    - 지금까지 제작된 Component들을 활용해 게임을 구현한다.
    - LuaJIT (Just-In-Time Compiler for Lua)을 엔진에 통합(Integration)한다.
    - 루아(Lua) 스크립트를 이해한다.
    - Component의 Hit, Overlap Event를 활용해 게임 로직을 구현한다.
    - 게임 룰, 로직은 최대한 루아 스크립트로 구현한다.
    - 게임의 시작과 끝, 재시작, 스코어는 반드시 있어야 한다.
    - Delegate와 Variadic Argument를 이해한다.
    - Coroutine을 구현한다.

- 기본 스크립트를 작성한다.
    - template.lua 이름으로 작성한다.
    - BeginPlay, EndPlay, Tick, OnOverlap 등의 공통 이벤트를 작성한다.
    
    ```lua
    function BeginPlay()
        print("[BeginPlay] " .. obj.UUID)
        obj:PrintLocation()
    end
    
    function EndPlay()
        print("[EndPlay] " .. obj.UUID)
        obj:PrintLocation()
    end
    
    function OnOverlap(OtherActor)
        OtherActor:PrintLocation();
    end
    
    function Tick(dt)
        obj.Location = obj.Location + obj.Velocity * dt
        obj:PrintLocation()
    end
    ```
    
- 스크립트를 생성한다.
    - 미리 제작된 template.lua를 액터가 배치된 Scene과 Actor 이름을 조합한 이름의 파일로 복제한다.
- 스크립트를 수정한다.
    - 스크립트 편집(Edit Script)을 눌러 *.lua와 연결된 텍스트 편집기로 스크립트를 편집한다.
    - Hot Reload를 지원한다.
    
    ```cpp
        LPCTSTR luaFilePath = _T("scene_name_actor_name.lua");
    
        // ShellExecute() -> Windows 확장자 연결(Association)에 따라 파일 열기
        HINSTANCE hInst = ShellExecute(
            NULL,            // 부모 윈도우 핸들 (NULL 사용 가능)
            _T("open"),      // 동작(Verb). "open"이면 등록된 기본 프로그램으로 열기
            luaFilePath,     // 열고자 하는 파일 경로
            NULL,            // 명령줄 인자 (필요 없다면 NULL)
            NULL,            // 작업 디렉터리 (필요 없다면 NULL)
            SW_SHOWNORMAL    // 열리는 창의 상태
        );
    
        // ShellExecute는 성공 시 32보다 큰 값을 반환합니다.
        // 실패 시 32 이하의 값이 반환되므로 간단히 체크 가능
        if ((INT_PTR)hInst <= 32) {
            MessageBox(NULL, _T("파일 열기에 실패했습니다."), _T("Error"), MB_OK | MB_ICONERROR);
        }
    ```
    

### **Engine Core (눈에 안 보이는 세상)**

- 스크립트를 실행한다.
    - Lua 스크립트를 실행한다.
    
    ```cpp
            class GameObject
            {
            public:
                uint32  UUID;
                FVector Location;
                FVector Velocity;
                void PrintLocation(){
                    ULog("Location %f %f %f\n", Location.x, Location.y, Location.z);
                }
            };
    
            sol::state lua;
            lua.open_libraries(sol::lib::base);
    
            lua.new_usertype<FVector>("Vector",
                sol::constructors<FVector(), FVector(float, float, float)>(),
                "x", &FVector::x,
                "y", &FVector::y,
                "z", &FVector::z,
                sol::meta_function::addition, [](const FVector& a, const FVector& b) { return FVector(a.x + b.x, a.y + b.y, a.z + b.z); },
                sol::meta_function::multiplication, [](const FVector& v, float f) { return v * f; }
            );
    
            lua.new_usertype<GameObject>("GameObject",
                "UUID", &GameObject::UUID,
                "Location", &GameObject::Location,
                "Velocity", &GameObject::Velocity,
                "PrintLocation", &GameObject::PrintLocation
            );
    
            GameObject obj;
            obj.Location = FVector(0, 0, 0);
            obj.Velocity = FVector(10, 0, 0);
    
            GameObject obj2;
            obj2.Location = FVector(0, 0, 0);
            obj2.Velocity = FVector(9, 9, 9);
    
            lua["obj"] = &obj;
    
            lua.script_file("script.lua");
    
            //lua.script(R"(
            //  function Tick(dt)
            //      obj.Location = obj.Location + obj.Velocity * dt
            //  end
            //)");
    
            lua["BeginPlay"]();
    
            // 테스트 루프
            for (int i = 0; i < 5; ++i) {
                lua["Tick"](0.1f); // delta time = 0.1
            }
    
            lua["OnOverlap"](obj2);
    
            lua["EndPlay"]();
    ```
    
- Coroutine을 구현한다.
    
    ```cpp
    function EnemyAI.start()
        print("[AI] Patrol start")
        for i = 1, 3 do
            move_to(10 * i, 0)
            wait_until_move_done()
            patrol_anim()
            wait(1.0)
        end
        print("[AI] Done")
    end
    ```
    
- Delegate를 구현한다.
    
    ```cpp
    #include <functional>
    #include <vector>
    
    template<typename... Args>
    class TDelegate
    {
    public:
        using HandlerType = std::function<void(Args...)>;
    
        // 일반 함수나 람다 등록
        void Add(const HandlerType& handler){
        }
    
        // 클래스 멤버 함수 바인딩
      template<typename T>
        void AddDynamic(T* Instance, void (T::*Func)(Args...)){
        }
    
        void Broadcast(Args... args){
        }
    
    private:
        std::vector<HandlerType> Handlers;
    };
    
    #define DECLARE_DELEGATE(Name, ...) TDelegate<__VA_ARGS__> Name
    DECLARE_DELEGATE(OnHealthChanged, int);
    DECLARE_DELEGATE(OnTakeDamage, int);
    
    class AActor : public UObject
    {
        FMySimpleDelegate OnTakeDamage;
    };
    
    void AAnotherActor::BeginPlay(){
        Super::BeginPlay();
    
        AMyActor* Target = ...;
    
        Target->OnTakeDamage.AddDynamic(this, &AAnotherActor::HandleDamage);
    }
    ```
    
- Box, Capsule, Sphere Component를 구현한다.
    
    ```cpp
    bool AActor::IsOverlappingActor(const AActor* Other) const{
        for (UActorComponent* OwnedComp : OwnedComponents)
        {
            if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnedComp))
            {
                if ((PrimComp->GetOverlapInfos().Num() > 0) && PrimComp->IsOverlappingActor(Other))
                {
                    // found one, finished
                    return true;
                }
            }
        }
        return false;
    }
    
    class UPrimitiveComponent
    {
        bool bGenerateOverlapEvents;
        bool bBlockComponent; // ComponentHit
    };
    
    class UShapeComponent : public UPrimitiveComponent
    {
        FColor ShapeColor;
        bool bDrawOnlyIfSelected;
    };
    
    class UBoxComponent : public UShapeComponent
    {
        FVector BoxExtent;
    };
    
    class USphereComponent : public UShapeComponent
    {
        float SphereRadius;
    };
    
    class UCapsuleComponent : public UShapeComponent
    {
        float CapsuleHalfHeight;
        float CapsuleRadius;
    };
    
    Box1->OnComponentHit.AddDynamic(this, &AMyActor::OnHit);
    Box1->OnComponentBeginOverlap.AddDynamic(this, &AMyActor::OnBeginOverlap);
    ```
    

# 역할 분담 - 나의 역할은 D

업무 분담

- **A: Delegate / Event 기반 담당**
    - `TDelegate<Args...>` 구현
    - `Add`, `AddDynamic`, `Remove/Clear` 가능하면 추가
    - `OnComponentBeginOverlap`, `OnComponentEndOverlap`, `OnComponentHit`의 공통 타입 정의
    - `AActor`, `UPrimitiveComponent`가 이벤트를 들고 있게 인터페이스 설계
    - 다른 팀원이 모두 의존하므로 제일 먼저 최소 API를 확정해야 합니다.
- **B: Collision Shape / Hit / Overlap 담당**
- `UShapeComponent`, `UBoxComponent`, `USphereComponent`, `UCapsuleComponent` 구현
- 기존
- PrimitiveComponent.h에 `bGenerateOverlapEvents`, `bBlockComponent` 계열 속성 추가
- WorldSpatialIndex
- 를 broad-phase로 활용
- 매 프레임 겹침 상태를 비교해서 `BeginOverlap / EndOverlap / Hit` 발생
- `AActor::IsOverlappingActor()` 같은 편의 함수 제공
- **C: Lua / Sol2 / ScriptComponent 담당**
- LuaJIT 또는 Lua + Sol2 빌드 통합
- `UScriptComponent` 또는 `ULuaScriptComponent` 구현
- `BeginPlay`, `Tick`, `EndPlay`, `OnOverlap`, `OnHit`을 Lua 함수로 호출
- `FVector`, `AActor`, `UObject UUID/Name`, Transform API 바인딩
- `template.lua` 생성, Actor별 `SceneName_ActorName.lua` 복제 규칙 구현
- Hot Reload 시 기존 Lua state를 교체하거나 스크립트만 재로드
- **D: Coroutine / Editor UX / Integration 담당**
- Lua coroutine scheduler 구현: `wait(sec)`, `yield`, `resume`, `StartCoroutine`
- Tick마다 대기 중인 coroutine을 깨우는 구조 작성
- Editor의 “Create Script / Edit Script / Reload Script” 버튼 연결
- `ShellExecute`로 `.lua` 열기
- 샘플 스크립트와 통합 테스트 씬 작성
- 마지막에 A/B/C 결과를 묶어서 실제 “Lua에서 Overlap 받고 coroutine으로 지연 처리”까지 검증

────────────────────────────────────────────────────────────

**팀원 A: Delegate / Event Contract (윤지)**

담당: C++ 이벤트 시스템의 공통 기반. 다른 팀원이 모두 의존하므로 가장 먼저 최소 API를 확정.

**수정/생성 파일:**

- 생성: Delegate.h
- 수정: CoreMinimal.h
- `Delegate.h`를 공통 include에 넣을지 결정
- 수정: CollisionTypes.h
- `FOverlapResult`, `FHitResult` 확장, 이벤트 인자 타입 정리
- 수정: PrimitiveComponent.h
- `OnComponentHit`, `OnComponentBeginOverlap`, `OnComponentEndOverlap` 선언만 추가

```
template<typename... Args>
class TDelegate
{
public:
    void Add(const HandlerType& Handler);
    template<typename T>
    void AddDynamic(T* Instance, void (T::*Func)(Args...));
    void Broadcast(Args... args);
    void Clear();
};
```

**팀원 B: Shape Component / Hit / Overlap (현길)**

담당: 실제 충돌 컴포넌트와 overlap/hit 판정. 이번 과제에서 엔진 기능의 물리적인 중심.

**수정/생성 파일:**

- 생성: ShapeComponent.h
- 생성: ShapeComponent.cpp
- 생성: BoxComponent.h
- 생성: BoxComponent.cpp
- 생성: SphereComponent.h
- 생성: SphereComponent.cpp
- 생성: CapsuleComponent.h
- 생성: CapsuleComponent.cpp
- 수정: PrimitiveComponent.h
- 수정: PrimitiveComponent.cpp
- 수정: World.h
- 수정: World.cpp
- 수정: AActor.h
- 수정: AActor.cpp

**구현 목표:**

- `bGenerateOverlapEvents`
- `bBlockComponent`
- `IsOverlappingActor`
- `GetOverlapInfos`
- `World::Tick()` 중 overlap pair 갱신
- 새로 겹치면 `BeginOverlap`, 빠지면 `EndOverlap`, block이면 `Hit`

기존 WorldSpatialIndex.cpp는 이미 BVH query가 있으니, B는 가능하면 이걸 활용하면 좋아.

**팀원 C: Lua / Sol2 / ScriptComponent (기훈)**

담당: Actor에 Lua 스크립트를 붙이고 lifecycle/event를 Lua 함수로 전달.

**수정/생성 파일:**

- 생성: LuaScriptComponent.h
- 생성: LuaScriptComponent.cpp
- 생성: LuaScriptSystem.h
- 생성: LuaScriptSystem.cpp
- 생성: LuaBindings.h
- 생성: LuaBindings.cpp
- 생성: template.lua
- 수정: ActorComponent.h
- 수정: ActorComponent.cpp

**구현 목표:**

- `BeginPlay`, `Tick`, `EndPlay`에서 Lua 함수 호출
- `OnOverlap`, `OnHit`을 Lua로 전달
- `FVector`, `AActor`, `UObject`, Transform API 바인딩
- Actor별 스크립트 경로 관리
- Hot Reload용 `ReloadScript()`

C는 B가 만든 delegate에 `LuaScriptComponent`가 구독하는 식으로 연결하면 깔끔해.

**팀원 D: Coroutine / Editor UX / Integration (세영)**

담당: Lua coroutine 실행 모델, 스크립트 생성/편집 버튼, 최종 통합 테스트.

수정/생성 파일:

- 생성: LuaCoroutineScheduler.h
- 생성: LuaCoroutineScheduler.cpp
- 수정: LuaScriptSystem.h
- 수정: LuaScriptSystem.cpp
- 수정: EditorPropertyWidget.cpp
- 수정: EditorComponentFactory.h
- 수정: EditorComponentFactory.cpp
- 생성 가능: ScriptUtils.h
- 생성 가능: ScriptUtils.cpp

**구현 목표:**

- Lua에서 `wait(1.0)` 가능
- `coroutine.resume/yield` 기반 Tick 스케줄링
- Property 창에서 `Create Script`, `Edit Script`, `Reload Script`
- `ShellExecute`로 `.lua` 열기
- `LuaScriptComponent`를 Add Component 메뉴에 등록
- 샘플 Lua로 전체 플로우 검증