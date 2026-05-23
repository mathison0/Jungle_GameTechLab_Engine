# Particle Class Ownership / Reference Map

`JSEngine/Source/Engine/Particle/` 내부 클래스들의 **소유(owns) / 참조(refs / weak)** 관계를 `UParticleSystemComponent`를 진입점으로 정리한다.

- **owns** : 해당 객체의 lifetime을 책임진다 (raw `new`/`delete`, value 멤버, `UPROPERTY`로 묶여 직렬화/GC 등 UObject 관리).
- **refs** : 다른 객체가 소유한 인스턴스를 단순히 가리킨다 (back-pointer, lookup 캐시, 행위 호출용).
- 화살표 방향은 **소유/참조하는 쪽 → 대상**.

> 파일 위치: 모든 클래스는 [JSEngine/Source/Engine/Particle/](JSEngine/Source/Engine/Particle/) 아래에 있다.

---

## 1. 파일별 클래스 인벤토리

| 파일 | 정의된 타입 | 종류 |
|------|------------|------|
| [ParticleTypes.h](JSEngine/Source/Engine/Particle/ParticleTypes.h) | `EParticleEmitterRenderMode`, `FBaseParticle`, `FParticleDataContainer`, `FParticleEventCollideData` | enum / POD struct |
| [ParticleHelper.h](JSEngine/Source/Engine/Particle/ParticleHelper.h) | `PARTICLE_PTR` 매크로, `GetParticleDirect()` | 인라인 헬퍼 (현재 외부 참조 없음) |
| [ParticleModule.h/.cpp](JSEngine/Source/Engine/Particle/ParticleModule.h) | `UParticleModule` | UObject (모듈 베이스) |
| [ParticleModules.h/.cpp](JSEngine/Source/Engine/Particle/ParticleModules.h) | `UParticleModuleRequired`, `UParticleModuleSpawn`, `UParticleModuleLifetime`, `UParticleModuleLocation`, `UParticleModuleVelocity`, `UParticleModuleColor`, `UParticleModuleSize`, `UParticleModuleCollision`, `UParticleModuleEventGenerator` | UObject (구체 모듈) |
| [ParticleSystem.h/.cpp](JSEngine/Source/Engine/Particle/ParticleSystem.h) | `UParticleLODLevel`, `UParticleEmitter`, `UParticleSystem` | UObject (에셋 트리) |
| [ParticleEmitterInstance.h/.cpp](JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h) | `FParticleEmitterInstance` | POD struct (런타임 인스턴스) |
| [ParticleSystemComponent.h/.cpp](JSEngine/Source/Engine/Particle/ParticleSystemComponent.h) | `UParticleSystemComponent`, `FOnParticleCollide` 델리게이트 | UComponent (씬 진입점) |
| [ParticleEvent.h/.cpp](JSEngine/Source/Engine/Particle/ParticleEvent.h) | `AParticleEventManager`, `FOnParticleEventCollide` 델리게이트 | AActor (씬 placeable 이벤트 허브) |

### 1.1 정의 없이 선언만 존재하는 타입 (stub)

| 타입 | 선언 위치 | 비고 |
|------|----------|------|
| `UParticleModuleTypeDataBase` | [ParticleSystem.h:6](JSEngine/Source/Engine/Particle/ParticleSystem.h:6) forward 선언 → `UParticleLODLevel::TypeDataModule` 멤버에서 사용 | 정의 파일이 디렉토리 내 없음. 비소유 포인터로만 들고 있음. |
| `FParticleEventInstancePayload` | [ParticleEmitterInstance.h:36](JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h:36) forward 선언 → `SpawnParticles(..., EventPayload)` 시그니처에만 등장 | 정의 없음. 호출부에서 `nullptr`로만 전달됨 (이벤트 기반 스폰을 위한 미래용 슬롯). |

---

## 2. 상속 (Inheritance)

