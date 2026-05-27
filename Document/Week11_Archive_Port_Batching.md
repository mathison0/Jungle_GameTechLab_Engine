# Week11 Archive Port Batching

## 2026-05-27 Continuation Batch Status

Scope: continue Week11 asset policy parity after the main 5-batch pass, excluding Blueprint.

Completed portions: 12 / 12 in the current continuation pass (100%).

Completed continuation batches:

- Batch 6: audited remaining parity gaps. The largest non-BP gap is SkeletalMesh/AnimSequence `.uasset` parity; current project still has FBX/cache and `.animseq` descriptors in that area.
- Batch 7: added `USkeletalMesh` `.uasset` runtime load support through `FAssetFile`, `FArchive`, and `FSkeletalMesh::Serialize`.
- Batch 8: connected save/package policy for `USkeletalMesh` `.uasset`, including `FArchive` `FRotator` support for socket rotation payloads.
- Batch 9: added `UAnimSequence` `.uasset` Archive payload support and routed new FBX animation stack imports to `Asset/Animation/*.uasset`.
- Batch 10: tightened editor UX paths so animation `.uasset` assets open in the animation viewer, show details previews, and legacy `.animseq` descriptors are only used as notify-copy compatibility when reimporting.
- Batch 11: scanned legacy animation assets and added a safe lazy migration bridge for old `.animseq` references with missing Bin caches.
- Batch 12: ran final smoke checks for Debug build, `.uasset` metadata parsing, and legacy extension counts.

What changed in this continuation pass:

- `FSkeletalMesh`, `FSkeletalMeshVertex`, `FBoneInfo`, and `FSkeletalMeshSocket` now have Archive serialization operators.
- `FSkeletalMeshLoadService` now routes `.uasset` paths through `FAssetFile::Load` and validates the metadata class as `USkeletalMesh`.
- `FResourceManager::SaveSkeletalMesh` now follows Week11 policy: only `.uasset` skeletal mesh assets are writable, and saves payload through `FAssetFile`.
- `GamePackager` runtime `.uasset` class allow-list now includes `USkeletalMesh`.
- `FArchive` now has default `FRotator` serialization as Pitch/Yaw/Roll, matching the Week11 socket payload flow.
- `UAnimDataModel`, raw animation tracks, `FFrameRate`, `FQuat` keys, bone tracks, and flat notifies now serialize through `FArchive`.
- `FResourceManager` can discover, load, and save `UAnimSequence` `.uasset` files while leaving `.animseq` as a temporary compatibility path.
- Content Browser/viewer tab routing now treats `UAnimSequence` `.uasset` metadata as an animation viewer target.
- `GamePackager` runtime `.uasset` class allow-list now also includes `UAnimSequence` and follows the target skeletal mesh dependency from the animation payload.
- Content Browser details can inspect `UAnimSequence` `.uasset` metadata, not just temporary `.animseq` descriptors.
- FBX animation reimport no longer keeps emitting `.animseq` just because a legacy descriptor exists; it writes `.uasset` and copies notifies forward.
- Direct legacy `.animseq` loads now rebuild through FBX and resolve to the generated `.uasset` path instead of reloading the empty descriptor.
- Legacy scan result: 47 `.animseq` descriptors remain under `Asset/Animation`, and their referenced `Asset/Animation/Bin/*.animseq.bin` cache files are not present. They should not be deleted until editor/runtime smoke has generated their `.uasset` replacements or animation graph references are rewritten.

Validation:

- `MSBuild Debug|x64`: warnings 0, errors 0
- `.uasset` metadata parse: total 335, errors 0
- Legacy asset file count after the broad migration: `.mat` 0, `.matinst` 0, `.curve` 0, `.particlesystem` 0, `.layout` 0, `.sequence` 0.
- Remaining animation legacy descriptors: `.animseq` 47. These have no matching `Asset/Animation/Bin` cache in the current checkout and are intentionally bridged instead of blindly deleted.
- Remaining asset references to `.animseq`: 6 animation graph references in `TopDownStateMachine.animgraph` and `TopDownBlending.animgraph`; they are covered by the lazy rebuild bridge until generated `.uasset` replacements exist.
- `MSBuild GameClientDebug|x64`: blocked by the local environment duplicate `Path`/`PATH` issue in MSBuild/CL process startup, not by compile diagnostics. Error: MSB6001, duplicate key `Path`/`PATH`.

