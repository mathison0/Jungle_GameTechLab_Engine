# Cycle 11 진입 전 진단 — Container 도입 영향 + Mesh/Ribbon/Beam 통합 Plan

**작성일**: 2026-05-25
**대상 브랜치**: `feature/ParticleRender`
**모드**: diagnose only (코드 변경 0)
**baseline**: [ParticleEmitter_InfraCheck.md](ParticleEmitter_InfraCheck.md) (Cycle 8–10 진입 시점 진단)
**전제 (재논의 금지)**: TypeData 패턴 채택, `EParticleEmitterRenderMode` 라우팅 키, 공통 infra → Mesh → Ribbon → Beam 순서

---

## Part A. 재조사 보고

### A.1 Container 정체 식별

| 항목 | 결과 |
|---|---|
| 이름 + 위치 | `FParticleDataContainer` — [ParticleTypes.h:46-102](../JSEngine/Source/Engine/Particle/ParticleTypes.h:46) |
| 저장 단위 | **per-emitter (per-instance)** — `FParticleEmitterInstance` 멤버 [ParticleEmitterInstance.h:88](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h:88) `FParticleDataContainer ParticleStorage` |
| 저장 형식 | **type-erased byte buffer** — `uint8* ParticleData` + `uint16* ParticleIndices` 단일 블록 ([ParticleTypes.h:51-52](../JSEngine/Source/Engine/Particle/ParticleTypes.h:51)). POD struct array 아님 |
| 누가 owns | `FParticleEmitterInstance::ParticleStorage` 값 멤버 ([ParticleEmitterInstance.h:88](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h:88)). Component 아님 |
| 누가 reads/writes | writes: Init/Reset ([ParticleEmitterInstance.cpp:43, 70](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:43)), Spawn slot 채우기 ([cpp:175-177](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:175)), KillParticle swap-pop ([cpp:211](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:211)). reads: Tick GetParticle ([cpp:245, 258](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:245)), GetRuntimeView ([cpp:218-219](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:218)), BuildInstanceData ([cpp:323-327](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:323)) |

> **결론**: baseline §2-3 의 `ParticleData/ParticleIndices`/`ParticleStride` 슬롯이 **container로 부분 흡수**. Data/Indices는 container 내부, **Stride는 instance 멤버 잔존**. type-erased 바이트 버퍼이므로 type 분기는 container 외부 (BuildInstanceData/CollectPrimitive)에서 발생.

### A.2 EmitterInstance 측 변화 (baseline §2 대비)

| baseline 항목 | 변화 | 근거 |
|---|---|---|
| ParticleData / Indices | 흡수 — container 내부로 이동. 외부 노출은 getter 경유 | [ParticleEmitterInstance.h:57-58](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h:57) `GetParticleData/Indices() → ParticleStorage.*` |
| ParticleStride | 잔존 — instance 멤버, align만 적용 | [ParticleEmitterInstance.h:93](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h:93), [cpp:31](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:31) `AlignSize(ParticleSize, 16)` |
| InstanceData / PayloadSize / PayloadOffset | 잔존 — slot은 채워지나 Stride에 미반영 | [cpp:53-54](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:53) `InstancePayloadSize = PayloadBytes; PayloadOffset = ParticleSize` |
| Tick/Spawn/Kill virtual | 도입됨 — Cycle 9 결과 | [ParticleEmitterInstance.h:26, 28, 30](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h:26) `virtual` + 가상 소멸자 line 22 |
| BuildInstanceData virtual | 신규 — Cycle 10c | [ParticleEmitterInstance.h:39](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h:39) + base Sprite 본문 [cpp:314-339](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:314) |
| Get{Sprite,Mesh,Ribbon,Beam}*Data 4종 | 신규 — type별 명시 getter (사용자 결정 2) | [ParticleEmitterInstance.h:45-48](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h:45) |
| SpriteInstanceDataBuffer | 신규 — buffer 소유권 Component → Instance 이전 | [ParticleEmitterInstance.h:102](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.h:102) |
| 파생 클래스 생성 | TypeData->CreateInstance() hook 경유 | [ParticleSystemComponent.cpp:54-56](../JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp:54) `TypeData ? TypeData->CreateInstance(this, Index) : new FParticleEmitterInstance()` |