```
UObject
  ├── UParticleModule
  │     ├── UParticleModuleRequired       (bSpawnModule=true; Required defaults)
  │     ├── UParticleModuleSpawn          (Rate 기반 ComputeSpawnCount)
  │     ├── UParticleModuleLifetime       (bSpawnModule=true)
  │     ├── UParticleModuleLocation       (bSpawnModule=true)
  │     ├── UParticleModuleVelocity       (bSpawnModule=true)
  │     ├── UParticleModuleColor          (bSpawnModule=true; bUpdateModule=true)
  │     ├── UParticleModuleSize           (bSpawnModule=true; bUpdateModule=true)
  │     ├── UParticleModuleCollision      (bUpdateModule=true; OnHit → Component.QueueCollisionEvent)
  │     └── UParticleModuleEventGenerator (bUpdateModule=true; → Component.DispatchQueuedParticleEvents)
  ├── UParticleLODLevel
  ├── UParticleEmitter
  └── UParticleSystem

UPrimitiveComponent
  └── UParticleSystemComponent

AActor
  └── AParticleEventManager
```

`UParticleModule`의 가상 메서드는 `Spawn(Owner, Particle, SpawnTime)` / `Update(Owner, DeltaTime)` 두 개. 각 파생 모듈이 이 둘 중 하나 또는 모두 오버라이드한다.

---

## 3. 소유/참조 전체 그래프 (`UParticleSystemComponent` 진입)

```
┌────────────────────────────────────────────────────────────────────────┐
│ UParticleSystemComponent  (extends UPrimitiveComponent)                │
│                                                                        │
│  owns  ──► UParticleSystem*  Template            (UPROPERTY, 에셋 ref) │
│  owns  ──► TArray<FParticleEmitterInstance*> EmitterInstances          │
│              (raw `new` / `delete`로 직접 lifetime 관리)               │
│  owns  ──► TArray<FParticleEventCollideData> PendingCollisionEvents    │
│  owns  ──► FOnParticleCollide OnParticleCollide  (델리게이트)          │
└────────────────────────────────────────────────────────────────────────┘
        │
        │ Template (UPROPERTY)
        ▼
┌──────────────────────────────────────────────────────────────┐
│ UParticleSystem  (extends UObject)                           │
│  owns ──► TArray<UParticleEmitter*> Emitters   (UPROPERTY)   │
└──────────────────────────────────────────────────────────────┘
        │ Emitters[i]
        ▼
┌──────────────────────────────────────────────────────────────┐
│ UParticleEmitter  (extends UObject)                          │
│  owns ──► TArray<UParticleLODLevel*> LODLevels (UPROPERTY)   │
│  values: ParticleSize, MaxActiveParticles (캐시)             │
└──────────────────────────────────────────────────────────────┘
        │ LODLevels[i]
        ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│ UParticleLODLevel  (extends UObject)                                         │
│  owns ──► UParticleModuleRequired* RequiredModule   (UPROPERTY)              │
│  owns ──► TArray<UParticleModule*> Modules          (UPROPERTY)              │
│                                                                              │
│  refs ──► UParticleModuleSpawn*    SpawnModule      (Modules의 부분 집합 캐시)│
│  refs ──► TArray<UParticleModule*> SpawnModules     (RequiredModule + 일부)  │
│  refs ──► TArray<UParticleModule*> UpdateModules    (Modules의 부분 집합)    │
│  refs ──► UParticleModuleTypeDataBase* TypeDataModule (stub, 비소유)         │
└──────────────────────────────────────────────────────────────────────────────┘
        ▲
        │ refs (Modules / RequiredModule)
        │
┌──────────────────────────────────────────────────────────────┐
│ UParticleModule  (베이스, extends UObject)                   │
│  values: bEnabled, bSpawnModule, bUpdateModule  (플래그)     │
│  virtual Spawn(Owner, Particle, SpawnTime)                   │
│  virtual Update(Owner, DeltaTime)                            │
└──────────────────────────────────────────────────────────────┘
        ▲
        │ extends
        │
   (Required / Spawn / Lifetime / Location / Velocity / Color / Size /
    Collision / EventGenerator) — 모두 value 멤버만, 추가 소유 없음
```