Remaining recommended batches:

- Manual smoke still recommended: launch editor, open one legacy `.animseq` animation graph path once, confirm `.uasset` generation under `Asset/Animation`, then rewrite those graph references and delete the 47 legacy descriptors.

## 2026-05-27 Final Batch Status

Scope: align Week12 asset/content policies to Week11, excluding Blueprint.

Completed portions: 5 / 5 (100%).

What changed in the final policy pass:

- Content Browser now treats `.uasset` metadata as the first-class source for tile labels, badges, thumbnails, double-click routes, and drag payload types.
- Drag-and-drop payloads now match the metadata-backed asset class route: Material, StaticMesh, Curve, ParticleSystem, RuntimeUILayout, and supported legacy source files where still valid.
- Runtime UI designer now saves `.uasset` as the single layout asset format. The old `.layout` text sync path, buttons, source metadata, drag-drop bypass, and creation undo entry were removed.
- AssetQuery now includes `UStaticMesh` `.uasset` files in static mesh lookup results.
- GamePackager now accepts runtime `UStaticMesh` `.uasset` metadata and follows `.obj/.bin` source dependencies when present.
- BP routes were intentionally not ported.

Validation:

- Legacy asset files: `.mat`, `.matinst`, `.curve`, `.particlesystem`, `.layout` = 0
- Legacy source references: `.mat`, `.matinst`, `.curve`, `.particlesystem`, `.layout`, `RUIL`, `RuntimeUILayoutBinaryMagic`, `LoadLegacy`, `SaveToTextLayout`, `LoadFromTextLayout` = 0
- `.uasset` metadata parse: total 335, errors 0
- Metadata classes: `UMaterial` 281, `UMaterialInstance` 43, `UCurveFloatAsset` 8, `ParticleSystem` 2, `UStaticMesh` 1
- `Scripts/CheckArchitecture.ps1`: violations 0
- `MSBuild Debug|x64`: warnings 0, errors 0
- `MSBuild GameClientDebug|x64`: warnings 0, errors 0

작성일: 2026-05-27

## 2026-05-27 최신 상태: Week11 Asset 정책 유사도

현재 방향은 Week11의 Asset 정책과 같은 축으로 맞춰가고 있다. 단순히 확장자만 `.uasset`으로 바꾼 MVP가 아니라, `FAssetFile` 공통 헤더/메타데이터, `ClassName` 기반 식별, metadata-only scan, payload별 `FArchive` 직렬화 흐름을 기준으로 정리했다.

이번 추가 보강:

- Content Browser가 Week11처럼 디렉터리 scan 시 `.uasset` metadata를 `FContentItem`에 캐시한다.
- Material/Curve/ParticleSystem/Runtime UI 판정이 반복 `LoadMetadataOnly` 대신 캐시된 metadata를 우선 사용한다.
- StaticMesh `.uasset` load route를 추가했다.
- `FStaticMesh::Serialize(FArchive&, PayloadVersion)`를 추가해서 Week11의 StaticMesh payload를 읽을 수 있게 했다.
- Material Preview와 Content Browser thumbnail용 PreviewSphere를 `Asset/Mesh/PreviewSphere.uasset`로 전환했다.
- ResourceManager discovery가 `UStaticMesh`/`USkeletalMesh` `.uasset` metadata도 식별한다.

현재 `.uasset` metadata 검증 결과:

| ClassName | Count |
| --- | ---: |
| `UMaterial` | 281 |
| `UMaterialInstance` | 43 |
| `UCurveFloatAsset` | 8 |
| `ParticleSystem` | 2 |
| `UStaticMesh` | 1 |

검증:

- legacy asset 실물: `.mat`, `.matinst`, `.curve`, `.particlesystem`, `.layout` 잔존 0개
- legacy loader 심볼: `RUIL`, `RuntimeUILayoutBinaryMagic`, `LoadLegacy` 잔존 0개
- `.uasset` metadata parse: total 335, errors 0
- `Scripts/CheckArchitecture.ps1`: violations 0
- `MSBuild Debug|x64`: warning 0, error 0
- `MSBuild GameClientDebug|x64`: warning 0, error 0

아직 Week11 대비 남은 큰 차이:

- Blueprint는 사용자 요청대로 제외했다.
- 전체 mesh import 결과를 모두 `.uasset` source asset으로 강제하는 단계는 아직 남아 있다. 다만 StaticMesh `.uasset` 로더와 PreviewSphere `.uasset`는 들어갔기 때문에 썸네일/프리뷰 체감 경로는 Week11 쪽에 가까워졌다.
- scene/prefab 자체의 binary 전환은 아직 하지 않았다. 현재는 내부 참조만 `.uasset`로 rewrite한 상태다.

## 결론

Week11에서 말하는 Archive 개념은 두 층으로 나뉜다.

1. `FArchive` 기반 직렬화 인터페이스
2. `.uasset` 파일을 공통 헤더/메타데이터/페이로드로 감싸는 `FAssetFile` 패키지 계층

현재 Week12는 이미 1번은 상당 부분 가지고 있다. 오히려 Week12의 `FArchive`는 `IObjectReferenceResolver`까지 있어서 Week11보다 일부 확장되어 있다.

초기 부족분은 2번이었다. 현재는 Runtime UI Editor의 전용 `RUIL` fallback을 제거하고 `FAssetFile` 공통 헤더/메타데이터/페이로드 정책으로 이전한 상태다.

구현 방향은 reflection-first로 잡는다.

- Archive 구현체는 파일/메모리/JSON 같은 저장 매체만 담당한다.
- 에셋 payload는 가능하면 `UObject::Serialize(FArchive&)`를 호출한다.
- `UObject::Serialize` 내부의 `SerializeProperties`가 `UClass::GetAllProperties`와 `FProperty::SerializeItem`을 통해 `UPROPERTY` 필드를 자동 직렬화한다.
- 에셋별 수동 직렬화는 reflection이 표현하지 못하는 runtime cache, curve map, object graph root id, post-load rebuild 정도로 제한한다.
- 이렇게 해야 Details Panel, Undo snapshot, Asset 저장이 같은 reflection metadata를 공유한다.

추천 배치 수:

- 최소 목표: Runtime UI Editor를 Week11식 Archive 패키지로 정리 = 3배치
- 추천 목표: Blueprint 제외, 주요 에셋까지 Archive 패키지화 = 5배치
- 확장 목표: Content Browser/GamePackager/AssetQuery까지 완성도 있게 통합 = 6배치

이번 프로젝트 흐름상 추천은 5배치 + 선택 1배치다.

## 현재 Week12 상태

이미 있는 것:

- `JSEngine/Source/Engine/Serialization/Archive.h`
- `JsonReader`, `JsonWriter`
- `MemoryWriter`
- `UObject::Serialize(FArchive&)`
- `UObject::SerializeProperties(FArchive&)`
- 여러 Component/Particle/Animation 계열의 `Serialize(FArchive&)`
- Runtime UI layout 전용 binary `.uasset` 저장/로드

현재 Runtime UI 전용 처리:

- 파일: `JSEngine/Source/Engine/UI/RuntimeUILayoutAsset.cpp`
- 내부 클래스: `FRuntimeUILayoutBinaryArchive`
- magic: `RUIL`
- 파일 버전: `RuntimeUILayoutBinaryFileVersion`
- 페이로드 버전: `URuntimeUILayoutAsset::CurrentPayloadVersion`

이 방식의 문제:

- `.uasset`이지만 Runtime UI만 읽을 수 있는 사설 포맷이다.
- Content Browser가 `RUIL` magic을 직접 확인해야 한다.
- 다른 에셋 타입과 메타데이터 정책을 공유하지 않는다.
- GamePackager/AssetQuery/ResourceManager가 공통 metadata 기반으로 판단하기 어렵다.

## Week11에서 가져올 핵심

Week11 핵심 파일:

- `Engine/Serialization/WindowsBinReader.h/.cpp`
- `Engine/Serialization/WindowsBinWriter.h/.cpp`
- `Engine/Asset/AssetHeader.h/.cpp`
- `Engine/Asset/AssetMetaData.h/.cpp`
- `Engine/Asset/AssetFile.h/.cpp`

Week11 구조:

```cpp
FAssetHeader Header;
FAssetMetaData MetaData;
Payload.Serialize(FArchive&);
```

파일 구조:

```text
.uasset
  FAssetHeader
    Magic
    Version
  FAssetMetaData
    Version
    PayloadVersion
    AssetGuid
    ClassName
    DisplayName
    SourceFile
  Payload
    asset-specific Serialize(FArchive&)
```

