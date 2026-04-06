# Occlusion Culling

## 개요

이 프로젝트의 `Occlusion culling`은 `primitive` 단위가 아니라 `visibility cluster` 단위로 동작한다.  
전체 흐름은 다음과 같다.

1. CPU에서 `frustum culling`으로 화면 안에 들어온 클러스터를 추린다.
2. 이전 프레임에서 만들어 둔 깊이 버퍼 기반 `HZB(Hierarchical Z-Buffer)`가 유효하면, 현재 프레임의 후보 클러스터를 GPU compute shader로 테스트한다.
3. GPU 결과는 즉시 사용하지 않고 `readback`이 완료된 뒤 몇 프레임 늦게 반영된다.
4. 실제 렌더링이 끝나면 현재 프레임 depth로 새 `HZB`를 만들고, 이 `HZB`를 다음 프레임의 occlusion test에 사용한다.

즉, 이 구현은 "현재 프레임의 후보를 이전 프레임 depth 기반 HZB로 판정하는" 구조이며, 카메라 이동이나 씬 변경이 크면 히스토리를 버리고 다시 `frustum-visible` 결과로 되돌아간다.

## 관련 코드 위치

- `PerformanceEngine/Source/Scene/Scene.cpp`
  - 정적 `visibility cluster` 생성
  - 동적 오브젝트 클러스터 생성
  - 큰 정적 클러스터를 BVH 자식으로 refine
- `PerformanceEngine/Source/Visibility/VisibilitySystem.cpp`
  - `frustum culling`
  - occlusion 후보 목록 작성
  - 최종 visible cluster/primitive 확정
- `PerformanceEngine/Source/Renderer/SceneRenderer.cpp`
  - depth 기반 `HZB` 생성
  - occlusion compute dispatch
  - 결과 readback과 지연 반영
- `PerformanceEngine/Shader/HZB/DepthToHzbMip0CS.hlsl`
  - depth -> `HZB mip 0`
- `PerformanceEngine/Shader/HZB/ReduceHzbMipCS.hlsl`
  - `HZB` 상위 mip 생성
- `PerformanceEngine/Shader/HZB/OcclusionCullCS.hlsl`
  - 후보 클러스터 occlusion 판정
- `PerformanceEngine/Source/Core/Core.cpp`
  - 프레임 단위 orchestration
  - 히스토리 invalidation

## 왜 cluster 단위인가

이 구현은 오브젝트 하나마다 GPU occlusion test를 보내지 않는다. 대신 여러 primitive를 묶은 `FVisibilityCluster` 단위를 사용한다.

`FVisibilityCluster`는 다음 정보를 갖는다.

- `BoundsMin`, `BoundsMax`
- `PrimitiveOffset`, `PrimitiveCount`
- `SourceBvhNodeIndex`
- `bUsesFramePrimitiveIndices`
- `bDynamic`

이 단위를 쓰는 이유는 다음과 같다.

- GPU 후보 개수를 primitive 수보다 훨씬 줄일 수 있다.
- CPU `frustum culling`과 GPU `occlusion culling`의 단위를 맞출 수 있다.
- BVH 노드를 그대로 활용해 큰 덩어리부터 보수적으로 가릴 수 있다.

## 정적 클러스터 생성 방식

정적 클러스터는 월드 BVH의 스냅샷에서 파생된다.  
`FScene::BuildVisibilityClusters()`가 BVH를 순회하면서 적절한 노드를 하나의 클러스터로 채택한다.

클러스터 생성 기준은 다음 상수로 정의되어 있다.

- `VisibilityClusterPrimitiveTarget = 8`
- `VisibilityClusterMaxLongestAxisScale = 2.5f`
- `VisibilityClusterMaxAspectRatio = 3.0f`

즉, 노드가 leaf이면 그대로 클러스터가 되고, leaf가 아니더라도 아래 조건을 만족하면 그 노드를 하나의 정적 클러스터로 사용한다.

- 포함 primitive 수가 너무 크지 않을 것
- 노드 AABB가 지나치게 길쭉하지 않을 것
- 노드 크기가 내부 primitive 평균 크기에 비해 과도하게 크지 않을 것

이 기준을 만족하지 않으면 BVH의 좌/우 자식으로 더 내려가며 클러스터를 생성한다.

결과적으로 정적 클러스터는 "너무 크지 않고, 너무 긴 막대 형태도 아닌, 적당한 BVH 묶음"으로 만들어진다.

## 동적 오브젝트 처리

동적 primitive는 정적 클러스터 스냅샷에 섞지 않는다.  
`FScene::BuildDynamicVisibilityClusters()`에서 프레임마다 따로 모아서, 동적 primitive 1개당 클러스터 1개를 만든다.