### A.3 ParticleModule 측 변화

| 항목 | 변화 | 근거 |
|---|---|---|
| `UParticleModuleTypeDataBase` 정의 | 도입 — bytes 단위 의미 | [ParticleModuleTypeData.h:13-28](../JSEngine/Source/Engine/Particle/ParticleModuleTypeData.h:13) `virtual int32 RequiredPayloadBytes() const { return 0; }` |
| `RequiredPayloadBytes()` 의미 | bytes (FBaseParticle 뒤 추가 byte 수). container 도입 후에도 동일 | [TypeData.h:20](../JSEngine/Source/Engine/Particle/ParticleModuleTypeData.h:20) 주석 |
| `CreateInstance()` 와 container 관계 | container 모름 — base 생성자가 container 초기화 책임 | [TypeData.cpp:10-15](../JSEngine/Source/Engine/Particle/ParticleModuleTypeData.cpp:10) `new FParticleEmitterInstance()` 만 호출 |
| `USpriteTypeData` 회귀 안전 | 유효 — `RequiredPayloadBytes() override = 0` | [TypeData.h:38](../JSEngine/Source/Engine/Particle/ParticleModuleTypeData.h:38) |
| `RequiredModule::RenderMode` 잔존 | 잔존 — UPROPERTY NoEdit, TypeData가 single source | [ParticleModules.h:58-59](../JSEngine/Source/Engine/Particle/ParticleModules.h:58) `Category="TypeData", NoEdit`, [ParticleSystem.cpp:57-68](../JSEngine/Source/Engine/Particle/ParticleSystem.cpp:57) `GetEffectiveRenderMode` |

### A.4 Builder / RenderPass / RenderCommand 측 변화

#### A.4.1 `CollectPrimitive` 함수 본문

| 항목 | 결과 | 근거 |
|---|---|---|
| 시그니처 + 위치 | `bool FPrimitiveDrawCommandBuilder::CollectPrimitive(UPrimitiveComponent*, FShowFlags&, EViewMode, FRenderBus&, FMeshBufferManager&) const` | [PrimitiveDrawCommandBuilder.cpp:227-681](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:227) |
| `case EPT_ParticleSystem` 위치 | line 565-676 | [Builder.cpp:565](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:565) |
| Sprite 분기 | 본문 보유 — `GetSpriteInstanceData → Cmd.ParticleInstances` + `VertexFactoryType = SpriteParticle` | [Builder.cpp:616-621](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:616) |
| Mesh 분기 | hook 보유, base nullptr fallback — `GetMeshInstanceData` + `MeshParticleInstances` 슬롯 | [Builder.cpp:622-627](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:622) |
| Ribbon 분기 | hook 보유, base nullptr fallback | [Builder.cpp:628-633](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:628) |
| Beam 분기 | hook 보유, base nullptr fallback | [Builder.cpp:634-639](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:634) |
| container 획득 경로 | **container 직접 미접근** — Instance virtual getter (`Get*InstanceData`)로만 회수, instance가 container를 caller로부터 은닉 | [Builder.cpp:617, 623, 629, 635](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:617) |
| Cmd 슬롯 전달 방식 | **포인터 전달 (제로 복사)** — instance buffer의 data() 포인터를 Cmd에 그대로 저장 | [Builder.cpp:617](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:617) `Cmd.ParticleInstances = Instance->GetSpriteInstanceData(Count)` |
| `return true` 종결 | 함수 끝 line 675 1회 + `bHasData` false 시 continue | [Builder.cpp:646-648, 675](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:646) — silent bug §7-5 준수 |
| `BuildSpriteInstanceData` 호출 위치 | **삭제됨** → `ParticleSystemComponent->BuildInstanceData()` (type-agnostic) | [Builder.cpp:579](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:579), [ParticleSystemComponent.cpp:207-216](../JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp:207) |