위 트리는 **에셋 측 트리** (`UParticleSystem` 루트, UObject로 구성).

### 3.1 런타임 인스턴스 측

`UParticleSystemComponent::EmitterInstances`가 가지는 `FParticleEmitterInstance`는 에셋 트리를 **참조**하면서 자신만의 GPU/CPU 버퍼를 **소유**한다.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ FParticleEmitterInstance  (POD struct, NOT a UObject)                        │
│                                                                              │
│  refs ──► UParticleEmitter*           SpriteTemplate   (Init 시 set)         │
│  refs ──► UParticleSystemComponent*   Component        (소유주 역참조)       │
│  refs ──► UParticleLODLevel*          CurrentLODLevel  (SpriteTemplate에서   │
│                                                          lookup 캐시)        │
│                                                                              │
│  owns ──► uint8*  ParticleData     (new uint8[ParticleStride * Max])         │
│  owns ──► uint16* ParticleIndices  (new uint16[Max])                         │
│  owns ──► uint8*  InstanceData     (Reset 시점에 delete[]만, 본문에서        │
│                                       new 하는 곳은 없음 — 미래 슬롯)       │
│                                                                              │
│  values: EmitterIndex, CurrentLODLevelIndex,                                 │
│          InstancePayloadSize, PayloadOffset, ParticleSize, ParticleStride,   │
│          ActiveParticles, ParticleCounter, MaxActiveParticles, SpawnFraction │
└──────────────────────────────────────────────────────────────────────────────┘
```

- `ParticleData`는 `FBaseParticle`을 `ParticleStride` 간격으로 연속 배치한 raw 버퍼.
- `ParticleIndices`는 `[0..Max)` 인덱스를 담은 **간접 테이블** — `KillParticle`이 `swap`만 하고 ActiveParticles만 감소시켜 compact한 active 리스트를 유지한다.

### 3.2 이벤트 측

```
┌──────────────────────────────────────────────────────────────┐
│ FParticleEventCollideData  (POD struct in ParticleTypes.h)   │
│                                                              │
│  refs ──► UParticleSystemComponent*   Component              │
│  refs ──► FParticleEmitterInstance*   EmitterInstance        │
│  refs ──► UPrimitiveComponent*        HitComponent  (*)      │
│  refs ──► AActor*                     HitActor      (*)      │
│  values: EmitterIndex, ParticleId, Location, OldLocation,    │
│          Velocity, Normal, Time, FHitResult Hit              │
└──────────────────────────────────────────────────────────────┘
(*) HitComponent / HitActor는 구조체에 슬롯만 존재. 현재 디렉토리 코드의
    UParticleModuleCollision::Update는 이 두 필드를 채우지 않는다 (단순 평면 충돌).