즉 현재 구조는 다음과 같다.

- 정적 오브젝트: BVH 기반 정적 클러스터 사용
- 동적 오브젝트: 프레임마다 1 primitive = 1 cluster

따라서 동적 오브젝트는 refine 이득은 적지만, 씬이 움직여도 정적 스냅샷 전체를 재구성하지 않아도 되는 장점이 있다.

## 프레임 파이프라인

### 1. 히스토리 유효성 검사

`FCore::Update()`에서 아래 조건 중 하나라도 만족하면 occlusion 히스토리를 버린다.

- 뷰포트 크기 변경
- 카메라 FOV 변경
- 카메라 이동 거리 `>= 10.0f`
- 카메라 회전 각도 `>= 5.0`도
- 씬 편집, 오브젝트 이동/추가/삭제

히스토리를 무효화하면 다음이 함께 초기화된다.

- `VisibilitySystem`의 history valid 상태
- `SceneRenderer`의 delayed readback ring
- 마지막 `HZB` 유효 상태

이 구현은 이전 프레임 `HZB`를 현재 프레임에서 재사용하지만 reprojection을 하지 않기 때문에, 카메라나 씬 변화가 커지면 과감하게 히스토리를 버리는 쪽을 선택한다.

### 2. `VisibilitySystem::PrepareFrame()`

이 단계에서 수행되는 일은 다음과 같다.

1. 현재 프레임 번호를 부여한다.
2. 현재 프레임에서 occlusion 결과를 쓸 수 있는지 `bOcclusionValid`를 기록한다.
3. 카메라로부터 frustum plane을 만든다.
4. 정적 클러스터와 동적 클러스터에 대해 `frustum culling`을 수행한다.
5. 히스토리가 유효하면, `frustum-visible` 클러스터 전부를 occlusion 후보로 등록한다.

즉 이 구현에서 GPU occlusion test의 입력은 "frustum을 통과한 클러스터 목록"이다.

### 3. 큰 정적 클러스터 refine

`AppendFrustumVisibleCluster()`는 필요한 경우 정적 클러스터를 BVH 자식 둘로 세분화한다.

refine 조건은 다음과 같다.

- history가 유효할 것
- 동적 클러스터가 아닐 것
- BVH 원본 노드 인덱스가 존재할 것
- primitive 수가 2개 이상일 것
- 카메라 근평면 바로 앞에 붙어 있지 않을 것
- 화면에서 충분히 크게 보일 것

화면 크기 기준 상수는 `ClusterRefineMinScreenFraction = 0.2f`이다.  
클러스터 AABB를 투영했을 때 화면 폭 또는 높이 중 큰 쪽이 화면의 20% 이상이면 refine를 시도한다.

refine가 성공하면 부모 클러스터 대신 BVH 좌/우 자식 노드 기반 클러스터 2개를 후보로 사용한다.

의도는 단순하다.

- 화면에서 큰 클러스터는 한 덩어리로 테스트하면 false visible이 늘어난다.
- 따라서 큰 정적 덩어리는 좀 더 잘게 나눠서 occlusion 정확도를 높인다.

## GPU occlusion 테스트 입력

GPU에 전달되는 후보는 `FGpuOcclusionCandidate` 배열이며, 각 원소는 다음만 가진다.

- `BoundsMin`
- `BoundsMax`

즉 GPU에서는 클러스터 AABB만 보고 판정한다.  
메시 삼각형 단위의 정밀 판정은 하지 않는다.

이 후보 배열은 `StructuredBuffer<FGpuOcclusionCandidate>`로 업로드되고, 결과는 `RWStructuredBuffer<uint>`에 `0` 또는 `1` 플래그로 기록된다.

## HZB 생성 방식

이 구현의 `HZB`는 현재 프레임 depth buffer에서 compute shader로 생성된다.

### Standard-Z 전제

코드와 셰이더는 다음 계약을 전제로 한다.

- depth clear 값은 `1.0`
- depth test는 `LESS_EQUAL`
- near는 0에 가깝고 far는 1에 가깝다
- `HZB` mip reduction은 `max()`를 사용해 더 먼 depth를 저장한다

이 전제가 있기 때문에 occlusion compare도 다음 식을 사용한다.

- `candidate min depth > HZB max depth + epsilon` 이면 occluded

만약 나중에 renderer를 `reversed-Z`로 바꾸면, `HZB` reduction과 compare 방향을 함께 바꿔야 한다.

### Mip 0

`DepthToHzbMip0CS.hlsl`은 scene depth를 그대로 `HZB mip 0`에 복사한다.

### 상위 mip

`ReduceHzbMipCS.hlsl`은 상위 mip를 만들 때 2x2 블록의 depth 중 `max()`를 저장한다.