#### A.4.2 RenderPass / RenderCommand

| 항목 | 결과 | 근거 |
|---|---|---|
| RenderPass의 container 접근 | **container 미접근** — Cmd의 type별 슬롯만 read | [ParticleRenderPass.cpp:169-202](../JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp:169) `Cmd.ParticleInstances`만 사용 |
| type별 분기 위치 | **단일 Pass + 4-way switch** (Sprite/Mesh/Ribbon/Beam) | [ParticleRenderPass.cpp:124-145](../JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp:124) `switch(Cmd.VertexFactoryType)` |
| FRenderCommand 슬롯 선택 | **옵션 (i) 별도 슬롯 4종** (사용자 결정 2 확정) | [RenderCommand.h:491-505](../JSEngine/Source/Engine/Render/Scene/RenderCommand.h:491) `ParticleInstances / MeshParticleInstances / RibbonVertices / BeamVertices` |
| container → Cmd 변환 위치 | Instance의 `BuildInstanceData()` 가 container 순회 → 자기 buffer 채움, Builder가 buffer 포인터를 Cmd로 매핑 | [ParticleEmitterInstance.cpp:323-338](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:323), [Builder.cpp:617](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:617) |
| Mesh/Ribbon/Beam RenderPass 본문 | NOP — `RenderMeshEmitter/Ribbon/Beam` 비어 있음 | [ParticleRenderPass.cpp:253-278](../JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp:253) |
| FRenderCommand sizeof baseline | 464 bytes (Cycle 10a 추가 슬롯 포함) | [ParticleRenderPass.cpp:15](../JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp:15) `static_assert(sizeof(FRenderCommand) == 464, ...)` |

### A.5 §7 결정 1~5 재검토

| 결정 | 상태 | 사유 |
|---|---|---|
| 1: RequiredModule::RenderMode | **변경됨 (잔존+deprecate)** | UPROPERTY NoEdit ([ParticleModules.h:58](../JSEngine/Source/Engine/Particle/ParticleModules.h:58)) + `GetEffectiveRenderMode`에서 TypeData 우선 |
| 2: FRenderCommand 슬롯 | **확정 — 옵션 (i) 별도 슬롯** | [RenderCommand.h:497-505](../JSEngine/Source/Engine/Render/Scene/RenderCommand.h:497), generic void* 금지 명시 |
| 3: RenderPass 분리 | **확정 — 단일 Pass + 4-way switch** | [ParticleRenderPass.cpp:124-145](../JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp:124) |
| 4: Mesh payload 0 vs MeshRotation | **재논의 필요** — 현재 미결정. **ξ 충돌 (A.6) 와 결합 검토 필요** | hook은 0 기본 ([TypeData.h:20](../JSEngine/Source/Engine/Particle/ParticleModuleTypeData.h:20)) |
| 5: Beam Noise 포함 | **재논의 필요** | 변경 없음 |

### A.6 silent bug ι/κ/λ/μ + 신규 후보

