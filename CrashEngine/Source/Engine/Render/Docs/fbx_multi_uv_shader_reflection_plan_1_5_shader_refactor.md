# FBX Multi-UV / Shader Reflection 1.5단계 계획

## 1. 목표

1단계에서 material/shader 연결과 reflection 기반 binding 복구를 진행했다면, 2단계로 넘어가기 전에 셰이더 코드 자체를 정리하는 별도 완충 단계가 필요하다.

현재 렌더 경로는 deferred 중심 구조에서 forward+를 병행하는 하이브리드 구조로 확장되었고, 그 과정에서 다음 문제가 누적되어 있다.

- forward opaque / deferred opaque HLSL에 표면 해석 로직이 중복되어 있다.
- deferred lighting 경로와 view-mode lighting 경로에 유사한 조명 계산 코드가 분산되어 있다.
- 전역광 / 지역광 계산 책임이 pass별로 갈라져 있어 읽기와 수정이 어렵다.
- `Passes/...` 와 `Render/Scene/...` 디렉터리의 책임 경계가 명확하지 않다.
- 테스트/과도기용 shader 및 special-case 경로가 남아 있어 실제 사용 경로 추적 비용이 높다.

이번 1.5단계의 목표는 기능 추가가 아니라, 다음 두 가지를 만족시키는 셰이더 구조 정리다.

- 셰이더 코드 가독성을 높인다.
- 2단계에서 vertex layout / UV 확장 리팩토링을 안전하고 예측 가능하게 수행할 수 있게 만든다.

이번 단계에서는 전역 forward/deferred 토글이 계속 존재한다고 가정한다.
장기적으로 per-object 단위 혼합 렌더링으로 갈 가능성은 염두에 두되, 이번 단계 목표는 그 구현이 아니라 구조 정리에 둔다.

### 기준 시점

- 기준 시각: `2026-05-12`
- 기준 작업 디렉터리: `CrashEngine/`

---

## 2. 진행 원칙

### 원칙 1. 이번 단계는 "동작 변경"보다 "구조 정리"가 우선이다

동일한 입력에서 기존과 동일하거나 거의 동일한 시각 결과를 유지하는 것을 우선한다.
조명 모델 자체를 바꾸거나 shading result를 재설계하지 않는다.

### 원칙 2. 공통 계산과 경로별 계산을 분리한다

완전한 단일 uber shader로 합치는 것이 목적이 아니다.
공통으로 써야 할 계산은 helper로 올리고, forward / deferred 차이로 남겨야 할 부분만 pass entry에 남긴다.

### 원칙 3. "광원 1개 계산"과 "광원 목록 순회 정책"을 분리한다

forward와 deferred는 local light를 순회하는 방식이 다르다.

- forward: 전체 local light 또는 바인딩된 목록을 직접 순회
- deferred: tile/cluster mask 기반 subset 순회

따라서 local light 처리 전체를 억지로 하나로 합치지 않고,
"single-light evaluation"과 "iteration policy"를 분리하는 방향으로 정리한다.

### 원칙 4. entry shader와 reusable helper의 경계를 고정한다

정리 후에는 다음 규칙을 지향한다.

- `Shaders/Passes/...`
  - 실제 compile entry를 가진 pass shader만 둔다.
- `Shaders/Render/Scene/...`
  - material evaluation, lighting, packing, shared type 등 재사용 helper만 둔다.

### 원칙 5. 테스트/실험 경로는 명시적으로 격리한다

reflection 검증용 또는 임시 래퍼 shader가 필요하면 유지할 수 있다.
다만 일반 경로와 같은 층위에 섞어 두지 않고, 문서/주석/이름으로 목적을 명확히 드러낸다.

---

## 3. 현재 구조 진단

### 3-1. 표면 해석 로직 중복

다음 코드가 사실상 동일한 역할을 각 pass 파일에서 별도로 구현하고 있다.

- `ForwardOpaquePass.hlsl`
  - normal resolve
  - specular/material param resolve
  - base color sampling
  - `FSurfaceData` 구성
- `DeferredOpaquePass.hlsl`
  - normal resolve
  - specular/material param resolve
  - base color sampling
  - `FSurfaceData` 구성

이 구조는 UV 채널이 늘어나거나 material reflection binding이 일반화될 때 수정 지점을 두 배로 만든다.