주의점:

- Week11 `FArchive`를 그대로 덮으면 안 된다.
- Week12 `FArchive`의 object resolver 기능을 유지해야 한다.
- Week11 binary archive에는 `FRotator`, `FQuat` 연산자가 있지만 현재 Week12 `FArchive` 인터페이스에는 없다. 필요한 타입만 현재 인터페이스에 맞춰 추가하거나, payload가 요구하는 경우에만 확장한다.
- Blueprint는 이번 범위에서 제외한다.

## Batch 1: Core Binary Archive

목표:

- Runtime UI 전용 binary archive를 공용 파일 archive로 승격한다.
- Week11 `FWindowsBinReader/Writer`를 Week12 `FArchive` 인터페이스에 맞춰 이식한다.

작업:

- `JSEngine/Source/Engine/Serialization/WindowsBinReader.*` 추가
- `JSEngine/Source/Engine/Serialization/WindowsBinWriter.*` 추가
- `FArchive`의 현재 인터페이스와 맞춤
- string length guard, array count guard 유지
- `FVector2`, `FVector`, `FVector4`, `FColor`, `FMatrix`, `FName`, primitive round-trip 확인

검증:

- Editor 빌드
- GameClient 빌드
- 작은 round-trip smoke test 또는 임시 에셋 저장/로드 확인

위험도:

- 낮음~중간
- 파일 IO와 primitive serialization이므로 범위가 작다.

## Batch 2: Asset Package Layer

목표:

- Week11의 `.uasset` 공통 컨테이너를 Week12에 도입한다.

작업:

- `JSEngine/Source/Engine/Asset/AssetHeader.*` 추가
- `JSEngine/Source/Engine/Asset/AssetMetaData.*` 추가
- `JSEngine/Source/Engine/Asset/AssetFile.*` 추가
- `FAssetFile::Save`
- `FAssetFile::Load`
- `FAssetFile::LoadMetadataOnly`
- 기존 `FPaths`/project-relative path 정책에 맞춤

검증:

- metadata-only load가 payload를 읽지 않고 성공하는지 확인
- invalid magic/version 방어 확인
- `.uasset` 확장자 검사 확인

위험도:

- 중간
- 파일 포맷의 기준점이 생기므로 이후 배치의 기반이 된다.

## Batch 3: Runtime UI Editor Migration

목표:

- 현재 Runtime UI 전용 `RUIL` binary archive를 Week11식 `FAssetFile` 기반으로 교체한다.
- UI Editor `.uasset` 저장/로드를 공통 Archive 패키지에 태운다.

작업:

- `URuntimeUILayoutAsset::SaveToFile`에서 `FAssetFile::Save` 사용
- `URuntimeUILayoutAsset::LoadFromFile`에서 `FAssetFile::Load` 사용
- metadata:
  - `ClassName = "RuntimeUILayout"`
  - `DisplayName`
  - `PayloadVersion = URuntimeUILayoutAsset::CurrentPayloadVersion`
  - 기존 asset guid 유지 가능하면 유지
- 기존 `RUIL` 포맷 fallback loader 유지 여부 결정
  - 추천: 최소 1회 호환 로더 유지
  - 이유: 이미 만들어진 Runtime UI `.uasset`이 깨지지 않게 하기 위해
- Content Browser의 `HasRuntimeUILayoutBinaryMagic`를 metadata 기반 판별로 변경

검증:

- Runtime UI Designer에서 새 `.uasset` 생성
- 저장 후 재시작/재로드
- `.layout` sync/export 유지
- Content Browser double click route 확인

위험도:

- 중간
- 현재 UI Editor가 이미 전용 바이너리를 사용하므로 변경 지점은 명확하지만, 기존 파일 호환성 처리가 중요하다.

## Batch 4: ParticleSystem / Material / Curve / Animation Selected Assets

목표:

- Blueprint를 제외하고 실효성이 큰 에셋 타입을 공통 Archive 패키지로 맞춘다.

우선순위:

1. ParticleSystem
2. Material
3. Curve
4. AnimSequence 또는 Animation State Machine
5. StaticMesh/SkeletalMesh metadata-only path

ParticleSystem 현재 상태:

- 현재 경로는 `.particlesystem` JSON 파일이다.
- `FResourceManager::LoadParticleSystem`, `SaveParticleSystem`이 JSON object graph를 만든다.
- object graph 내부는 `FJsonReader/FJsonWriter` + `IObjectReferenceResolver` + `UObject::Serialize`를 사용한다.
- `UParticleSystem`, `UParticleEmitter`, `UParticleLODLevel`, `UParticleModule` 계열은 `UPROPERTY` 기반 reflection 직렬화 대상이다.
- `UParticleModule::Serialize`는 `UObject::Serialize` 후 distribution runtime data와 curve payload를 수동으로 추가 저장한다.

ParticleSystem 적용 방향:

- 처음부터 `.particlesystem`을 삭제하지 않는다.
- 신규 저장은 `.uasset` + `FAssetFile`로 지원한다.
- 기존 `.particlesystem`은 fallback import/load로 유지한다.
- metadata:
  - `ClassName = "ParticleSystem"`
  - `DisplayName`
  - `PayloadVersion`
  - `SourceFile`은 기존 `.particlesystem`에서 변환한 경우 기록
- payload는 가능하면 현재 object graph serialization을 binary `FArchive`로 태운다.
- load 후 `CacheEmitterModuleInfo`, `Validate` smoke test를 반드시 호출한다.

작업:

- ParticleSystem `.uasset` save/load route 추가
- 기존 `.particlesystem` JSON load fallback 유지
- `FAssetQueryService::GetParticleSystemPaths`가 `.particlesystem`과 `ClassName == "ParticleSystem"`인 `.uasset`을 함께 반환하도록 변경
- Content Browser create/open/drop route가 양쪽을 처리하도록 변경
- Week11 `MaterialSerializationService`의 `FAssetFile` 사용 지점을 현재 Week12 material 정책에 맞춰 재검토
- 현재 Week12 material 수정 가능 정책과 충돌하지 않도록 save/load만 이식
- Curve/Animation은 현재 Week12에 실제 editor workflow가 연결된 타입부터 적용
- mesh는 payload 전체 이식보다 metadata 표준화와 loader route부터 접근

검증:

- ParticleSystem Editor에서 새 asset 생성, 저장, 재로드
- sprite/mesh/ribbon/beam 기본 system smoke test
- module reorder/add/delete 후 저장/재로드
- distribution curve 저장/재로드
- Content Browser에서 material/mesh thumbnail 유지
- material 편집 후 저장/재로드
- 기존 non-uasset asset path fallback 유지
- GameClient 빌드

위험도:

- 중간~높음
- Material은 이미 최근에 정책을 건드렸기 때문에 충돌 가능성이 있다.
- Mesh payload까지 한 번에 바꾸면 import/cache/loading 이슈가 커진다.

## Batch 5: Content Browser / Asset Query / Resource Loading Integration

목표:

- `.uasset`을 확장자만이 아니라 metadata로 식별하게 한다.

작업:

- Content Browser item scan 시 `FAssetFile::LoadMetadataOnly` 적용
- `.uasset` preview/type label/icon/thumbnail route를 `ClassName` 기반으로 정리
- `AssetQueryService` 또는 현재 동등 기능에 metadata query 추가
- ResourceManager에서 `.uasset` metadata 기반 route 보강
- ParticleSystem은 `.particlesystem`과 `.uasset`을 모두 노출하되, 신규 생성은 `.uasset`을 우선한다.
- unknown `.uasset` 경고/Toast 정리

검증:

- texture/material/mesh 많은 폴더 진입 시 불필요한 payload load가 없는지 확인
- metadata-only scan이 thumbnail generation을 막지 않는지 확인
- double click/open route 확인

위험도:

- 중간
- 최근 사용자가 말한 Content Browser buffering 문제와 직접 연결된다. metadata-only scan을 제대로 쓰면 개선 여지가 있다.

## Optional Batch 6: GamePackager Integration

목표:

- 패키징이 `.uasset` metadata를 보고 필요한 runtime asset을 더 안정적으로 복사하게 한다.

작업:

- GamePackager의 `.uasset` asset class allow-list 정리
- Runtime UI layout, particle system, material, curve, animation asset copy 정책 정리
- source file dependency가 metadata에 있으면 packaging diagnostics에 표시
- legacy 확장자 fallback은 유지하되 신규 `.uasset` 경로를 우선

검증:

- package run
- packaged runtime에서 material/mesh/UI layout 로드
- 누락 asset 메시지가 Toast/log로 정확히 표시되는지 확인

위험도:

- 중간
- 패키징은 파일 복사 범위가 넓어서 별도 배치로 빼는 편이 안전하다.

## 보류 권장

이번 Archive 포트에서 바로 하지 않는 것:

- Blueprint `.uasset`
- 모든 scene/prefab binary 전환
- mesh payload 포맷 전면 교체
- 기존 legacy 확장자 즉시 제거
- Week11 `FArchive` 원본으로 Week12 `FArchive` 덮어쓰기

이유:

- Blueprint는 사용자가 제외했다.
- scene/prefab binary 전환은 저장 호환성 리스크가 크다.
- mesh payload 전환은 import/cache/loading 정책까지 흔든다.
- 팀원이 `.uasset` 통합에 반대하는 상황이면, 기존 확장자를 즉시 없애는 것보다 metadata 기반 `.uasset` 병행 도입이 설득하기 쉽다.

## 팀 설득 포인트

`.uasset` 통합을 강하게 밀기보다, 먼저 다음 장점으로 설득하는 편이 좋다.

- 확장자가 중구난방이어도 내부 metadata로 타입을 판별할 수 있다.
- Content Browser가 payload를 매번 열지 않고 metadata-only scan을 할 수 있다.
- GamePackager가 파일명/확장자 추측 대신 asset class를 보고 복사할 수 있다.
- Runtime UI처럼 이미 `.uasset`을 쓰는 기능부터 공통화하면 리스크가 낮다.
- legacy 확장자는 당장 제거하지 않고 fallback으로 둔다.

즉, 메시지는 "전부 갈아엎자"가 아니라 "새 에셋부터 공통 헤더를 달고, 기존 에셋은 읽을 수 있게 두자"가 더 낫다.

## 최종 추천 진행 순서

1. Batch 1: Core Binary Archive
2. Batch 2: Asset Package Layer
3. Batch 3: Runtime UI Editor Migration
4. Batch 5: Content Browser metadata integration
5. Batch 4: ParticleSystem/Material/Curve/Animation selected assets
6. Optional Batch 6: GamePackager integration

순서를 이렇게 바꾸는 이유:

- Runtime UI는 이미 `.uasset` 저장이 있어서 가장 좋은 첫 적용 대상이다.
- Content Browser metadata integration은 현재 buffering 문제와도 연결된다.
- Material/mesh 계열은 정책 충돌 가능성이 있어 Runtime UI 검증 후 들어가는 편이 안전하다.

## 예상 단계 수 요약

| 목표 | 배치 수 | 설명 |
| --- | ---: | --- |
| Runtime UI만 Week11식 Archive 패키지화 | 3 | Core binary archive, asset package, UI migration |
| Blueprint 제외 주요 에셋까지 공통화 | 5 | UI 이후 Content Browser와 ParticleSystem/Material/Curve/Animation 일부 적용 |
| 패키징까지 정리 | 6 | GamePackager copy/diagnostic 정책 포함 |

## 2026-05-27 진행 기록

- Batch 1 완료: `FWindowsBinReader/Writer` 공용 binary archive 추가.
- Batch 2 완료: `FAssetHeader`, `FAssetMetaData`, `FAssetFile` 공용 `.uasset` 컨테이너 추가.
- Batch 3 완료: Runtime UI Layout 저장/로드를 `FAssetFile` 기반으로 이전하고 기존 `RUIL` payload fallback 유지.
- Batch 4 완료 쪽으로 전환:
  - ParticleSystem 신규 저장/로드는 `.uasset` object graph로 고정.
  - Material/MaterialInstance 신규 저장 경로는 `.uasset`으로 고정.
  - CurveFloatAsset 신규 저장/로드는 `.uasset`으로 고정.
- Batch 5 완료 쪽으로 전환:
  - Content Browser와 AssetQuery는 `.uasset` metadata `ClassName` 기반으로 Runtime UI, ParticleSystem, Material, Curve를 노출.
  - `.mat`, `.matinst`, `.curve`, `.particlesystem`, `.layout`은 Content Browser asset type 노출에서 제외.
