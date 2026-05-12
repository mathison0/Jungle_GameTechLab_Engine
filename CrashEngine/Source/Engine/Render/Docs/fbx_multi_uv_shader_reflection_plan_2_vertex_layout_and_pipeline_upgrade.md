# FBX Multi-UV / Shader Reflection 2단계 계획

## 1. 목표

이 계획의 목표는 1단계에서 복구한 material/shader 연동 위에, vertex layout과 asset pipeline을 일반화해 FBX 입력의 폭을 넓히는 것이다.

- shader reflection 기반 vertex layout 기능을 추가한다.
- FBX에서 들어오는 다양한 vertex input을 소화할 수 있도록 렌더 파이프라인을 확장한다.

이 단계는 단순 UV2 대응을 넘어, 장기적으로 `Material -> Shader`만 지정해도 vertex layout / resource binding / asset 전달이 함께 맞물리도록 만드는 구조 개선이다.

### 기준 시점

- 기준 시각: `2026-05-11 22:43:11 +09:00`
- 기준 브랜치 HEAD 커밋: `273ba948dc57bfc3273dffa69ab3ee07394d720c`

이 문서는 위 기준 시점의 코드 상태를 전제로 한다. 다만 실제 착수 시에는 1단계 결과를 먼저 반영한 뒤, Phase 순서를 다시 확인한다.

---

## 2. 진행 원칙

### 원칙 1. Step 종료 조건은 항상 빌드 성공과 최소 동작 검증이다

각 Step은 구현 그 자체보다 검증이 끝나야 완료로 본다.

### 원칙 2. 한 Step 안에서는 한 축만 바꾼다

vertex format, importer, serialization, draw binding을 한 번에 바꾸지 않는다.

### 원칙 3. 기존 경로는 바로 제거하지 않고 우회 경로와 fallback을 먼저 만든다

새 canonical/superset 경로를 먼저 붙이고, 이후 단계적으로 기본값을 전환한다.

### 원칙 4. Static Mesh 경로를 먼저 검증한 뒤 Skeletal Mesh로 확장한다

skeletal mesh는 CPU skinning과 runtime vertex 변환까지 연결되어 있으므로 항상 후순위로 둔다.

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
3. 1단계에서 복구한 material `ShaderPath`, reflection binding, pass override 정책이 유지되는지 확인한다.
4. 기준 검증 scene과 테스트 FBX가 여전히 유효한 자산인지 확인한다.
5. 확인 결과에 따라 Phase 순서와 Step 범위를 다시 조정한다.

즉, 이 문서는 고정 절차서라기보다 기준 시점 코드 상태를 전제로 한 구조 확장 계획이다.

---

## 5. 범위

### 이번 단계에 포함

- reflection 결과를 vertex layout key로 바꾸는 경로
- canonical 또는 superset vertex descriptor 도입
- static mesh의 UV2 이상 경로 검증
- FBX importer / asset serialization / runtime upload 확장
- skeletal mesh 경로 확장
- fallback 경로 정리와 운영 가이드 정리

### 이번 단계의 전제

- 1단계에서 material `ShaderPath`와 reflection 기반 resource binding이 최소 1개 경로에서 정상 동작해야 한다.

---

## 6. Phase 구성

### Phase 1. Vertex Layout 일반화 설계

목표:

- shader가 요구하는 semantic 집합을 런타임 vertex layout 선택 기준으로 사용할 수 있게 만든다.
- CPU struct와 shader input signature를 직접 결박하는 구조를 완화한다.

#### Step 1-1. vertex semantic descriptor 도입

Task:

- CPU vertex struct와 분리된 vertex semantic descriptor 타입을 추가한다.

검증:

- 빌드 성공
- 기존 동작 변화 없음

#### Step 1-2. shader reflection 결과를 vertex layout key로 변환

Task:

- input semantic 목록에서 정규화된 vertex layout key를 생성한다.

검증:

- 빌드 성공
- 기존 shader들에 대해 key 생성 로그 확인

#### Step 1-3. canonical 또는 superset static mesh vertex 경로 추가

Task:

- `FVertexPNCT_T` 직접 의존 경로 대신 UV1 이상을 담을 수 있는 중간 descriptor 또는 superset vertex 경로를 추가한다.

검증:

- 빌드 성공
- 기존 single UV mesh 정상

### Phase 2. Static Mesh UV2 Prototype

목표:

- static mesh 1종에서 UV2를 실제로 shader까지 전달한다.
- 2단계 전체 확장의 첫 실증 케이스를 만든다.

#### Step 2-1. static mesh 1종용 UV2 포함 vertex buffer 생성

Task:

- 테스트 FBX/static mesh에 한해 UV2를 포함하는 vertex buffer 생성 경로를 추가한다.

검증:

- 빌드 성공
- UV2 없는 mesh 정상
- UV2 있는 mesh에서 buffer 생성 확인

#### Step 2-2. main opaque shader 1종에서 `TEXCOORD1` 사용

Task:

- 테스트 shader 1개에서 `TEXCOORD1`을 실제로 사용하도록 연결한다.

검증:

- 빌드 성공
- UV2 테스트 material 시각 결과 확인

### Phase 3. FBX / Asset Format 확장

목표:

- importer 단계부터 runtime까지 UV 채널이 보존되도록 만든다.

#### Step 3-1. FBX importer가 UV set 개수를 수집

Task:

- importer가 UV set 목록과 개수를 읽고 기록하도록 확장한다.

검증:

- 빌드 성공
- 테스트 FBX의 UV set 개수 로그 확인

#### Step 3-2. FBX importer가 UV0, UV1을 실제 저장

Task:

- 우선 2채널까지만 importer 결과 구조에 저장한다.

검증:

- 빌드 성공
- UV2 테스트 FBX import 성공

#### Step 3-3. static mesh asset serialization 확장

Task:

- UV1 데이터가 asset cache/bin에 저장되도록 serialization을 확장한다.

검증:

- 빌드 성공
- 재로드 후에도 UV1 유지

#### Step 3-4. tangent 생성 기준 UV set 명시

Task:

- tangent 생성과 참조가 UV0 기준인지 UV1 기준인지 명확히 고정한다.

검증:

- 빌드 성공
- normal map이 있는 mesh에서 시각 문제 없음

### Phase 4. Skeletal Mesh 경로 확장

목표:

- static mesh에서 검증한 구조를 skeletal mesh까지 옮긴다.

#### Step 4-1. skeletal mesh 축소 경로 관측 로그 추가

Task:

- `FVertexSkinned -> FVertexPNCT_T` 축소 경로에 로그 또는 경고를 추가한다.

검증:

- 빌드 성공
- 기존 skeletal mesh 렌더 유지

#### Step 4-2. skeletal runtime vertex에 UV1 보존 경로 추가

Task:

- CPU skinning 결과 또는 intermediate vertex에 UV1 이상을 보존할 필드를 추가한다.

검증:

- 빌드 성공
- 기존 skeletal mesh 렌더 유지

#### Step 4-3. skeletal mesh buffer가 확장 vertex format을 수용

Task:

- skeletal buffer update/create 경로를 새 descriptor 또는 vertex format에 맞게 확장한다.

검증:

- 빌드 성공
- skeletal mesh 1종 정상 렌더링

#### Step 4-4. skeletal material/shader 1종을 일반 경로로 전환

Task:

- 테스트용 skeletal material/shader 1개를 일반화된 경로로 연결한다.

검증:

- 빌드 성공
- animation 재생 중 정상 렌더링

### Phase 5. 정리 및 전환 판단

목표:

- fallback 제거 가능 여부를 판단하고, 운영 규칙을 문서화한다.

#### Step 5-1. fallback 경로 제거 여부 판단

Task:

- 고정 `SurfaceMaterial` / 고정 3-SRV / 고정 vertex struct 경로를 계속 유지할지 판단한다.

검증:

- 남겨야 할 경로와 제거 가능한 경로 목록 정리

#### Step 5-2. 기존 material의 점진 전환

Task:

- static mesh material 일부를 새 경로로 전환한다.

검증:

- 각 전환 묶음마다 scene 검증 통과

#### Step 5-3. 문서와 운영 가이드 정리

Task:

- shader authoring 규칙
- semantic naming 규칙
- material 작성 규칙
- FBX import 규칙

검증:

- 동일 규칙으로 asset/shader를 추가할 수 있음

---

## 7. 권장 구현 순서

1. vertex semantic descriptor와 layout key 도입
2. static mesh UV2 prototype 완성
3. FBX importer / asset serialization 확장
4. skeletal mesh 경로 확장
5. fallback 제거 여부 판단과 운영 문서 정리

이 순서를 권장하는 이유는, importer를 먼저 넓혀도 renderer가 받지 못하면 검증 가치가 낮고, skeletal을 먼저 건드리면 실패 범위가 너무 커지기 때문이다.

---

## 8. 완료 기준

### 기능 기준

- shader reflection 결과로 vertex layout key를 만들 수 있다.
- static mesh 1종에서 UV2가 import -> asset -> runtime -> shader까지 전달된다.
- skeletal mesh 1종이 확장 경로에서 정상 동작한다.

### 구조 기준

- 특정 CPU vertex struct에 renderer 전체가 고정되어 있지 않다.
- FBX 입력 형식 확장 시 importer, asset, runtime 경로가 같은 모델로 설명 가능하다.
- 이후 UV3 이상 또는 추가 semantic 확장 시 수정 지점이 예측 가능하다.