즉 상위 mip일수록 더 넓은 영역의 "가장 먼 depth"가 남는다.

이 방식은 보수적이다.

- 어떤 영역에 가까운 가림막이 있더라도
- 그 영역 안 어딘가에 더 먼 depth가 섞여 있으면 `max depth`가 커진다
- 그 결과 culling은 덜 공격적이지만, 잘못 가리는 위험은 줄어든다

## `OcclusionCullCS` 판정 방식

`OcclusionCullCS.hlsl`은 후보 클러스터마다 다음 절차를 수행한다.

### 1. AABB 8개 코너 투영

클러스터 AABB의 8개 코너를 현재 카메라의 `View`, `ViewProjection`으로 변환한다.

이때 다음 경우는 바로 `visible`로 처리한다.

- 코너 중 하나라도 근평면 근처에 걸리는 경우
- `clip.w`가 너무 작아 정상 투영이 어려운 경우
- 최종 screen rect를 만들 수 없는 경우

즉 이 셰이더는 애매한 경우는 가리지 않고 보이게 두는 보수적 정책을 사용한다.

### 2. Screen rect와 최소 depth 계산

8개 코너를 화면 좌표로 바꿔 screen-space AABB를 만든다.  
동시에 `NDC.z`의 최소값을 구한다.

여기서 사용하는 depth는 "클러스터가 카메라에 가장 가까운 깊이"다.

### 3. 사용할 mip 선택

screen rect를 더 거친 mip로 옮겨 가며 footprint를 측정하고, 대략 `2x2` 이하가 되는 mip를 선택한다.

추가로 작은 물체에 대해서는 한 단계 더 세밀한 mip를 사용한다.

- `SmallObjectMaxFootprint = 8`
- 원래 screen rect가 `8x8` 이하이면서 `SelectedMip > 0`이면 `SelectedMip -= 1`

즉, 아주 작은 후보는 너무 거친 mip를 써서 과하게 visible 판정되는 것을 줄이려는 의도가 들어가 있다.

### 4. 선택된 mip 범위에서 최대 depth 샘플링

선택한 mip에서 후보가 덮는 모든 texel을 순회하며 `MaxHzbDepth`를 구한다.

### 5. 최종 비교

최종 판정은 아래 식이다.

```text
MinDepth > MaxHzbDepth + DepthEpsilon  -> occluded
그 외                                 -> visible
```

현재 상수는 `DepthEpsilon = 1e-3f`이다.

의미는 다음과 같다.

- 후보 박스의 가장 가까운 점조차
- 해당 screen 영역의 가장 먼 기록 depth보다 더 뒤에 있으면
- 그 후보는 완전히 가려졌다고 본다

반대로 조금이라도 애매하면 visible로 남겨 둔다.

## 지연 readback 구조

GPU 결과는 즉시 CPU에서 쓰지 않는다.  
`SceneRenderer`는 `OcclusionReadbackSlotCount = 3` 크기의 ring buffer를 사용해 비동기 readback을 관리한다.

각 slot은 다음 상태를 가진다.

- GPU visibility flag buffer
- staging buffer
- completion query
- GPU timestamp query
- 제출된 `FVisibilityFrameInput`
- 제출 시각

프레임 흐름은 다음과 같다.

1. 프레임 시작 시 pending slot들을 검사한다.
2. 완료된 slot이 있으면 가장 최신 프레임 결과를 하나 선택한다.
3. staging buffer를 map 해서 visible cluster index를 복원한다.
4. 같은 시점에 GPU occlusion time, readback latency, CPU copy time도 집계한다.
5. 완료된 오래된 slot들은 정리한다.
6. 마지막 프레임에서 만든 `HZB`가 유효하면 현재 프레임 후보를 새로운 slot에 제출한다.

이 구조 덕분에 GPU가 바쁜 프레임에서도 CPU가 즉시 block되지 않는다.  
대신 결과는 몇 프레임 늦게 도착할 수 있다.

## 어떤 결과를 실제 렌더링에 쓰는가

`FCore::Update()`는 상황에 따라 세 가지 경로 중 하나를 택한다.

### 1. 지연된 occlusion 결과가 도착한 경우

`FinalizeFrame()`에 해당 프레임의 visible cluster index를 넘겨서, 실제 occlusion 결과를 반영한 `VisibilityResults`를 만든다.

이때 `VisibilityResultAgeFrames`는 현재 프레임과 결과가 생성된 프레임 번호 차이로 기록된다.

### 2. 아직 pending readback이 없거나, 사용할 이전 결과가 없는 경우

occlusion 없이 `frustum-visible` 클러스터를 그대로 사용한다.

즉 첫 프레임 또는 히스토리 직후에는 무조건 frustum 결과로 렌더링한다.