### 3-2. 조명 계산 경계 혼재

현재 조명 helper는 일부는 공통이고, 일부는 pass 파일 안에서 다시 조합된다.

- `DirectLighting.hlsli`
  - global + local을 모두 포함하는 함수 존재
  - global only 함수도 별도 존재
  - single local light term 함수 존재
- `DeferredLightingPS.hlsl`
  - global 계산 후 tile light loop를 다시 구현
- forward opaque
  - 통합 함수 호출로 바로 최종 색을 계산

즉, "조명 수학"과 "렌더 경로 정책"이 깔끔하게 분리되어 있지 않다.

### 3-3. 파일 역할 혼재

현재 실제 사용 파일과 유사/중복 파일이 함께 존재한다.

예:

- 실제 deferred lighting pass는 `DeferredLightingPS.hlsl`를 사용
- 유사한 역할의 `Render/Scene/ViewModes/UberLit.hlsl`가 별도로 존재

이 상태는 유지보수자가 어느 파일이 canonical인지 즉시 판단하기 어렵다.

### 3-4. 과도기/테스트 특례 존재

reflection binding 검증을 위한 특수 shader와 특정 material-shader pair 하드코딩이 남아 있다.
이 경로는 유용하지만, 일반 경로와 섞여 있으면 이후 2단계 확장에서 구조를 오염시킬 수 있다.

---

## 4. 이번 단계 범위

### 포함

- forward/deferred opaque의 공통 surface evaluation helper 추출
- lighting helper 계층 재구성
- deferred lighting 관련 중복/유사 shader 정리
- shader 디렉터리 책임 재정의
- 테스트/임시 shader 경로 식별 및 격리
- 필요한 경우 C++ registry / 주석 / 이름 보강

### 제외

- vertex layout 확장
- UV1 이상 런타임 전달 구현
- FBX importer 변경
- static/skeletal mesh vertex struct 확장
- per-object forward/deferred 선택 기능 구현
- shading model 추가

---

## 5. 리팩토링 목표 구조

### 5-1. Material / Surface 계층

목표:

- base color / normal / material param 해석을 pass 밖으로 뺀다.
- `FSurfaceData` 구성 규칙을 공통화한다.

권장 결과물 예시:

- `Render/Scene/Material/MaterialSampling.hlsli`
  - texture sampling primitive
- `Render/Scene/Material/SurfaceEvaluation.hlsli`
  - normal resolve
  - material param resolve
  - static mesh surface build

핵심 포인트:

- forward/deferred가 공통으로 사용할 수 있는 helper로 만든다.
- UV0 기준 구현은 유지하되, UV source를 향후 확장 가능한 형태로 둔다.

### 5-2. Lighting 계층

목표:

- single-light 계산과 light iteration 정책을 분리한다.

권장 결과물 예시:

- `Render/Scene/Lighting/DirectLighting.hlsli`
  - directional/global accumulation
  - single local light lambert/blinn-phong term
- `Render/Scene/Lighting/LightIteration.hlsli`
  - forward local light loop
  - deferred tiled local light loop
- `Render/Scene/Lighting/LightingEvaluation.hlsli`
  - 최종 lambert/blinn-phong shading composition

핵심 포인트:

- global/local을 함수 이름 수준에서 명확히 구분한다.
- pass마다 tile/light list 순회 코드가 복제되지 않게 한다.

### 5-3. Pass Entry 계층

목표:

- pass 파일에는 "입력 준비 + helper 호출 + 출력 encoding"만 남긴다.

예:

- `ForwardOpaquePass.hlsl`
  - VS entry
  - forward decal 적용
  - 공통 surface build 호출
  - 공통 lighting evaluation 호출
- `DeferredOpaquePass.hlsl`
  - VS entry
  - 공통 surface build 호출
  - gbuffer encode만 수행
- `DeferredLightingPS.hlsl`
  - surface decode
  - 공통 lighting evaluation 호출

### 5-4. Experimental / Test 계층

목표:

- reflection 검증용 shader와 특례 경로를 일반 경로와 분리한다.

예:

- `CustomTest.hlsl` 유지 시:
  - phase validation / test-only 라는 성격을 명시
  - 문서에서 사용 이유와 제거 조건을 기록

---

## 6. 세부 Phase 구성

### Phase 1. Shader Inventory와 Canonical 경로 고정

