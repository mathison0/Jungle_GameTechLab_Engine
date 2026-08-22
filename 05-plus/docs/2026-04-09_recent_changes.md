# 2026-04-09 기준 최근 변경 사항 설명서

## 기준

- 조회 기준 시각: `2026-04-09 00:00:00 +0900` 이후의 `main` 브랜치 커밋 로그
- 결과: `2026-04-09`에 생성된 새 커밋은 없습니다.
- 따라서 이 문서는 `2026-04-08 22:12:25 +0900`부터 `2026-04-08 23:58:52 +0900`까지의 가장 최근 개발 세션을 정리합니다.
- 병합 전용 커밋은 설명 대상에서 제외하고, 실제 기능 추가/수정 커밋만 요약합니다.

## 최근 커밋 목록

| 시각(KST) | 커밋 | 작성자 | 제목 |
|---|---|---|---|
| 2026-04-08 23:58:52 | `6ea3aa7` | DDing-Ho | `fix: 회전 계산과 씬 설정 조정` |
| 2026-04-08 22:41:27 | `5eec2e6` | DDing-Ho | `feat: MovementComponent 추가 및 PlayerStart 선택 복구` |
| 2026-04-08 22:33:17 | `1fcdc03` | CaptainTangerine | `chore : hide Playstart in PIE` |
| 2026-04-08 22:30:25 | `da1151c` | Seyoung Park | `fix: 파일탐색기 선택없이 종료시 발생한 멈춤현상` |
| 2026-04-08 22:26:16 | `df45735` | Seyoung Park | `browse한 파일도 드롭리스트에 노출되게 수정` |
| 2026-04-08 22:12:25 | `f1c1eea` | Seyoung Park | `스프라이트 드롭리스트 기본 표시 경로 축소` |

## 변경 요약

### 1. 에디터의 스프라이트/텍스처 선택 UX 개선

관련 커밋:
- `f1c1eea`
- `df45735`
- `da1151c`

핵심 변경:
- 빌보드 기본 스프라이트가 `Textures/FileIcon.png` 대신 `Editor/Icons/Pawn_64x.png`를 사용하도록 정리되었습니다.
- 텍스처 드롭리스트의 기본 스캔 범위를 에디터 아이콘 중심으로 축소해 노이즈를 줄였습니다.
- 파일 브라우저로 한 번 불러온 텍스처도 드롭리스트에 다시 나타나도록 `UTexture::GetAvailableTextureAssetPaths()`가 메모리에 이미 로드된 텍스처까지 포함하도록 확장되었습니다.
- 경로 정규화와 중복 제거가 추가되어, 같은 텍스처가 다른 상대경로나 대소문자 차이로 중복 노출되는 문제를 줄였습니다.
- 파일 선택 대화상자에 `OFN_NOCHANGEDIR`를 추가하고 취소 경로를 보강해, 탐색기에서 아무 파일도 고르지 않고 닫았을 때 에디터가 멈추는 현상을 완화했습니다.

영향 범위:
- `Editor/Source/UI/PropertyWindow.cpp`
- `Engine/Source/Renderer/Texture.cpp`

사용자 체감:
- Billboard Sprite 선택 UI가 더 짧고 깔끔한 목록을 보여줍니다.
- 직접 `Browse...`로 읽어온 파일도 이후 콤보 목록에서 다시 선택할 수 있습니다.
- 파일 탐색기를 취소했을 때 UI가 덜 불안정해집니다.

### 2. PlayerStart의 PIE 동작과 시작 방향 처리 보강

관련 커밋:
- `1fcdc03`
- `6ea3aa7`

핵심 변경:
- `APlayerStart::BeginPlay()`에서 `SetVisible(false)`를 호출하도록 바뀌어, PIE/Game 실행 시 PlayerStart 마커가 화면에 남지 않게 했습니다.
- PIE 시작 시 카메라 위치는 여전히 `PlayerStart`의 루트 위치를 사용하지만, 회전은 이제 단순 상대 회전값이 아니라 루트의 실제 월드 forward 축으로부터 계산됩니다.
- `EditorEngine`에 `MakeCameraRotatorFromForward()` 보조 함수가 추가되어, `PlayerStart`가 부모에 붙어 있거나 월드 회전이 반영된 경우에도 PIE 카메라가 올바른 방향을 바라보도록 수정했습니다.

영향 범위:
- `Editor/Source/EditorEngine.cpp`
- `Engine/Source/Actor/PlayerStart.h`
- `Engine/Source/Actor/PlayerStart.cpp`

사용자 체감:
- PIE 시작 시 PlayerStart가 에디터용 마커로만 동작하고, 실제 플레이 화면에는 덜 보입니다.
- PlayerStart 화살표 방향과 PIE 시작 카메라 방향의 일치성이 좋아집니다.

### 3. UMovementComponent 추가

관련 커밋:
- `5eec2e6`