### 3. readback은 pending인데 새 결과가 아직 안 도착한 경우

직전의 `VisibilityResults`를 계속 사용한다.  
즉 화면에는 약간 오래된 visibility 결과가 쓰일 수 있고, HUD에는 결과 나이가 증가해서 표시된다.

이 점이 이 구현의 중요한 특성이다.

- 테스트는 현재 프레임 후보로 진행한다
- 렌더링은 최근에 확정된 visibility 결과를 사용한다
- render depth로 새 `HZB`를 만들고 다음 프레임 테스트에 넘긴다

## 렌더링과 HZB의 관계

`RenderVisibleScene()`은 현재 선택된 `VisibilityResults`로 base pass를 렌더링한 뒤, 그 depth buffer에서 `HZB`를 생성한다.

즉 `HZB`의 원본은 별도 depth prepass가 아니라 "실제로 렌더된 장면의 depth"다.

참고로 코드에는 `RunDepthOnlyPrepass()`와 `FOcclusionTimingStats::DepthPrepassGpuTimeMs`가 존재하지만, 현재 기본 경로에서는 사용되지 않는다.  
현재 구현 기준 문서에서는 별도 depth prepass가 없는 것으로 보는 것이 맞다.

## 최종 visible primitive 생성

occlusion 결과는 cluster index 목록으로 돌아온다.  
`VisibilitySystem::FinalizeFrame()`은 이 클러스터들을 primitive index로 확장하고, 중복 primitive를 제거한 뒤 LOD를 선택한다.

즉 occlusion 단계의 출력은 "보이는 primitive 목록"이 아니라 "보이는 cluster 목록"이며, 실제 렌더 가능한 primitive 집합은 그 다음 단계에서 만들어진다.

## 디버그/HUD에서 확인 가능한 값

HUD에는 다음 항목이 출력된다.

- 총 클러스터 수
- frustum 통과 클러스터 수
- occlusion 후보 클러스터 수
- 최종 visible cluster 수
- occluded cluster 수
- 결과 age(frame)
- `HZB` 유효 여부
- occlusion 결과 사용 여부
- `HZB` GPU 시간
- occlusion compute GPU 시간
- GPU readback latency
- CPU readback copy 시간

따라서 디버깅 시에는 다음을 같이 보면 된다.

- `HZB VALID`
- `OCCLUSION USED`
- `VIS AGE`
- `GPU HZB`
- `GPU OCC`

## 구현의 장점

- frustum 이후 후보만 GPU에 보내므로 비용이 단순하다.
- cluster 단위를 써서 primitive 수가 많아도 후보 수를 줄일 수 있다.
- 큰 정적 클러스터 refine로 coarse cluster의 false visible을 줄인다.
- 지연 readback ring으로 CPU stall을 줄인다.
- 보수적 판정이라 잘못 가리는 위험이 낮다.

## 현재 구현의 한계

### 1. 이전 프레임 HZB를 reprojection 없이 재사용

카메라가 조금만 움직인 경우에는 그대로 쓰고, 많이 움직이면 히스토리를 버린다.  
즉 reprojection 기반 temporal occlusion이 아니라 "작은 변화에는 재사용, 큰 변화에는 invalidate" 방식이다.

### 2. 결과가 늦게 반영될 수 있음

GPU readback이 늦으면 화면은 이전 visibility 결과를 계속 사용할 수 있다.  
그래서 `VIS AGE`가 증가할 수 있다.

### 3. 동적 오브젝트는 1 primitive = 1 cluster

정적 BVH 클러스터처럼 정교한 계층형 grouping을 활용하지 않는다.

### 4. 보수적 판정이라 false visible은 허용

근평면 근처, 작은 물체, 투영이 애매한 경우는 visible 쪽으로 기운다.  
즉 성능보다 안정성을 우선한 구현이다.

### 5. Standard-Z에 강하게 묶여 있음

`reversed-Z`나 다른 depth contract로 바꾸면 다음을 함께 수정해야 한다.

- depth clear 값
- depth compare 함수
- `HZB` reduction 방향(`max` 또는 `min`)
- occlusion compare 식

## 요약

이 프로젝트의 occlusion culling은 다음 문장으로 요약할 수 있다.

> CPU에서 `frustum-visible cluster`를 만들고, 이전 프레임 depth로 생성한 `HZB`로 현재 후보를 GPU에서 보수적으로 테스트한 뒤, 비동기 readback 결과를 몇 프레임 늦게 반영하는 cluster 기반 occlusion 시스템이다.

정확도보다는 안정성과 구현 단순성, 그리고 GPU/CPU 동기화 비용 절감을 우선한 구조라고 보면 된다.