목표:

- 어떤 shader가 실제 사용 경로인지 먼저 고정한다.

#### Step 1-1. entry shader / helper / test shader 분류표 작성

Task:

- `CrashEngine/Shaders` 내 파일을 다음으로 분류한다.
  - active entry shader
  - reusable helper
  - duplicate/legacy candidate
  - test-only shader

검증:

- 분류표 기준으로 주요 scene pass가 참조하는 실제 shader 파일을 설명 가능

#### Step 1-2. deferred lighting canonical 파일 확정

Task:

- `DeferredLightingPS.hlsl`와 `UberLit.hlsl`의 책임을 정리한다.
- 실제 사용하는 canonical entry를 하나로 명시한다.
- 다른 하나는 제거 후보 또는 wrapper/helper 성격으로 축소한다.

검증:

- registry 기준 deferred lighting 경로를 1개 canonical 파일로 설명 가능

#### Step 1-3. experimental shader 경로 문서화

Task:

- `CustomTest.hlsl` 및 reflection 검증용 특례 경로를 문서화한다.
- 일반 렌더 경로와 실험 경로의 차이를 명시한다.

검증:

- 왜 해당 파일이 존재하는지 팀원이 문서만 보고 이해 가능

### Phase 2. Opaque Surface Evaluation 공통화

목표:

- forward/deferred opaque의 공통 surface build 경로를 만든다.

#### Step 2-1. normal resolve 공통 helper 추출

Task:

- tangent-space normal map 해석과 fallback normal 처리 코드를 공통 helper로 이동한다.

검증:

- forward/deferred 양쪽에서 동일 helper 사용
- compile 성공

#### Step 2-2. material param resolve 공통 helper 추출

Task:

- shininess/specular strength 해석을 공통 helper로 이동한다.

검증:

- forward/deferred 양쪽에서 동일 helper 사용
- compile 성공

#### Step 2-3. static mesh surface build 공통화

Task:

- base color / section color / normal / specular/material param / gouraud를 묶어 `FSurfaceData`를 만드는 helper를 공통화한다.

검증:

- visual change 없음 또는 최소 수준
- build 성공

### Phase 3. Lighting Evaluation 계층 재구성

목표:

- 조명 계산 책임을 읽기 쉬운 계층으로 재배치한다.

#### Step 3-1. directional/global lighting helper 정리

Task:

- ambient + directional 누적 책임을 하나의 계층으로 모은다.
- lambert / blinn-phong용 global-only 함수 이름을 명확히 정리한다.

검증:

- global lighting helper만 따로 읽어도 directional path 이해 가능

#### Step 3-2. single local light helper 고정

Task:

- local light 1개에 대한 diffuse/specular 계산을 canonical helper로 고정한다.
- spot/point shadow 처리 위치를 명확히 한다.

검증:

- forward/deferred 양쪽이 동일 single-light helper를 사용

#### Step 3-3. iteration policy 분리

Task:

- forward용 local light loop
- deferred tiled/clustered loop

를 별도 helper로 분리한다.

검증:

- pass 파일에서 tile loop 직접 구현량 감소
- compile 성공

#### Step 3-4. 최종 shading evaluation helper 정리

Task:

- lambert / blinn-phong 최종 색 합성 함수를 공통화한다.

검증:

- forward와 deferred lighting가 같은 수학/다른 순회 정책 구조로 설명 가능

### Phase 4. Pass Entry Slim-down

목표:

- pass shader는 entry 역할에 집중시킨다.

#### Step 4-1. ForwardOpaquePass 단순화

Task:

- 공통 helper 호출 위주로 재배치한다.
- forward decal 처리만 forward 특화 책임으로 남긴다.

검증:

- 파일 길이 및 중복 코드 감소

#### Step 4-2. DeferredOpaquePass 단순화

Task:

- surface build 후 gbuffer encode만 담당하도록 정리한다.

검증:

- opaque pass 역할이 명확해짐

#### Step 4-3. DeferredLightingPS 단순화

Task:

- fullscreen lighting pass는 surface decode + lighting evaluation 호출만 담당하도록 정리한다.

검증:

- lighting 구현 상세가 helper로 이동

### Phase 5. Naming / Ownership 정리

목표:

- shader 파일 이름과 실제 역할의 불일치를 줄인다.