| § | 항목 | 상태 | 근거 |
|---|---|---|---|
| ι | TypeDataModule UPROPERTY 누락 | **해소** | [ParticleSystem.h:46-47](../JSEngine/Source/Engine/Particle/ParticleSystem.h:46) UPROPERTY 마크 + 주석으로 명시 |
| κ | RenderMode vs TypeData 우선순위 | **해소** | `GetEffectiveRenderMode` TypeData 우선 + Required NoEdit |
| λ | BuildSpriteInstanceData 캐시 무효 flag | **미해결** — 매 frame 무조건 rebuild | [ParticleEmitterInstance.cpp:316-322](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:316) `clear() + reserve()` 매 호출 |
| μ | MeshBuffer cache UUID 충돌 | **미확인** — 본 진단 범위 외 (Cycle 11 진입 시 측정 필요) | — |
| **ν** (신규) | **container 이중 할당 + leak** | **위험: 높음** | [ParticleEmitterInstance.cpp:43-62](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:43) `ParticleStorage.Allocate()` (단일 블록 잡음) **직후** `ParticleStorage.ParticleData = new uint8[...]; ParticleStorage.ParticleIndices = new uint16[...]` 로 덮어씀 → Allocate가 잡은 블록 leak. Reset()은 `delete[] ParticleData`만 하고 `ParticleIndices` free 누락 — 매 Reset마다 추가 leak |
| **ξ** (신규) | **ParticleStride payload-aware 미반영** | **위험: 중간** (Sprite는 영향 0, Mesh/Ribbon/Beam payload>0 시 buffer overflow) | [ParticleEmitterInstance.cpp:31](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:31) `ParticleStride = AlignSize(ParticleSize, 16)` — `+ RequiredPayloadBytes()` 가산 누락. [cpp:53](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:53) `InstancePayloadSize = PayloadBytes` 채워지지만 Stride 계산에 미사용 → `ParticleData + SlotIndex * Stride` 가 payload 영역 침범 또는 잘림 |

---

## Part B. Mesh / Ribbon / Beam 3종 구현 Plan

### B.0 작성 원칙

- baseline §3 의 4-카테고리를 container 전제로 재작성
- 각 emitter = 별도 cycle, 단일 component / 단일 issue
- silent bug **ν / ξ** 해소를 어느 cycle에 끼울지 명시 (각 cycle별 회귀 안전 장치 항목)

### B.1 Mesh emitter plan (Cycle 11)

**목표**: payload 0 형식적 파생 + Mesh asset 1개 화면 표시.

**변경 대상 파일**
| path | 신규/수정 |
|---|---|
| `Engine/Particle/ParticleModuleTypeDataMesh.h/.cpp` | 신규 |
| `Engine/Particle/ParticleMeshEmitterInstance.h/.cpp` | 신규 (FParticleEmitterInstance 파생) |
| `Engine/Render/Resource/VertexTypes.h` | 수정 — `FMeshParticleInstanceData` 정의 추가 |
| `Engine/Render/Resource/VertexFactoryTypes.h` | 수정 — `MeshParticleLayout` + `MeshParticleDesc` 본문 채움 (현재 `EmptyParticleDesc` line 239) |
| `Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp` | 수정 — `RenderMeshEmitter` 본문 ([line 253-258](../JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp:253) NOP 교체) |
| `Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp` | 수정 — Mesh 분기 ([line 622-627](../JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp:622))에 Mesh asset 조회 + `Cmd.MeshBuffer` 채움 |
| `Shaders/Particle/MeshParticle.hlsl` | 신규 |
| `Engine/Render/Resource/ShaderPaths.h` | 수정 — `ParticleMesh` 경로 추가 |
| `JSEngine.vcxproj` + `.filters` | 수정 — 신규 .h/.cpp/.hlsl 등록 (silent bug §7-4) |

**container 상호작용**
- read: `FParticleMeshEmitterInstance::BuildInstanceData()` override가 base `ParticleStorage` + `ParticleStride` 그대로 사용 (`GetParticle(i)` loop)
- write: 없음 (Mesh는 spawn/kill 모두 base 동작 그대로)
- init: payload 0이면 container 초기화 base 그대로 OK
- handoff: derived가 `MeshInstanceDataBuffer` (자기 멤버) 를 채워 `GetMeshInstanceData()` override로 노출 → Builder가 `Cmd.MeshParticleInstances`로 매핑

**완료 기준**
- cube/sphere mesh asset 1개 + UMeshTypeData 로 교체한 ParticleSystem asset 빌드/실행 → 화면에 N개 mesh 표시
- RenderDoc: `DrawIndexedInstanced(IndexCount, ActiveParticles, 0, 0, 0)` event 발생, slot 0 mesh VB / slot 1 instance VB

