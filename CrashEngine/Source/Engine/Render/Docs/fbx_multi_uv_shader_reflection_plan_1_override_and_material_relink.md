# FBX Multi-UV / Shader Reflection 1단계 계획

## 1. 목표

이 계획의 목표는 일정 리스크가 낮고, 이후 확장 작업의 전제 조건이 되는 두 가지를 먼저 끝내는 것이다.

- pass별 강제 shader override 정책을 정리한다.
- shader reflection 기반 material/shader 연동을 복구한다.

이 단계는 "material이 지정한 shader가 실제 런타임 렌더 경로에 반영되는가"를 먼저 해결하는 데 집중한다.  
vertex layout 일반화, FBX 다중 UV 자산 형식 확장, skeletal 경로 확장은 2단계로 넘긴다.

### 기준 시점

- 기준 시각: `2026-05-11 22:43:11 +09:00`
- 기준 브랜치 HEAD 커밋: `273ba948dc57bfc3273dffa69ab3ee07394d720c`

이 문서는 위 기준 시점의 코드 상태를 전제로 한다. 이후 renderer 관련 변경이 들어온 뒤 다시 사용할 경우, 먼저 변경 내역을 대조하고 순서를 재평가한다.

---

## 2. 진행 원칙

### 원칙 1. Step 종료 조건은 항상 빌드 성공과 최소 동작 검증이다

각 Step은 구현 그 자체보다 검증이 끝나야 완료로 본다.

### 원칙 2. 한 Step 안에서는 한 축만 바꾼다

한 번에 renderer binding, vertex format, importer를 동시에 흔들지 않는다.

### 원칙 3. 기존 경로는 바로 제거하지 않고 우회 경로와 fallback을 먼저 만든다

새 경로를 붙인 뒤, fallback이 있는 상태에서 점진적으로 전환한다.

### 원칙 4. Static Mesh 경로를 먼저 안정화한다

이 단계에서는 skeletal mesh 확장을 목표에 넣지 않는다. static mesh 기준으로 먼저 통과시킨다.

### 원칙 5. 각 Phase마다 반복 가능한 기준 scene을 고정한다

scene, material, mesh 조합을 고정하고 같은 시나리오로 계속 검증한다.

---

## 3. 공통 검증 시나리오

모든 Step에서 가능한 한 아래 시나리오를 반복 사용한다.

### 빌드 검증

- Editor 빌드 성공
- Game 빌드 성공
- shader compile error 없음

### 기본 렌더 검증

- 기본 static mesh가 정상 렌더링됨
- normal map 적용 material이 정상 렌더링됨
- shadow pass가 깨지지 않음
- depth pre / selection / wireframe / scene depth / world normal view mode가 유지됨

### FBX 검증

- 단일 UV FBX 정상
- UV 2개를 가진 테스트 FBX 정상 import
- material slot이 여러 개인 FBX 정상 렌더링

### 권장 기준 자산

- 단순 static mesh 1개
- normal/specular texture를 가진 static mesh 1개
- UV 채널 2개를 가진 FBX 1개
- skeletal mesh 1개

---

## 4. 재사용 전 점검 항목

다른 작업 이후 이 계획을 다시 사용할 때는 먼저 아래를 확인한다.

1. 기준 커밋 `273ba948dc57bfc3273dffa69ab3ee07394d720c` 이후 `CrashEngine/Source/Engine/Render`, `CrashEngine/Source/Engine/Materials`, `CrashEngine/Source/Engine/Mesh`, `CrashEngine/Shaders` 경로의 변경 여부를 비교한다.
2. `BuildDrawCommand.cpp`, `DrawCommandList.cpp`, `MaterialManager.cpp`, `FBXImporter.cpp`, `VertexTypes.h` 및 관련 HLSL 파일의 책임 경계가 바뀌었는지 확인한다.
3. pass override 정책, material `ShaderPath` 처리 여부, reflection 기반 resource binding 경로가 이미 바뀌었는지 확인한다.
4. 기준 검증 scene과 테스트 FBX가 여전히 유효한 자산인지 확인한다.
5. 확인 결과에 따라 Phase 순서와 Step 범위를 다시 조정한다.

즉, 이 문서는 고정 절차서라기보다 기준 시점 코드 상태를 전제로 한 실행 계획이다.

---

## 5. 범위

### 이번 단계에 포함

- shader/pass/material 추적 로그와 mismatch 경고 추가
- pass별 shader override 정책 문서화 및 정리
- material `ShaderPath` 런타임 연동 복구
- reflection 기반 cbuffer / texture binding 경로 복구

### 이번 단계에서 제외

- shader reflection 기반 vertex layout 자동 선택
- FBX UV 채널 일반화
- static/skeletal mesh vertex format 구조 개편
- skeletal mesh용 확장 vertex/runtime 경로

---

## 6. Phase 구성

### Phase 1. 관측성 보강

목표:

- 현재 어떤 pass가 어떤 shader를 최종 선택하는지 추적 가능하게 만든다.
- reflection 결과와 mesh 입력 불일치가 있으면 즉시 보이게 만든다.

#### Step 1-1. shader/pass/material 추적 로그 추가

Task:

- mesh draw command 생성 시 최종 선택 shader 소스를 로그로 남긴다.
- pass override가 발생한 경우 원인 태그를 함께 출력한다.

검증:

- 빌드 성공
- 기준 scene 실행
- Opaque / DepthPre / ShadowMap에서 로그가 정상 출력됨

#### Step 1-2. shader input / mesh vertex mismatch 경고 추가

Task:

- shader reflection 결과와 mesh buffer stride/semantic 계약 불일치 시 warning 또는 assert를 추가한다.

검증:

- 빌드 성공
- 정상 mesh에서는 불필요한 경고가 없음

#### Step 1-3. 기준 scene/asset 세트 고정

Task:

- single UV mesh와 multi-UV 테스트 FBX를 포함한 기준 검증 scene을 확정한다.
- 문서에 scene 이름과 검증 순서를 기록한다.

검증:

- 기준 scene이 항상 열리고 렌더링됨

### Phase 2. Pass Override 정리

목표:

- 어떤 pass가 왜 override를 사용하는지 명시한다.
- 필요한 override와 legacy override를 분리한다.

#### Step 2-1. pass별 shader override 정책 문서화 및 주석 정리

Task:

- `BuildDrawCommand.cpp`의 override 분기를 이유별로 정리한다.
- "필수 override"와 "legacy override"를 구분한다.

검증:

- 빌드 성공
- 동작 변화 없음

#### Step 2-2. 불필요한 legacy override 비활성화 경로 추가

Task:

- Opaque 경로의 불필요한 강제 override를 끌 수 있는 플래그 또는 우회 경로를 추가한다.

검증:

- 빌드 성공
- 플래그 OFF 시 기존 동작 유지
- 플래그 ON 시 기준 scene이 최소 수준으로 렌더링됨

#### Step 2-3. special pass input contract 고정

Task:

- DepthOnly / ShadowEncode가 요구하는 최소 semantic 집합을 문서와 코드 상수로 고정한다.

검증:

- 빌드 성공
- shadow / depth pre / selection 시각 결과가 깨지지 않음

### Phase 3. Material/Shader Ownership 복구

목표:

- `ShaderPath`를 실제 런타임 shader 선택 경로에 연결한다.
- material이 section draw command에 shader ownership을 전달하도록 만든다.

#### Step 3-1. Material JSON에서 `ShaderPath` 읽기

Task:

- material load 경로에서 `ShaderPath`를 파싱해 material 객체에 저장한다.

검증:

- 빌드 성공
- 기존 material 로드 영향 없음

#### Step 3-2. material shader reference 데이터 구조 추가

Task:

- `UMaterial` 또는 별도 render descriptor에 shader path/reference 필드를 추가한다.

검증:

- 빌드 성공
- serialize/load가 깨지지 않음

#### Step 3-3. shader compile/cache와 material 연결

Task:

- material이 자신의 `ShaderPath`에 해당하는 graphics program을 요청할 수 있게 연결한다.

검증:

- 빌드 성공
- 기존 built-in shader 경로 유지
- 테스트 material 1개에서 custom shader program 생성 확인

#### Step 3-4. static mesh section이 material shader를 우선 사용

Task:

- static mesh section draw command가 fallback보다 material shader를 우선 사용하도록 전환한다.

검증:

- 빌드 성공
- 지정한 material만 해당 shader를 사용
- 기존 material은 이전 경로 유지 가능

### Phase 4. Reflection 기반 Resource Binding 복구

목표:

- 고정 `SurfaceMaterial` 중심 경로에서 벗어나 reflection 기반 binding으로 되돌린다.
- 최소한 1개 material/shader 조합이 reflection 기반 cbuffer / texture binding으로 동작하도록 만든다.

#### Step 4-1. reflected cbuffer layout을 사용하는 material template 경로 추가

Task:

- 고정 `SurfaceMaterial` 외에 reflection 기반 template 생성 경로를 추가한다.

검증:

- 빌드 성공
- 기존 고정 template 경로 유지
- reflection 기반 template 생성 로그 확인

#### Step 4-2. texture binding metadata 추출 구조 추가

Task:

- shader reflection 또는 별도 descriptor에서 texture slot/register 요구 정보를 수집한다.

검증:

- 빌드 성공
- 테스트 shader의 texture 요구 목록 출력 가능

#### Step 4-3. draw command의 일반화된 texture binding 구조 추가

Task:

- `Diffuse/Normal/Specular` 고정 필드 외에 일반 SRV binding 배열 또는 테이블을 추가한다.

검증:

- 빌드 성공
- 기존 3-SRV 경로는 계속 정상 동작

#### Step 4-4. static mesh material 1종을 reflection binding 경로로 전환

Task:

- 테스트용 material/shader 1개에 한해 일반 binding 경로를 사용하도록 연결한다.

검증:

- 빌드 성공
- 테스트 material 정상 렌더링
- 나머지 material 영향 없음

---

## 7. 권장 구현 순서

1. 관측성 추가
2. pass override 정책 정리
3. material `ShaderPath` ownership 복구
4. reflection 기반 cbuffer / texture binding 복구

이 순서를 권장하는 이유는, 지금 가장 큰 불확실성이 "reflection 코드가 남아 있는가"보다 "material shader ownership이 실제 렌더 경로에 들어가는가"이기 때문이다.

---

## 8. 완료 기준

### 기능 기준

- Opaque main pass에서 material `ShaderPath`가 실제 shader 선택에 반영된다.
- pass override 정책이 코드와 문서 양쪽에서 설명 가능하다.
- 최소 1개 material/shader 조합이 reflection 기반 cbuffer / texture binding으로 동작한다.

### 운영 기준

- 기준 scene 반복 검증이 가능하다.
- mismatch 발생 시 로그나 assert로 즉시 진단할 수 있다.
- 2단계에서 vertex layout과 asset format 확장으로 넘어갈 수 있는 안정 경계가 생긴다.