#### Step 5-1. naming cleanup 후보 목록 작성

Task:

- 파일명은 남기되, 주석/문서에서 legacy/active 여부를 우선 정리한다.
- 실제 rename은 영향 범위가 크면 후속 작업으로 분리한다.

검증:

- active shader와 historical shader를 구분 가능

#### Step 5-2. C++ registry / 주석 보강

Task:

- `ViewModePassRegistry`, `ShaderProgramRegistry`, 관련 pass 클래스 주석에 canonical shader entry를 명시한다.

검증:

- 코드 탐색 시 pass -> hlsl entry 추적이 쉬워짐

---

## 7. 전역광 / 지역광 통합 방안 검토 결론

질문:

- 현재 전역광과 지역광을 별도 함수에서 다루고 있는데, 이를 통합해 가독성을 높일 수 있는가?

결론:

- "완전 단일 함수"로의 통합은 비권장
- "공통 조명 평가 계층"으로의 통합은 권장

이유:

1. forward와 deferred는 local light 순회 정책이 다르다.
2. deferred는 tile/cluster/light mask를 읽어 subset만 순회해야 한다.
3. 하지만 single-light BRDF 계산과 global light accumulation 수학은 공통화할 수 있다.

따라서 이번 단계의 권장 구조는 아래와 같다.

- 공통:
  - ambient/directional accumulation
  - single local light lambert term
  - single local light blinn-phong term
  - 최종 diffuse/specular 합성
- 경로별:
  - forward local light iteration
  - deferred tiled/clustered local light iteration

즉, "함수 1개로 합치기"가 아니라 "계층을 통합하고 반복 정책만 분리"하는 것이 맞다.

---

## 8. 2단계 준비 관점에서의 기대 효과

이번 1.5단계가 끝나면 2단계에서 다음 이점이 생긴다.

- UV 채널 확장 시 수정 지점이 공용 surface evaluation helper 쪽으로 수렴한다.
- material reflection binding 일반화 시 forward/deferred 양쪽에 중복 수정하지 않아도 된다.
- skeletal / static 경로 확장 시 어떤 semantic이 surface build에 실제로 필요한지 파악이 쉬워진다.
- per-object forward/deferred 혼합 렌더링을 장기적으로 도입할 때도 shading core 재사용이 가능해진다.

---

## 9. 검증 시나리오

### 빌드 검증

- Editor 빌드 성공
- Game 빌드 성공
- shader compile error 없음

### 시각 검증

- single UV static mesh 정상 렌더링
- normal map material 정상 렌더링
- specular material 정상 렌더링
- shadow map / depth pre / selection mask 결과 유지
- deferred lit / forward lit 양쪽에서 기본 장면 결과 유지

### view mode 검증

- Lit Gouraud
- Lit Lambert
- Lit Phong
- Unlit
- WorldNormal
- SceneDepth
- Wireframe

### 회귀 검증

- forward decal 적용 장면 유지
- reflection 검증용 material/shader 조합이 기존처럼 동작
- 테스트용 특례 경로를 제외한 일반 opaque 렌더 결과 변화 없음

---

## 10. 완료 기준

### 구조 기준

- forward/deferred opaque의 surface evaluation 중복이 제거되어 있다.
- deferred lighting pass와 유사 shader의 canonical 경로가 문서와 코드에서 명확하다.
- lighting helper가
  - global accumulation
  - single-light evaluation
  - iteration policy
  - final shading composition

  으로 읽히는 구조가 되어 있다.

### 운영 기준

- shader 파일을 처음 읽는 사람이 "어디가 공통 로직이고 어디가 pass-specific인지" 빠르게 파악할 수 있다.
- 2단계에서 UV 확장 작업 시 수정 진입점이 명확하다.
- 테스트/과도기 shader가 일반 경로와 구분되어 있다.

---

## 11. 권장 구현 순서

1. shader inventory / canonical 파일 확정
2. forward/deferred opaque surface evaluation 공통화
3. lighting helper 계층 재구성
4. deferred lighting / view-mode 중복 정리
5. naming / registry 주석 정리
6. 회귀 검증 및 문서 보완

이 순서를 권장하는 이유는, 2단계 리스크의 핵심이 vertex layout 자체보다도 "셰이더 수정 지점이 너무 많고 책임이 분산되어 있는 상태"에 있기 때문이다.