```

```
┌──────────────────────────────────────────────────────────────┐
│ AParticleEventManager  (extends AActor)                      │
│  owns ──► TArray<FParticleEventCollideData> CollisionEvents  │
│  owns ──► FOnParticleEventCollide OnParticleCollide          │
└──────────────────────────────────────────────────────────────┘
```

**현재 코드 상태:** `AParticleEventManager`는 `UParticleSystemComponent`나 모듈로부터 **참조되지 않는 독립 액터**다 (Particle/ 외부 검색 시 `.vcxproj` 외에는 나오지 않는다). 즉 컴포넌트 단위의 이벤트 큐(`UParticleSystemComponent::PendingCollisionEvents`)와는 **별개의 병렬 허브**로 존재한다.

---

## 4. 관계 표 (요약)

### 4.1 소유 관계

| 소유자 | 소유 대상 | 멤버명 | 메커니즘 |
|--------|-----------|--------|----------|
| `UParticleSystemComponent` | `UParticleSystem` (참조) | `Template` | `UPROPERTY` 포인터 (에셋 ref) |
| `UParticleSystemComponent` | `FParticleEmitterInstance` 인스턴스들 | `EmitterInstances` | `new` / `delete` (raw heap) |
| `UParticleSystemComponent` | `FParticleEventCollideData` | `PendingCollisionEvents` | value `TArray` |
| `UParticleSystemComponent` | `FOnParticleCollide` | `OnParticleCollide` | value 델리게이트 |
| `UParticleSystem` | `UParticleEmitter` | `Emitters` | `UPROPERTY` `TArray` |
| `UParticleEmitter` | `UParticleLODLevel` | `LODLevels` | `UPROPERTY` `TArray` |
| `UParticleLODLevel` | `UParticleModuleRequired` | `RequiredModule` | `UPROPERTY` |
| `UParticleLODLevel` | `UParticleModule` 들 | `Modules` | `UPROPERTY` `TArray` |
| `FParticleEmitterInstance` | particle raw 메모리 | `ParticleData`, `ParticleIndices`, `InstanceData` | `new[]` / `delete[]` |
| `AParticleEventManager` | `FParticleEventCollideData` | `CollisionEvents` | value `TArray` |
| `AParticleEventManager` | `FOnParticleEventCollide` | `OnParticleCollide` | value 델리게이트 |

### 4.2 비소유 참조 관계 (back-pointer / 캐시 / 행위 호출)

| 참조 보유자 | 참조 대상 | 멤버명 | 용도 |
|-------------|-----------|--------|------|
| `UParticleLODLevel` | `UParticleModuleSpawn` | `SpawnModule` | `Modules` 안에서 캐스팅 캐시 |
| `UParticleLODLevel` | `UParticleModule` (subset) | `SpawnModules` | `Modules` 중 `IsSpawnModule()` 필터 캐시 |
| `UParticleLODLevel` | `UParticleModule` (subset) | `UpdateModules` | `Modules` 중 `IsUpdateModule()` 필터 캐시 |
| `UParticleLODLevel` | `UParticleModuleTypeDataBase` (stub) | `TypeDataModule` | 미래용 슬롯, 비소유 |
| `FParticleEmitterInstance` | `UParticleEmitter` | `SpriteTemplate` | 에셋 lookup |
| `FParticleEmitterInstance` | `UParticleSystemComponent` | `Component` | 소유자 역참조 (World Location / 이벤트 큐) |
| `FParticleEmitterInstance` | `UParticleLODLevel` | `CurrentLODLevel` | LOD 캐시, `SpriteTemplate->GetLODLevel(...)` 결과 |
| `FParticleEventCollideData` | `UParticleSystemComponent`, `FParticleEmitterInstance`, `UPrimitiveComponent`, `AActor` | (각 필드) | 이벤트 컨텍스트 |

---

## 5. 행위적(behavioral) 호출 관계

소유/참조와 별개로, **누가 누구의 메서드를 호출하는가**:

| 호출자 | 피호출자 / 멤버 | 위치 |
|--------|-----------------|------|
| `UParticleSystemComponent::SetTemplate` | `RecreateEmitterInstances` → `new FParticleEmitterInstance` + `Instance->Init(...)` | [ParticleSystemComponent.cpp:30](JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp:30) |
| `FParticleEmitterInstance::Init` | `SpriteTemplate->CacheEmitterModuleInfo()`, `SelectLODLevel(0)`, `GetLODLevel(...)` | [ParticleEmitterInstance.cpp:18](JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:18) |
| `UParticleEmitter::CacheEmitterModuleInfo` | `UParticleLODLevel::CacheModuleLists` (모든 LOD) | [ParticleSystem.cpp:46](JSEngine/Source/Engine/Particle/ParticleSystem.cpp:46) |
| `UParticleSystemComponent::TickComponent` | `Instance->Tick(DeltaTime)` 루프 + `NotifySpatialIndexDirty` | [ParticleSystemComponent.cpp:141](JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp:141) |
| `FParticleEmitterInstance::Tick` | `Component->ComputeEmitterLODDistance()`, `SelectLODLevel`, `CurrentLODLevel->GetSpawnModule()->ComputeSpawnCount`, `SpawnParticles`, `KillParticle`, 각 `UpdateModule->Update(this, dt)` | [ParticleEmitterInstance.cpp:76](JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:76) |
| `FParticleEmitterInstance::SpawnParticles` | 각 `SpawnModule->Spawn(this, *Particle, SpawnTime)` | [ParticleEmitterInstance.cpp:152](JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:152) |
| `UParticleModuleLocation::Spawn` | `Owner->Component->GetWorldLocation()` | [ParticleModules.cpp:107](JSEngine/Source/Engine/Particle/ParticleModules.cpp:107) |
| `UParticleModuleCollision::Update` | `Owner->Component->QueueCollisionEvent(Event)` + `Owner->KillParticle(i)` | [ParticleModules.cpp:213](JSEngine/Source/Engine/Particle/ParticleModules.cpp:213) |
| `UParticleModuleEventGenerator::Update` | `Owner->Component->DispatchQueuedParticleEvents()` | [ParticleModules.cpp:273](JSEngine/Source/Engine/Particle/ParticleModules.cpp:273) |
| `UParticleSystemComponent::DispatchQueuedParticleEvents` | `OnParticleCollide.Broadcast(EventData)` 후 큐 clear | [ParticleSystemComponent.cpp:88](JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp:88) |
| `UParticleSystemComponent::ComputeEmitterLODDistance` | `GetOwner()->GetFocusedWorld()->GetActiveCamera()->GetLocation()` (외부 의존) | [ParticleSystemComponent.cpp:64](JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp:64) |
| `AParticleEventManager::DispatchEvents` | `OnParticleCollide.Broadcast` 후 큐 clear | [ParticleEvent.cpp:15](JSEngine/Source/Engine/Particle/ParticleEvent.cpp:15) |

### 5.1 데이터 흐름 (한 프레임)

```
UEngine Tick
  └─ UParticleSystemComponent::TickComponent(dt)
       └─ for each FParticleEmitterInstance* in EmitterInstances:
            Instance->Tick(dt)
              ├─ Component->ComputeEmitterLODDistance()   ← World/Camera 외부 의존
              ├─ SelectLODLevel(distance)                  → CurrentLODLevel 갱신
              ├─ SpawnModule->ComputeSpawnCount(this, dt) → SpawnCount
              ├─ SpawnParticles(SpawnCount, ...)
              │    └─ for each SpawnModule in CurrentLODLevel->GetSpawnModules():
              │         Module->Spawn(this, particle, t)   ← Required/Lifetime/Location/Velocity/Color/Size 초기화
              ├─ active particle 루프:
              │    RelativeTime/Location 업데이트, RelativeTime≥1 → KillParticle (swap)
              └─ for each UpdateModule in CurrentLODLevel->GetUpdateModules():
                   Module->Update(this, dt)
                   ├─ ColorModule.Update / SizeModule.Update : 모든 active particle 보간
                   ├─ CollisionModule.Update : 평면 통과 → Component->QueueCollisionEvent + (옵션) KillParticle
                   └─ EventGenerator.Update : Component->DispatchQueuedParticleEvents → OnParticleCollide.Broadcast