핵심 변경:
- 새 런타임 컴포넌트 `UMovementComponent`가 추가되었습니다.
- 이 컴포넌트는 `Game` 또는 `PIE` 월드에서만 동작하며, 소유 액터의 루트 컴포넌트를 Z축 방향으로 사인파 형태로 상하 반복 이동시킵니다.
- 직렬화 지원이 포함되어 `Enabled`, `Amplitude`, `Speed` 값이 씬 저장/로드를 통과합니다.
- 복제 시 런타임 내부 상태(`InitialRelativeZ`, `ElapsedTime`)를 초기화해 PIE 복제 흐름과 충돌하지 않게 했습니다.
- 에디터 프로퍼티 창에 `Movement` 섹션이 추가되어 활성화 여부, 진폭, 속도를 직접 편집할 수 있습니다.

영향 범위:
- `Engine/Source/Component/MovementComponent.h`
- `Engine/Source/Component/MovementComponent.cpp`
- `Editor/Source/UI/PropertyWindow.cpp`
- `Engine/Engine.vcxproj`
- `Engine/Engine.vcxproj.filters`

사용 예:
1. 에디터에서 액터를 선택합니다.
2. `Add Component`에서 `Movement`를 추가합니다.
3. `Movement` 섹션에서 `Amplitude`와 `Speed`를 조절합니다.
4. PIE를 시작하면 액터가 위아래로 반복 이동합니다.

### 4. PlayerStart 선택 복구와 UObject 포인터 안정성 보강

관련 커밋:
- `5eec2e6`

핵심 변경:
- 씬 로드 중 액터 UUID를 저장값으로 복원한 뒤, 이미 생성되어 있던 컴포넌트들의 `Owner`를 다시 `this`로 재연결하도록 `AActor::Serialize()`가 수정되었습니다.
- `TObjectPtr::Get()`이 UUID 맵에서 찾은 최신 객체 포인터로 `CachedPtr`를 갱신하도록 보강되었습니다.

배경:
- `PlayerStart`는 로드 도중 visualizer component를 먼저 만들고, 나중에 액터 UUID를 저장값으로 되돌립니다.
- 이 순서에서 component owner가 예전 UUID 기준 상태를 유지하면, 피킹은 component까지 맞아도 `GetOwner()`가 잘못된 액터를 가리킬 수 있습니다.

사용자 체감:
- PlayerStart의 Arrow/Billboard가 보이는데도 선택이 안 되거나 기즈모가 뜨지 않는 현상이 완화됩니다.

영향 범위:
- `Engine/Source/Actor/Actor.cpp`
- `Engine/Source/Types/ObjectPtr.h`

### 5. 회전 수학 보정

관련 커밋:
- `6ea3aa7`

핵심 변경:
- `FMatrix::MakeRotationY()`의 부호 방향이 수정되었습니다.
- `FQuat::Rotator()`가 단순 행렬 원소 분해 대신, 정규화된 quaternion의 `Forward`와 `Right` 벡터를 기반으로 pitch/yaw/roll을 계산하도록 변경되었습니다.
- 직교 기저를 다시 만드는 경로가 들어가, 특정 축 근처에서 roll/yaw가 뒤집히는 문제를 줄이는 방향으로 보정되었습니다.

영향 범위:
- `Engine/Source/Math/Matrix.h`
- `Engine/Source/Math/Quat.cpp`

예상 효과:
- PlayerStart, 카메라, 기즈모, 회전 직렬화 재적용 시의 방향 해석이 더 일관됩니다.
- forward 축 기반 회전 계산이 필요한 PIE 카메라 초기화와도 잘 맞습니다.

### 6. 샘플 씬 업데이트

관련 커밋:
- `6ea3aa7`

핵심 변경:
- `Assets/Scenes/Week5Plus.json`에 새 액터가 추가되었습니다.
- 새 액터는 `UStaticMeshComponent`와 `UMovementComponent`를 함께 가지고 있으며, `chopper.Model` 정적 메시를 사용합니다.
- PlayerStart의 위치/회전과 카메라 초기 상태도 함께 조정되었습니다.

의도:
- 새 `UMovementComponent`를 실제 씬에서 바로 확인할 수 있는 데모 데이터를 제공하려는 변경으로 보입니다.

## 시스템별 영향 정리

### Editor

- 프로퍼티 창의 Billboard Sprite 선택 경험 개선
- MovementComponent 편집 UI 추가
- PIE 시작 시 PlayerStart 방향 반영 정확도 개선

### Runtime / Engine

- MovementComponent 런타임 tick 및 직렬화 추가
- Actor 로드 후 component owner 재연결
- TObjectPtr 캐시 갱신 안정성 보강
- Quaternion/Matrix 회전 계산 수정

### Scene / Content

- `Week5Plus` 샘플 씬에 움직이는 오브젝트와 PlayerStart 배치 반영
- ImGui 레이아웃 파일 일부 변경

## 주의 사항

- `2026-04-09` 당일 커밋은 없으므로, 이 문서는 실제로는 `2026-04-08` 늦은 밤의 최근 변경을 정리한 문서입니다.
- 이 문서는 커밋 로그와 diff를 기준으로 작성했습니다.
- 빌드 성공 여부나 수동 테스트 결과는 커밋 로그 자체만으로는 보장되지 않으므로, 실제 반영 전에는 `Engine`, `Editor`, `Client` 빌드와 PIE 스모크 테스트를 다시 확인하는 것이 안전합니다.