- Optional Batch 6 일부 완료:
  - GamePackager가 `.uasset` metadata `ClassName` allow-list로 Runtime UI Layout, ParticleSystem, Material, MaterialInstance, Curve를 런타임 복사 대상으로 식별.
  - Material/MaterialInstance `.uasset` payload에서 parent material과 texture parameter dependency를 따라가도록 연결.
  - 기존 `.mat`, `.matinst`, `.curve`, `.particlesystem` 전체 복사는 제거.
  - 기존 `.mat`, `.matinst`, `.prefab`, `.particlesystem` JSON dependency scan은 직접 참조된 파일을 위한 임시 브릿지로만 유지.
  - unknown `.uasset`은 확장자만 보고 무조건 복사하지 않고 metadata 확인 후 스킵.
- 의도적으로 보류:
  - 기존 `.mat`, `.matinst`, `.curve` 확장자 제거.
  - Mesh payload 전면 `.uasset` 전환.
  - Blueprint.
  - 실제 package run과 packaged runtime 로드 확인.

## 나중에 반드시 정리할 저장 경로 / BinarySerializer 이슈

현재 Archive 패키지 도입과 별개로 `ResourceManager`에는 기존 mesh 전용 binary/cache 흐름이 남아 있다.

- `FBinarySerializer`는 StaticMesh/SkeletalMesh cooked/cache 전용 포맷이다.
- 주요 경로:
  - `Asset/Mesh/Bin/*.bin`
  - `Asset/Cooked/Mesh/*.bin`
  - source 옆 sibling `.bin`
  - `Asset/Material/Auto/*.mat`, `*.matinst`
- 지금 추가한 `FAssetFile` `.uasset`은 에셋 컨테이너이고, 기존 `FBinarySerializer`는 runtime/cooked mesh payload cache에 가깝다.
- 따라서 당장 하나로 섞기보다 다음 단계에서 역할을 분리해서 정리해야 한다.
  - source asset: 사람이 편집/관리하는 `.uasset`, `.mat`, `.curve`, `.particlesystem`
  - cooked/cache asset: 빌드/런타임 성능용 `.bin`
  - package asset: GamePackager가 복사하거나 굽는 최종 runtime 파일
- 후속 작업:
  - `AssetPathPolicy`에 cooked/cache/source 경로 정책을 한 곳으로 모으기.
  - `GamePackager.cpp` 안의 `MakeCookedMeshRelativePath`, `MakeStaticMeshCacheBinaryPath` 중복을 `AssetPathPolicy`로 이전.
  - `ResourceManager`의 `BinarySerializer` 호출부와 `StaticMeshLoadService`/`SkeletalMeshLoadService` 경로 정책을 문서화.
  - mesh를 `.uasset` payload로 전면 이전할지, cooked `.bin`을 계속 별도 유지할지 팀 합의 후 결정.
  - `Asset/Material/Auto` 자동 생성 material 경로가 source asset인지 import cache인지 정책 확정.

## 2026-05-27 legacy 제거 전환 상태

- 새로 만드는 Material, MaterialInstance, Curve, ParticleSystem은 `.uasset`로 고정했다.
- Runtime UI의 기존 `RUIL` 전용 binary fallback은 제거했다.
- Content Browser / AssetQuery / ResourceManager discovery / GamePackager broad copy에서 legacy asset 확장자를 걷어냈다.
- 아직 남은 legacy 브릿지:
  - 기존 scene/prefab/material json이 직접 참조하는 `.mat`, `.matinst`, `.particlesystem`을 패키징 의존성으로 따라가기 위한 JSON scan.
  - 기존 scene/prefab이 아직 `.mat/.matinst` 경로를 들고 있을 때 런타임이 바로 깨지지 않게 하는 material deserialize 경로.
- 현재 legacy 에셋 실물 수:
  - `.curve`: 8개
  - `.mat`: 280개
  - `.matinst`: 43개
  - `.particlesystem`: 2개
- 다음 배치에서 해야 할 실제 정리:
  - 변환 도구로 위 legacy 에셋을 `.uasset`로 생성.
  - scene/prefab/material instance 안의 문자열 참조를 새 `.uasset` 경로로 rewrite.
  - 변환 검증 후 legacy 파일 삭제.

현재 구현은 6배치의 코드 통합과 신규 정책 전환까지 진행된 상태다. 다만 실제 패키징 실행과 패키징된 런타임에서 material/mesh/UI layout을 여는 검증은 별도 스모크 테스트로 남겨둔다.