**회귀 안전 장치**
- UMeshTypeData::RequiredPayloadBytes() = 0 유지 → **ξ 회피** (Sprite와 동일하게 payload 0이라 Stride 계산 영향 없음)
- USpriteTypeData가 변하지 않으므로 Sprite path 회귀 0
- VertexFactoryRegistry::Get의 MeshParticle case (현재 `EmptyParticleDesc` line 262)를 본문 채울 때만 교체 — Sprite case 미접근 (silent bug §7-1)

**silent bug 매칭**
| § | 충돌 | 해소 |
|---|---|---|
| §7-1 | MeshParticle case 본문 누락 시 EmptyParticleDesc → silent | 본 cycle에서 명시 채우기 |
| §7-4 | vcxproj 신규 파일 다수 (TypeData/Instance/VertexTypes/hlsl) | VS 닫고 작업 후 reload |
| §7-5 | EPT_ParticleSystem case `return true` | 이미 보장됨 (변경 0) |
| **ν** | Init 직후 container 덮어쓰기 | **본 cycle 전 또는 동시 해결 필수** — Mesh 본문 추가 전 base Init 패스에서 `ParticleStorage.Allocate()`만 사용하고 redundant `new uint8/uint16` 삭제, Reset에서 `ParticleStorage.Reset()`만 사용 |
| **ξ** | payload 0이면 noop | 차후 MeshRotation 도입 시 필요 — 본 cycle 결정 4 권고: 도입하면 **ξ 해소도 함께** (Stride에 `RequiredPayloadBytes()` 가산) |

### B.2 Ribbon emitter plan (Cycle 12a + 12b)

**목표**: trail 1개에 N개 particle linked-list 무결성 (12a) → strip 화면 표시 (12b).

#### Cycle 12a — payload + KillParticle override

**변경 대상 파일**
| path | 신규/수정 |
|---|---|
| `Engine/Particle/ParticleModuleTypeDataRibbon.h/.cpp` | 신규 |
| `Engine/Particle/ParticleRibbonEmitterInstance.h/.cpp` | 신규 |
| `Engine/Particle/ParticleTypes.h` 또는 `ParticleRibbonTypes.h` | 신규 `FRibbonParticlePayload` |
| `Engine/Particle/ParticleEmitterInstance.cpp` | 수정 — **ξ 해소 (Stride에 RequiredPayloadBytes() 가산)** 필수 선행 |

**container 상호작용**
- read: `GetParticle(ActiveIndex)`로 FBaseParticle 영역 + `(uint8*)Particle + PayloadOffset` 으로 RibbonPayload 영역 접근. **payload는 container 내부에 stride 단위로 인터리브** (UE Cascade 패턴)
- write: Spawn override가 새 slot의 payload 초기화 (NextIndex/PrevIndex/Tangent...)
- init: container.Allocate 그대로 사용. 단 ParticleStride에 RibbonPayload bytes 반드시 가산되어야 함 (**ξ 해소 선행**)
- handoff: 12b에서 trail 순회 결과를 `RibbonVertexBuffer` (instance 멤버)에 unroll → `GetRibbonVertexData()` 노출

**완료 기준 (12a)**
- 디버거 watch: trail 1개에 5개 particle spawn → linked list traversal로 head→tail 5회 hop 가능
- KillParticle (중간 노드) 후에도 linked list 무결성 (head→tail traversal 정상 + ActiveParticles 정확히 감소)

**회귀 안전 장치 (12a)**
- 12a는 렌더 없음 — Sprite path 변경 0, 화면 결과 동일
- RibbonPayload는 **물리 SlotIndex (container의 ParticleData 위치)** 를 저장. swap-pop은 `ParticleIndices`만 swap하고 SlotIndex 불변이므로 link 안전
- KillParticle override는 base swap-pop 호출 전에 NextIndex/PrevIndex 재연결