```

---

## 6. 외부 의존성 (Particle/ 디렉토리 밖)

| 외부 타입 | 사용 위치 | 관계 종류 |
|-----------|----------|----------|
| `UPrimitiveComponent` | `UParticleSystemComponent`의 부모 | 상속 |
| `UObject` | `UParticleSystem`/`Emitter`/`LODLevel`/`Module` 베이스 | 상속 |
| `AActor` | `AParticleEventManager`의 부모, `FParticleEventCollideData::HitActor` 슬롯 | 상속 / 참조 |
| `UWorld`, `FViewportCamera`, `GetFocusedWorld()` | `UParticleSystemComponent::ComputeEmitterLODDistance` | 일시 참조 (호출 후 폐기) |
| `FEngineRandom` | `ParticleModules.cpp`의 `RandomRange` 헬퍼 | 정적 의존 |
| `FHitResult`, `CollisionTypes` | `FParticleEventCollideData::Hit` | value 멤버 |
| `FName` | `UParticleModuleRequired::SubUVName` | value 멤버 |
| `FVector`, `FColor`, `FBoundingBox`, `FRay` | 전반 (Math/Core) | value/인자 |
| `DECLARE_DELEGATE` 매크로 | `FOnParticleCollide`, `FOnParticleEventCollide` | 델리게이트 정의 |

---

## 7. 주의/관찰 사항

1. **이중 이벤트 허브** — `UParticleSystemComponent::OnParticleCollide`(컴포넌트 단위)와 `AParticleEventManager::OnParticleCollide`(레벨 단위)가 별개로 존재하지만, 현재 코드에선 후자에 push 하는 경로가 디렉토리 내에 **없다**. `AParticleEventManager`는 placeable 액터로 선언만 되어 있는 상태.
2. **`ParticleHelper.h` 미사용** — `PARTICLE_PTR` 매크로/`GetParticleDirect` 인라인은 디렉토리 내·외 어디서도 include 되지 않는다 (현재 `FParticleEmitterInstance::GetParticle`이 동일 계산을 직접 수행).
3. **Forward-only 타입** — `UParticleModuleTypeDataBase`, `FParticleEventInstancePayload`는 정의가 어디에도 없고 슬롯만 존재 (Mesh/Beam/Ribbon 등 비-Sprite 렌더 모드 + 이벤트 기반 스폰을 위한 placeholder).
4. **`FParticleEmitterInstance`는 UObject가 아니다** — `UParticleSystemComponent`가 `new`/`delete`로 직접 lifetime 관리. UObject GC 경로 밖.
5. **`UParticleLODLevel`의 캐시 멤버** — `SpawnModule`, `SpawnModules`, `UpdateModules`는 `Modules` (+`RequiredModule`)의 부분 뷰. `CacheModuleLists()` 호출 전에는 비어 있다. `UParticleEmitter::CacheEmitterModuleInfo`가 모든 LOD에 대해 일괄 호출 (FParticleEmitterInstance::Init 시점).
6. **컴포넌트 ↔ 인스턴스 양방향 참조** — `UParticleSystemComponent → EmitterInstances*` (소유), `FParticleEmitterInstance → Component` (back-ref). 컴포넌트 소멸 시 `ClearEmitterInstances`에서 인스턴스가 모두 delete 되므로 dangling은 발생하지 않음.
7. **`FParticleEventCollideData.HitComponent / HitActor`** — 구조체에 슬롯만 있고 현재 collision 모듈은 값을 채우지 않는다 (단순 Z 평면 충돌). 외부에서 일반 충돌 시스템과 연결될 때 채워질 슬롯.

---

## 8. 한 줄 요약

> `UParticleSystemComponent`가 **에셋 트리(`UParticleSystem → UParticleEmitter → UParticleLODLevel → UParticleModule`)** 를 `Template`으로 참조하면서, 각 `UParticleEmitter`마다 **런타임 `FParticleEmitterInstance`** 를 `new`로 소유한다. 인스턴스는 에셋 트리(SpriteTemplate, CurrentLODLevel)와 컴포넌트(Component)를 **back-ref**로 들고, 자기 자신의 particle raw 버퍼만 소유한다. 모듈은 인스턴스(`Owner`)를 통해 다시 컴포넌트로 들어가 충돌/이벤트를 큐잉하고, 컴포넌트가 `OnParticleCollide` 델리게이트로 외부에 broadcast 한다.
