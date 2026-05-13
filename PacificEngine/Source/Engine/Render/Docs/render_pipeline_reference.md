# Render Pipeline Reference

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-25 |
| 최근 수정자 | 김연하 |
| 최근 수정일 | 2026-04-25 |
| 버전 | 1.1 |

## 1. 개요

이 문서는 `RenderPipelineRegistry`와 `RenderPipelineRunner` 기준으로 현재 엔진에 등록된 pipeline과 선택 규칙을 정리한다.

- 구조 기준: `RenderPipelineRegistry.cpp`
- 실행 조건 기준: `RenderPipelineRunner.cpp`
- pass 세부 설명: `render_pass_reference.md`

## 2. Root Pipelines

### 2.1 `DefaultRootPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | 일반 scene 렌더링의 진입점 |
| 자식 | `ScenePipeline` |

### 2.2 `EditorRootPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | scene 렌더링 뒤 editor overlay까지 포함하는 진입점 |
| 자식 | `ScenePipeline`, `OverlayPipeline` |

## 3. Scene Pipelines

### 3.1 `ScenePipeline`

| 항목 | 내용 |
|---|---|
| 역할 | scene rendering 관련 상위 분기 |
| 자식 | `ForwardPipeline`, `PostProcessPipeline` |

### 3.2 `ForwardPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | forward shading path용 하위 분기 |
| 실행 조건 | `RenderPath == Forward` |
| 자식 | `ForwardLitPipeline`, `ForwardUnlitPipeline`, `ForwardWorldNormalPipeline`, `ForwardSceneDepthPipeline` |

## 4. Forward Sub Pipelines

### 4.1 `ForwardLitPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | lit view mode의 forward opaque 경로 |
| 실행 조건 | `RenderPath == Forward && UsesLightingPass(ViewMode)` |
| 자식 | `DepthPrePass`, `ShadowMapPass`, `ForwardOpaquePass` |
| 비고 | forward lit 경로는 scene opaque 렌더 전에 shadow map을 생성한다. |

### 4.2 `ForwardUnlitPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | unlit/wireframe용 forward opaque 경로 |
| 실행 조건 | `RenderPath == Forward && (ViewMode == Unlit || ViewMode == Wireframe)` |
| 자식 | `DepthPrePass`, `ForwardOpaquePass` |

### 4.3 `ForwardWorldNormalPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | forward world normal visualization |
| 실행 조건 | `RenderPath == Forward && ViewMode == WorldNormal` |
| 자식 | `DepthPrePass`, `ForwardOpaquePass` |

### 4.4 `ForwardSceneDepthPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | forward scene depth visualization |
| 실행 조건 | `RenderPath == Forward && ViewMode == SceneDepth` |
| 자식 | `DepthPrePass`, `NonLitViewModePass` |

## 5. Post / Overlay Pipelines

### 5.1 `PostProcessPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | scene 결과에 공통 후처리를 적용 |
| 자식 | `HeightFogPass`, `FXAAPass` |

### 5.2 `OverlayPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | editor/debug overlay 렌더링 |
| 자식 | `LightHitMapPass`, `DebugLinePass`, `OutlinePipeline`, `OverlayBillboardPass`, `GizmoPass`, `OverlayTextPass` |

### 5.3 `OutlinePipeline`

| 항목 | 내용 |
|---|---|
| 역할 | selection outline 전용 하위 분기 |
| 자식 | `SelectionMaskPass`, `OutlinePass` |

### 5.4 `PresentPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | viewport target을 최종 backbuffer 또는 present target으로 복사/출력 |
| 자식 | `PresentPass` |

## 6. ViewMode 매핑 요약

| ViewMode | Pipeline |
|---|---|
| `Lit_Lambert` | `ForwardLitPipeline` |
| `Lit_Phong` | `ForwardLitPipeline` |
| `Unlit` | `ForwardUnlitPipeline` |
| `Wireframe` | `ForwardUnlitPipeline` |
| `WorldNormal` | `ForwardWorldNormalPipeline` |
| `SceneDepth` | `ForwardSceneDepthPipeline` |

## 7. 참고 파일

- `CrashEngine/Source/Engine/Render/Execute/Registry/RenderPipelineType.h`
- `CrashEngine/Source/Engine/Render/Execute/Registry/RenderPipelineRegistry.cpp`
- `CrashEngine/Source/Engine/Render/Execute/Runner/RenderPipelineRunner.cpp`
- `CrashEngine/Source/Engine/Render/Docs/render_execution.md`
- `CrashEngine/Source/Engine/Render/Docs/render_pass_reference.md`

`ForwardWorldNormalPipeline`은 `ForwardOpaquePass`의 world normal variant가 viewport color에 직접 쓰는 direct output 경로를 사용한다. intermediate normal texture를 쓰고 다시 읽는 fullscreen post process를 생략해 bandwidth 비용을 줄이는 것이 목적이다.

Deferred path는 현재 mainline에서 제거되었고, 복원 노트는 `render_deferred_reconstruction_notes.md`에만 남겨둔다.