**silent bug 매칭 (12a)**
| § | 충돌 | 해소 |
|---|---|---|
| baseline §4.1 | swap-pop vs linked list | RibbonPayload가 SlotIndex 저장, swap이 link 안 깨뜨림 — 검증을 디버거 watch로 |
| **ξ** | Stride에 payload 가산 누락 → buffer overflow | **본 cycle에 ξ 해소 작업 포함** (Mesh가 payload 0으로 회피한 항목, Ribbon은 회피 불가) |
| **ν** | leak | 12a 진입 전 해결 필요 (Cycle 11과 동시 또는 별도 fix cycle) |

#### Cycle 12b — strip VB + Pass 분기

**변경 대상 파일**
| path | 신규/수정 |
|---|---|
| `Engine/Render/Resource/VertexTypes.h` | 수정 — `FRibbonParticleVertex` 정의 |
| `Engine/Render/Resource/VertexFactoryTypes.h` | 수정 — `RibbonParticleLayout` + `RibbonParticleDesc` 본문 |
| `Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp` | 수정 — `RenderRibbonEmitter` 본문 ([line 263-268](../JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp:263)), TRIANGLESTRIP topology 분기 |
| `Engine/Particle/ParticleRibbonEmitterInstance.cpp` | 수정 — `BuildInstanceData()` override 추가, dynamic VB 채움 |
| `Shaders/Particle/RibbonParticle.hlsl` | 신규 |

**container 상호작용**: BuildInstanceData에서 HeadIndices → NextIndex traversal → vertex unroll. container는 read-only (BuildInstanceData는 const 데이터 view).

**완료 기준 (12b)**: trail 1개에 strip 표시 (단순 material). RenderDoc Draw(VertexCount) 발생, topology TRIANGLESTRIP.

**회귀 안전 장치 (12b)**
- Pass의 `IASetPrimitiveTopology` Begin에서 TRIANGLELIST (line 95) 박혀 있음 → Ribbon 분기에서 명시 변경 + Sprite 분기 진입 시 복귀 필수
- `bAnySpriteRendered`처럼 `bAnyRibbonRendered` 플래그 추가 또는 Ribbon helper 끝에서 topology 복귀

**silent bug 매칭 (12b)**
| § | 충돌 |
|---|---|
| §7-1 | RibbonParticle Desc 본문 누락 시 silent — 명시 case 본문 채움 |
| §7-4 | vcxproj 신규 파일 |

### B.3 Beam emitter plan (Cycle 13a + 13b)

**목표**: Source→Target beam 1개 (13a) → Noise 적용 strip 표시 (13b).

#### Cycle 13a — Beam payload + Tick override + Source/Target 모듈

**변경 대상 파일**
| path | 신규/수정 |
|---|---|
| `Engine/Particle/ParticleModuleTypeDataBeam2.h/.cpp` | 신규 |
| `Engine/Particle/ParticleBeamEmitterInstance.h/.cpp` | 신규 |
| `Engine/Particle/ParticleModuleBeamSource.h/.cpp` | 신규 |
| `Engine/Particle/ParticleModuleBeamTarget.h/.cpp` | 신규 |
| `Engine/Particle/ParticleTypes.h` 또는 `ParticleBeamTypes.h` | 신규 `FBeamParticlePayload` |

**container 상호작용**
- read: BeamPayload는 container 내부 stride 단위. base `GetParticle(i)` + offset access
- write: Spawn override가 SourcePoint/TargetPoint payload 초기화. Tick override가 매 frame Source/Target lookup 결과로 payload 갱신
- init: container.Allocate 그대로, **ξ 해소 선행 전제** (Stride에 BeamPayload bytes 가산)
- Tick 의미 변경: base Tick의 `RelativeTime >= 1.0f → KillParticle` 로직 우회. Beam derived Tick는 base 호출 안 하거나 base 호출 후 다시 살리는 패턴. [추측: 별도 SpawnRate 무시하고 매 frame Source/Target lookup이 자연스러움]

**완료 기준 (13a)**: 디버거 — Source actor 이동 시 BeamPayload.SourcePoint 자동 추적. ActiveParticles는 일정 (RelativeTime 사망 우회).

**회귀 안전 장치 (13a)**: Beam 전용 instance만 Tick override, base/Sprite/Mesh/Ribbon 영향 0.

**silent bug 매칭 (13a)**
| § | 충돌 |
|---|---|
| **ξ** | Stride 가산 — Cycle 12a에서 이미 해소되었으면 자동 OK |
| Tick 의미 분기 | base Tick의 SpawnModule 호출 ([ParticleEmitterInstance.cpp:101-103](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:101)) 가 Beam에서 의미 다름 — derived Tick에서 override 명시 |

#### Cycle 13b — strip VB + Noise + Pass 분기

**변경 대상 파일**
| path | 신규/수정 |
|---|---|
| `Engine/Render/Resource/VertexTypes.h` | 수정 — `FBeamParticleVertex` |
| `Engine/Render/Resource/VertexFactoryTypes.h` | 수정 — `BeamParticleLayout/Desc` 본문 |
| `Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp` | 수정 — `RenderBeamEmitter` 본문 |
| `Engine/Particle/ParticleBeamEmitterInstance.cpp` | 수정 — `BuildInstanceData()` override, dynamic VB |
| `Engine/Particle/ParticleModuleBeamNoise.h/.cpp` | 신규 (결정 5에 따라 본 cycle 또는 별도) |
| `Shaders/Particle/BeamParticle.hlsl` | 신규 |

**container 상호작용**: BuildInstanceData에서 Beam payload별 Source→Target 보간 + Noise 적용 → vertex unroll. container read-only.

**완료 기준 (13b)**: 두 점 간 beam + (Noise 포함 시) 시각 검증.

**회귀 안전 장치 (13b)**: Ribbon과 동일 — topology 분기 시 Sprite 진입 전 복귀.

**silent bug 매칭 (13b)**
| § | 충돌 |
|---|---|
| §7-1 | BeamParticle Desc 본문 |
| Noise 시점 | 결정 5 — 본 cycle 포함 vs 별도 |

### B.4 cycle 간 의존성 / 순서

| 항목 | 권고 |
|---|---|
| 기존 순서 (Mesh → Ribbon → Beam) | **유지** — container 도입이 instance virtual hook을 이미 마련해두어 Mesh는 더 단순해짐 |
| **선행 fix cycle 신설 권고** | **silent bug ν (leak) + ξ (Stride payload-aware)** 를 Cycle 11 진입 전 별도 fix cycle (Cycle 10d?)로 분리. 이유: Mesh는 payload 0이라 ν/ξ를 회피할 수 있으나, Ribbon/Beam 진입 시 반드시 필요. 별도 cycle로 분리하면 회귀 격리 가능 |
| 대안 | ν만 Cycle 11에 끼우고 ξ는 Cycle 12a 진입 시 — Mesh가 payload 0 유지 결정 시 가능 |

### B.5 진단의 한계 / 추측 영역 (baseline §6 대비)

**해소** (코드 확인으로 명확해진 것)
- 추측 2 부분 (Ribbon KillParticle): Spawn 패턴 [ParticleEmitterInstance.cpp:175-177](../JSEngine/Source/Engine/Particle/ParticleEmitterInstance.cpp:175) 가 `SlotIndex = ParticleIndices[ActiveIndex]` 후 `ParticleData + SlotIndex * Stride` 로 접근 → payload는 SlotIndex 저장이 안전. swap-pop은 active list 순서만 변경
- 추측 4 (RenderPass 분리): 단일 Pass + procedural switch로 확정 ([ParticleRenderPass.cpp:124-145](../JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.cpp:124))
- 추측 5 (FRenderCommand 옵션): (i) 별도 슬롯 확정 + sizeof=464 baseline 측정됨

**추측 잔존** (여전히 코드 확인 외 영역)
- 추측 1 (MeshRotation 필요성): 결정 4와 동일
- 추측 3 (Beam Source/Target): 단순 component world location vs actor reference vs 별도 추상 — 미결정
- 추측 6 (MeshBuffer cache UUID 충돌): μ 본 진단 범위 외
- 추측 7 (Noise 데이터 모델): 결정 5와 동일

**신규 추측 (container 도입으로 발생)**
- container의 `MemBlockSize` 단일 블록과 Ribbon free-list 관리의 호환성 — **ν 해소 시 함께 검토 필요**. 현재는 ν 자체가 버그라 container의 단일 블록 설계가 의도대로 활용되고 있지 않음
- BuildInstanceData가 매 frame 전체 재구축 (λ 미해결): emitter 수 × frame 수 누적 비용 — Mesh/Ribbon/Beam이 진입하면 비용 증가 가시화 [추측]

---

## Part C. 다음 cycle 진입 결정 항목 (사용자 영역)

- **(결정 4) Mesh의 payload 0 vs MeshRotation 도입 (초기)** — 선택지 (A) payload 0 (가장 단순, ξ 회피) / (B) MeshRotation ~36B (3축 회전 + ξ 해소 동시 진행). **본 진단의 권고**: (A) — Cycle 11을 형식적 파생만으로 마무리, ξ는 Cycle 12a에 통합
- **(결정 5) Beam Noise를 Phase 4 cycle 13b에 포함 vs 별도 cycle** — 선택지 (A) 13b 포함 / (B) 13c로 분리. **본 진단의 권고**: (B) — 13b는 단순 Source→Target strip만 완료 후 Noise는 별도 cycle (단일 issue 단위 원칙)
- **(결정 6, 신규) silent bug ν (container 이중 할당 + leak) 해소 시점** — 선택지 (A) Cycle 11 안에 끼움 / (B) 별도 fix cycle (Cycle 10d) 신설 / (C) Mesh가 payload 0이므로 ν 무시하고 Cycle 12a 진입 시 ξ와 함께. **본 진단의 권고**: (B) — leak은 Sprite path에도 매 Reset마다 발생 중 (silent), Mesh 진입 전 격리 fix가 가장 안전
- **(결정 7, 신규) silent bug ξ (Stride payload-aware) 해소 시점** — 선택지 (A) Cycle 10d에 ν와 동시 / (B) Cycle 12a 진입 시 / (C) Cycle 11에 끼움. **본 진단의 권고**: (B) — Mesh가 payload 0 유지 시 ξ가 Sprite/Mesh에 영향 0이라 미루어도 됨. Ribbon 진입 시점이 자연스러움
- **(결정 8, 신규) BuildInstanceData 매 frame rebuild (silent bug λ) 캐시화 시점** — 선택지 (A) Cycle 11–13 진입 전 / (B) 3종 emitter 완료 후 별도 최적화 cycle / (C) 무시. **본 진단의 권고**: (B) — 본 도입 cycle에서는 정확성 우선, 캐시는 측정 후

---

## 결론 한 줄

> Cycle 8–10 완료로 **container (FParticleDataContainer) + TypeData + Instance virtual + 4-way 명시 슬롯 + 4-way Pass switch** 의 골격이 정확히 자리잡았다. Cycle 11 (Mesh) 는 payload 0 형식적 파생으로 거의 끝낼 수 있으나, **신규로 식별된 silent bug ν (container 이중 할당 leak) 와 ξ (Stride payload-aware 누락)** 은 늦어도 Ribbon 진입 (Cycle 12a) 전에 해소되어야 한다. baseline §6 추측 중 2/4/5가 코드 확인으로 해소되었고, ν/ξ 가 신규 발생한 잠재 위험이다.
