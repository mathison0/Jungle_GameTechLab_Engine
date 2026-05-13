# Rendering Pipeline Construction

> Companion to [render_pipeline_anatomy.md](render_pipeline_anatomy.md).
> The anatomy doc describes *what* the renderer does each frame.
> This doc describes *how* the pieces are built and wired together — the shader reflection system, the pipeline tree, and how SRVs / DSVs / constant buffers / samplers are created, cached, and bound.

---

## 1. Big Picture

There are three orthogonal "build" systems that together define the renderer:

| System | Source | What it builds | Lives in |
|---|---|---|---|
| Shader reflection | HLSL files | `FGraphicsProgram` (VS+PS+InputLayout+CB layout) | [Render/RHI/D3D11/Shaders/](../RHI/D3D11/Shaders/) + [Render/Resources/Shaders/](../Resources/Shaders/) |
| Pipeline tree | C++ tables | nested `FRenderPipelineDesc` of pipeline / pass nodes | [Render/Execute/Registry/](../Execute/Registry/) |
| Resource pools | runtime D3D11 | constant buffers, structured buffers, samplers, render targets | [Render/Resources/](../Resources/) + [Render/RHI/D3D11/](../RHI/D3D11/) |

They are independent in source but converge inside `FDrawCommand` and `FRenderPipelineContext`:
the **command** points at a shader (reflection output), a per-object CB (resource pool), and a logical pass; the **context** carries the registries and the frame-scoped resources to every node in the **pipeline tree**.

```text
HLSL file ─► D3DCompile ─► ID3DBlob ─► D3DReflect ─► FGraphicsProgram
                                                       │
                                                       ▼
            FDrawCommand ◄──── BuildMeshDrawCommand ──┘
                  │                                    
                  ▼                                    
        FDrawCommandList (sorted, ranged by Pass)      
                  │                                    
                  ▼                                    
FRenderPipelineRunner ◄── FRenderPipelineRegistry      
                  │   ◄── FRenderPassRegistry          
                  ▼                                    
            Pass.Execute() ── binds CBs/SRVs/Samplers/RTs
```

---

## 2. Shader Reflection System

### 2.1 The descriptors that drive compilation

Built-in shaders are not loaded by name strings scattered through the codebase. Each one is referenced by a stable enum key, [`EShaderType`](../Execute/Registry/ShaderProgramTypes.h), and described once in [`FShaderProgramRegistry::Initialize`](../Execute/Registry/ShaderProgramRegistry.cpp).

A descriptor is a small POD: file path, entry point, optional defines.

```cpp
struct FShaderStageDesc {
    FString                      FilePath;     // e.g. "Shaders/Render/Editor/Primitive.hlsl"
    FString                      EntryPoint;   // e.g. "VS" / "PS"
    TArray<FShaderCompileDefine> Defines;      // becomes D3D_SHADER_MACRO[]
};

struct FGraphicsProgramDesc {
    FString                     DebugName;
    FShaderStageDesc            VS;            // required
    TOptional<FShaderStageDesc> PS;            // optional → VS-only programs (depth, shadow VS)
};
```

Variants of the same shader (e.g. `ShadowEncodeVSM` vs `ShadowEncodeESM`, `SceneDepth` vs `NormalView`) are just two registry entries that share a file but inject different defines through `AddDefine`. The HLSL file then uses `#ifdef` to select the right code path.

The defines `SHADER_STAGE_VERTEX` and `SHADER_STAGE_PIXEL` are added automatically inside `FGraphicsProgram::CompileVertexShader / CompilePixelShader`, so a single HLSL file can host both stages and choose the right code with `#ifdef SHADER_STAGE_VERTEX`.

### 2.2 ShaderManager — owner, cache, hot-reloader

[`FShaderManager`](../Resources/Shaders/ShaderManager.h) is a singleton that owns:

- one `FGraphicsProgram` per `EShaderType` (`Shaders[(uint32)EShaderType::MAX]`)
- a parallel `BuiltInShaderFiles[]` array of file dependencies
- a `CustomShaderCache` map keyed by normalized file path for ad-hoc shaders

The lifecycle is:

1. `Initialize(Device)` → builds the registry table, no compiles yet.
2. `GetShader(EShaderType)` → first-touch lazy compile via `RefreshBuiltInShader`.
3. `TickHotReload()` (debug builds only, every frame) → walks every entry, asks `ShaderDependencyUtils::HasDependencyChanged` whether any of the tracked files have a newer write time, and recompiles only the changed ones.

Custom shaders take a similar but separate path through `CreateCustomShader / GetCustomShader`. They use `FCustomShaderCacheEntry` so the editor (e.g. node-graph or material editor) can hand a raw `.hlsl` path to the renderer without touching the registry.

### 2.3 Compilation — what `FShaderProgramBase::CompileShaderBlob` actually does

The compile path lives in [`FShaderProgramBase::CompileShaderBlobStandalone`](../RHI/D3D11/Shaders/ShaderProgramBase.cpp):

1. Resolve the file path to an absolute, canonical path so `D3DCompileFromFile` and the include loader agree.
2. Build a stable cache signature from `(absolute path, entry point, target, compile flags, defines)`.
3. **Disk cache lookup.** `TryLoadCachedShaderBlob` reads `<sanitized-name>__<entry>__<target>__<hash>.cso` from `FPaths::ShaderCacheDir()`. If a sibling `.meta` file's stored dependency hash matches the file's current dependency hash, `D3DReadFileToBlob` returns the cached blob and we skip the compiler entirely.
4. **Compile.** Otherwise `D3DCompileFromFile` runs with an [`FShaderIncludeLoader`](../Resources/Shaders/ShaderIncludeLoader.h) that records every transitively included file path into a dependency set.
5. **Cache write.** On success, `D3DWriteBlobToFile` saves the `.cso` and a `.meta` carrying the dependency hash.

Two engineering details that matter:

- The cache signature embeds the compile flags, so Debug (`D3DCOMPILE_DEBUG | SKIP_OPTIMIZATION`) and Release blobs sit in different files and never collide.
- `FShaderIncludeLoader::ResolveIncludePath` searches the file's own directory, the root shader directory, and an upwards `Shaders/` chain. That is why HLSL includes like `#include "Common/ConstantBuffers.hlsl"` work regardless of where the including file lives.

### 2.4 Reflection — input layouts and CB layouts come from the blob

Once the VS blob exists, [`FGraphicsProgram::Create`](../RHI/D3D11/Shaders/GraphicsProgram.cpp) does **two** reflection passes on it.

**(a) Vertex input layout** — `CreateInputLayoutFromReflection`:

```cpp
ID3D11ShaderReflection* Reflector;
D3DReflect(VSBlob, ..., &Reflector);
Reflector->GetDesc(&ShaderDesc);

for (UINT i = 0; i < ShaderDesc.InputParameters; ++i) {
    D3D11_SIGNATURE_PARAMETER_DESC ParamDesc;
    Reflector->GetInputParameterDesc(i, &ParamDesc);
    if (ParamDesc.SystemValueType != D3D_NAME_UNDEFINED) continue;  // skip SV_*

    D3D11_INPUT_ELEMENT_DESC Elem = {};
    Elem.SemanticName      = <copied semantic>;
    Elem.SemanticIndex     = ParamDesc.SemanticIndex;
    Elem.Format            = MaskToFormat(ParamDesc.ComponentType, ParamDesc.Mask);
    Elem.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    Elements.push_back(Elem);
}
Device->CreateInputLayout(Elements.data(), ..., &InputLayout);
```

The renderer never hand-authors `D3D11_INPUT_ELEMENT_DESC` arrays. The vertex layout is whatever the VS declares, with `MaskToFormat` translating component-count×type into the matching `DXGI_FORMAT`. This is why static and skeletal vertex layouts will differ purely by what the VS function signature declares — no editor-side table change is needed.

**(b) Material parameter layout** — `ExtractCBufferInfo` (called for both VS and PS blobs):

```cpp
D3DReflect(blob, ..., &Reflector);
Reflector->GetDesc(&ShaderDesc);

for (UINT i = 0; i < ShaderDesc.ConstantBuffers; ++i) {
    auto* CB = Reflector->GetConstantBufferByIndex(i);
    D3D11_SHADER_BUFFER_DESC CBDesc; CB->GetDesc(&CBDesc);

    D3D11_SHADER_INPUT_BIND_DESC BindDesc;
    Reflector->GetResourceBindingDescByName(CBDesc.Name, &BindDesc);

    if (BindDesc.BindPoint != 2 && BindDesc.BindPoint != 3) continue;  // material slots only

    for (UINT j = 0; j < CBDesc.Variables; ++j) {
        D3D11_SHADER_VARIABLE_DESC VarDesc; CB->GetVariableByIndex(j)->GetDesc(&VarDesc);
        ShaderParameterLayout[VarDesc.Name] = new FMaterialParameterInfo {
            .BufferName = CBDesc.Name,
            .SlotIndex  = BindDesc.BindPoint,   // b2 or b3
            .Offset     = VarDesc.StartOffset,
            .Size       = VarDesc.Size,
            .BufferSize = CBDesc.Size,
        };
    }
}
```

The result is a `TMap<FString, FMaterialParameterInfo*>` keyed by *parameter name*. `UMaterial::SetScalarParameter("Roughness", 0.4f)` does not need to know that `Roughness` lives at offset 32 of `cbuffer MaterialParameters : register(b3)` — the layout map already has that. Material setters can therefore upload to the right CB / offset by name.

The hard-coded `BindPoint == 2 || 3` filter is intentional. Frame and per-object CBs (b0/b1) are written by the renderer itself and don't need name-based lookup. b2/b3 are the user-facing material slots; reflecting only those keeps `ShaderParameterLayout` small and avoids leaking engine CB names into the material API.

### 2.5 Shader variant cache — for view modes

Beyond built-in shaders, view-mode shader variants live in [`FShaderVariantCache`](../Resources/Shaders/ShaderVariantCache.h). Its key is `<file>|VS=<entry>|PS=<entry>|<defines>` and `GetOrCreate` will compile-on-demand and cache by that key. `FViewModePassRegistry` (described in the anatomy doc) calls into this cache to pick the right shader for the active view mode without polluting the main registry table.

### 2.6 Hot-reload mechanic

Each cached shader keeps an `FShaderFileDependency` snapshot — file paths plus a hash derived from the include set and last-write times. `TickHotReload` runs once per frame in `_DEBUG`. When a file changes:

1. `HasDependencyChanged` returns true.
2. `Create` is called again with the same desc.
3. The new VS/PS/InputLayout/parameter-layout is built; the **previous** program's resources are released only after the new compile succeeds.
4. The on-disk cache is updated.

Because the variant cache and built-in cache are queried by stable keys (enum or hash), all consumers get the new program automatically the next time they call `GetShader / GetOrCreate`. No pass needs to know hot-reload happened.

---

## 3. Pipeline Tree

The renderer's frame is a tree, not a fixed function. The tree is defined by data tables in the registries; the runner just walks them.

### 3.1 Two registries

| Registry | Stored as | Keyed by | What it returns |
|---|---|---|---|
| [`FRenderPassRegistry`](../Execute/Registry/RenderPassRegistry.h) | `TMap<int32, FRenderPass*>` | `ERenderPassNodeType` | a heap-allocated concrete pass instance |
| [`FRenderPipelineRegistry`](../Execute/Registry/RenderPipelineRegistry.h) | `TMap<int32, FRenderPipelineDesc>` | `ERenderPipelineType` | a list of `FRenderNodeRef` children |

A `FRenderNodeRef` is a tagged union: either a child *pipeline* or a child *pass*.

```cpp
enum class ERenderNodeKind { Pipeline, Pass };

struct FRenderNodeRef {
    ERenderNodeKind Kind;
    int32           TypeValue;   // cast to ERenderPipelineType or ERenderPassNodeType
};

struct FRenderPipelineDesc {
    ERenderPipelineType    Type;
    TArray<FRenderNodeRef> Children;
};
```

This is enough to express arbitrary nesting. There is no separate "pass group" concept — a pipeline is just a node whose children are themselves nodes.

### 3.2 Building the tree

The whole tree is constructed once in [`FRenderPipelineRegistry::Initialize`](../Execute/Registry/RenderPipelineRegistry.cpp) using a single helper:

```cpp
auto AddPipeline = [this](ERenderPipelineType Type, std::initializer_list<FRenderNodeRef> Nodes) {
    Pipelines.emplace((int32)Type, FRenderPipelineDesc { Type, { Nodes.begin(), Nodes.end() } });
};

AddPipeline(ERenderPipelineType::EditorRootPipeline, {
    PipelineNode(ERenderPipelineType::ScenePipeline),
    PipelineNode(ERenderPipelineType::OverlayPipeline),
});

AddPipeline(ERenderPipelineType::ForwardLitPipeline, {
    PassNode(ERenderPassNodeType::DepthPrePass),
    PassNode(ERenderPassNodeType::ShadowMapPass),
    PassNode(ERenderPassNodeType::ForwardOpaquePass),
    PassNode(ERenderPassNodeType::AlphaBlendPass),
    PassNode(ERenderPassNodeType::SubUVPass),
});
```

Reading this file is the most direct way to see what the engine actually runs. The full registered tree is enumerated in [render_pipeline_anatomy.md §9](render_pipeline_anatomy.md#9-registered-pipeline-tree).

The `FRenderPassRegistry::Initialize` companion is just a pile of `Passes.emplace((int32)NodeType, new FConcretePass())` lines. Adding a pass = (a) declare the enum, (b) `new` it here, (c) reference it from a pipeline.

### 3.3 Walking the tree

[`FRenderPipelineRunner::ExecutePipelineRecursive`](../Execute/Runner/RenderPipelineRunner.cpp) is small enough to read in full:

```cpp
const FRenderPipelineDesc* Desc = PipelineRegistry.FindPipeline(Type);
for (const FRenderNodeRef& Child : Desc->Children) {
    if (Child.Kind == ERenderNodeKind::Pipeline) {
        if (ShouldExecutePipeline(Context, (ERenderPipelineType)Child.TypeValue))
            ExecutePipelineRecursive((ERenderPipelineType)Child.TypeValue, ...);
    } else {
        const ERenderPassNodeType PassType = (ERenderPassNodeType)Child.TypeValue;
        if (!ShouldExecutePass(Context, PassType)) continue;
        if (FRenderPass* Pass = PassRegistry.FindPass(PassType)) {
            FScopedGpuEvent Event(*Context.Renderer, GetRenderPassMarkerName(PassType));
            Pass->Execute(Context);
        }
    }
}
```

Two gates are applied per child:

- `ShouldExecutePipeline` — keyed off the active view mode and pipeline availability. Example: `ForwardLitPipeline` runs only when `ViewModeRegistry->UsesLightingPass(ActiveViewMode)` is true.
- `ShouldExecutePass` — keyed off `FViewModePassRegistry` flags. `DepthPrePass` only runs if the view mode says it needs depth pre; `FinalPostProcessCompositePass` only runs if any post-process toggle is on.

This is the runtime equivalent of "compile-out" branching. The tree is statically described, but the runner skips entire subtrees when their gate is false. That keeps "what runs in mode X" derivable from data, not buried in `if` chains inside passes.

### 3.4 Pass interface — four-stage lifecycle

Every concrete pass derives from [`FRenderPass`](../Execute/Passes/Base/RenderPass.h):

```cpp
class FRenderPass {
public:
    virtual void PrepareInputs(FRenderPipelineContext&)  = 0;
    virtual void PrepareTargets(FRenderPipelineContext&) = 0;
    virtual void BuildDrawCommands(FRenderPipelineContext&)                                = 0;
    virtual void BuildDrawCommands(FRenderPipelineContext&, const FPrimitiveProxy& Proxy)  = 0;
    virtual void SubmitDrawCommands(FRenderPipelineContext&) = 0;

    virtual void Execute(FRenderPipelineContext& Context) {
        PrepareInputs(Context);
        PrepareTargets(Context);
        SubmitDrawCommands(Context);
    }
};
```

The two `BuildDrawCommands` overloads are called *before* the runner walks the tree, during `FRenderer::BuildDrawCommands` (proxy-driven) and per-pass fullscreen/overlay command emission. The proxy-overload is invoked once per visible primitive proxy; the no-arg overload is for fullscreen passes that don't iterate proxies.

The `Execute` body intentionally calls only the three runtime-stage methods. By the time the runner reaches a pass, all its commands already exist in the sorted `FDrawCommandList`; the pass only needs to bind targets and submit its range.

`MeshPassBase` adds a default `SubmitDrawCommands` that grabs `(start, end)` from `DrawCommandList.GetPassRange(MyPass)` and walks `SubmitCommand(...)` for each entry, using `FDrawSubmitStateCache` to avoid redundant binds.

---

## 4. Resource Management

D3D11 resources fall into a few buckets in this engine, each with its own ownership model. The split is deliberate: long-lived state objects live in singletons or in the device wrapper; per-frame uploads live in `FFrameResources`; viewport-local copies live in `FViewportRenderTargets`.

### 4.1 Constant Buffers

There are three pools:

| Pool | Owner | Allocation policy | Used for |
|---|---|---|---|
| Built-in CBs | `FFrameResources` | created once at `Create(Device)` | `FrameBuffer` (b0), `PerObjectConstantBuffer` (b1, fallback), `GlobalLightBuffer` (b4), `ShadowPassBuffer` (b5) |
| Per-object CB pool | `FFrameResources::PerObjectCBPool` | grow-on-demand `TArray<FConstantBuffer>` indexed by `FPrimitiveProxy::ProxyId` | each visible proxy gets its own b1 binding |
| Generic slot cache | [`FConstantBufferCache`](../Resources/Buffers/ConstantBufferCache.h) | `TMap<uint32 slot, FConstantBuffer>` | passes that need a CB at a specific slot without pre-declaring it (decals, fog, outlines, …) |

Slot constants are centralized in [`RenderBindingSlots.h`](../Resources/Bindings/RenderBindingSlots.h) — `ECBSlot::Frame=0`, `ECBSlot::PerObject=1`, `ECBSlot::PerShader0=2`, etc.

The per-object pool pattern is the load-bearing one. `GetPerObjectCBForProxy(Device, Proxy)` calls `EnsurePerObjectCBPoolCapacity(Device, Proxy.ProxyId + 1)` and returns `&PerObjectCBPool[Proxy.ProxyId]`. Two consequences:

- the same proxy gets the same `ID3D11Buffer*` every frame, so the submit-state cache can detect "same b1 already bound, skip rebind";
- the pool only ever grows. Releasing happens at `FFrameResources::Release`, not per-frame.

`FCBBindingEntry` is a small inline-storage helper used inside `FDrawCommand` for one-off CB payloads. It carries `Buffer + Slot + Size` plus 128 bytes of inline data, so a fullscreen pass can author a `cbuffer` value at command-build time without allocating.

### 4.2 Shader Resource Views (SRVs)

There is no central SRV pool. SRVs are created and owned by whoever produces the underlying resource:

| SRV | Created by | Bound at | Lifetime |
|---|---|---|---|
| Material Diffuse/Normal/Specular | texture loader (`UTexture2D`) | t-slots in mesh draw command | with the texture asset |
| `LocalLightSRV` | `FFrameResources::UpdateLocalLights` | `t6` | grow-on-demand structured buffer; rebuilt only when capacity increases |
| `ShadowAtlasBase` array (t20–t23) | shadow atlas builder | bound by lighting/opaque passes | atlas lifetime |
| Scene depth/color copies | `FViewportRenderTargets` | `t10` / `t11` | viewport lifetime |
| Tile light mask / debug hit map | `FTileBasedLightCulling` | `t7` / `t8` | viewport lifetime, recreated on resize |

`FFrameResources::UpdateLocalLights` is a representative example of a structured-buffer SRV pool. When the count exceeds capacity it releases both `LocalLightBuffer` and `LocalLightSRV`, allocates a `D3D11_USAGE_DYNAMIC` structured buffer rounded up (min 8), and creates a fresh SRV against it. Subsequent frames just `Map`/`memcpy` into the existing buffer.

The most important bit-of-discipline around SRVs is **null-binding before becoming a render target**. Any texture used as a target this pass that was an SRV in the previous pass must be unbound. You'll see this pattern repeatedly inside passes:

```cpp
ID3D11ShaderResourceView* NullSRVs[8] = {};
Context->PSSetShaderResources(0, ARRAYSIZE(NullSRVs), NullSRVs);
// ... now safe to OMSetRenderTargets(... that texture's RTV ...)
```

D3D11 silently unbinds SRVs that conflict with a new RTV, but it produces debug-layer warnings; the renderer pre-empts that by clearing slots explicitly.

### 4.3 Render Target Views (RTVs) and Depth Stencil Views (DSVs)

Render targets are owned in two places:

- **Viewport targets** — [`FViewportRenderTargets`](../Execute/Context/Viewport/ViewportRenderTargets.h) holds the per-viewport color RTV/DSV, the depth copy texture+SRV, and the scene color copy texture+SRV. Sized to the viewport, recreated on resize.

The `FSurfaceTexture` struct deserves attention because it embodies the engine's RTV ↔ SRV duality: every offscreen color texture exists with both views from the moment it's created, so a pass can write through `RTV` and a later pass can read through `SRV` without re-creating views. The cost of carrying both views is paid once at allocation.

The **frame's target handoff** is described in [render_pipeline_anatomy.md §19](render_pipeline_anatomy.md#19-target-handoffs). The build-side rules:

1. Anything that wants to read scene depth or color reads from the **copy** texture, not the live one. The copy is populated by `Context->CopyResource(...)` when the producing pass finishes.
2. `OMSetRenderTargets` is always paired with explicit SRV null-binding for the textures being written.
3. `BeginFrame` / `PresentPass` are the only places that touch the swapchain target directly.

### 4.4 Samplers

Samplers are created once at `FFrameResources::Create` and bound globally each frame:

```cpp
void FFrameResources::BindSystemSamplers(ID3D11DeviceContext* Ctx) {
    ID3D11SamplerState* Samplers[4] = {
        LinearClampSampler,   // s0
        LinearWrapSampler,    // s1
        PointClampSampler,    // s2
        ShadowSampler,        // s3 — D3D11_FILTER_COMPARISON_*, COMPARISON_GREATER_EQUAL (reversed-Z)
    };
    Ctx->VSSetSamplers(0, 4, Samplers);
    Ctx->PSSetSamplers(0, 4, Samplers);
}
```

Slot constants are in `ESamplerSlot::*`. There's no per-pass sampler binding API — every shader uses these four samplers and references them by register. The shadow comparison sampler in `s3` is configured for reversed-Z (greater-equal compare), which is why opaque/lighting shaders that sample shadow atlases just use `SamplerComparisonState` at `s3` directly.

### 4.5 Pipeline State Objects

D3D11 has no PSO type, so the engine emulates one with three small managers:

| Manager | Pre-built objects | `Set()` behavior |
|---|---|---|
| [`FBlendStateManager`](../RHI/D3D11/State/BlendStateManager.h) | `Alpha`, `Additive`, `NoColorWrite` | caches `CurrentState`, no-op on repeat |
| [`FDepthStencilStateManager`](../RHI/D3D11/State/DepthStencilStateManager.h) | `Default`, `DepthReadOnly`, `StencilWrite`, `StencilMaskEqual`, `NoDepth`, `GizmoInside`, `GizmoOutside` | same |
| [`FRasterizerStateManager`](../RHI/D3D11/State/RasterizerStateManager.h) | `BackCull`, `FrontCull`, `NoCull`, `WireFrame` | same |

All three states are baked at device init and never recreated. `Set` takes an enum, looks up the matching `ID3D11*State*`, and calls `OMSetBlendState / OMSetDepthStencilState / RSSetState` only when the value differs from the cached one.

`FDrawCommand` carries `EDepthStencilState`, `EBlendState`, `ERasterizerState`, plus a topology and a stencil ref. `SubmitCommand` consults `FDrawSubmitStateCache` to skip redundant `Set` calls within a sorted command range. Combined with `FDrawCommand::BuildSortKey` (which packs Pass→User→Shader→Mesh→Material into 64 bits), this means the GPU sees long runs of identical pipeline state inside each pass range — the closest D3D11 can get to PSO batching.

The default state per logical `ERenderPass` is set up by `InitializeDefaultRenderPassPresets` (see [render_pipeline_anatomy.md §18](render_pipeline_anatomy.md#18-state-presets)). Mesh sections can override depth/blend/rasterizer per command unless the pass is mask-like.

---

## 5. How a Frame Pulls It All Together

For a single mesh in `ForwardLitPipeline`:

1. **At engine init.** `FShaderProgramRegistry::Initialize` registers `EShaderType::StaticMesh → "Shaders/Render/Editor/Primitive.hlsl"`. `FRenderPipelineRegistry` registers the forward-lit subtree. `FRenderPassRegistry` `new`s the forward pass set. `FFrameResources::Create` allocates samplers, the four built-in CBs, and the empty per-object pool.
2. **First time the mesh is visible.** `FShaderManager::GetShader(StaticMesh)` triggers `FGraphicsProgram::Create`: compile (or load cached) `Primitive.hlsl` for VS+PS, reflect input layout, reflect b2/b3 to populate `ShaderParameterLayout`. Result is cached for the rest of the run.
3. **Each frame, build phase.** `BuildMeshDrawCommand` allocates the proxy's per-object CB from `FFrameResources::PerObjectCBPool[ProxyId]`, picks the right shader (here `StaticMesh`, but `DepthOnly` for the depth pre-pass entry), fills `FDrawCommand` with material SRVs and CBs, and pushes it into `FDrawCommandList` with a sort key whose top byte is the logical `ERenderPass` value.
4. **Each frame, sort phase.** `FDrawCommandList::Sort` orders commands by `SortKey` and computes `PassOffsets`, so each pass can ask for `(start, end)` without scanning.
5. **Each frame, execute phase.** `FRenderPipelineRunner::ExecutePipelineRecursive(EditorRootPipeline)` walks the tree. For forward lit it descends into `ScenePipeline → ForwardPipeline → ForwardLitPipeline`. At `ForwardOpaquePass`, `Execute` calls `PrepareInputs` (binds shadow atlases, light CBs, tile light culling resources, …), `PrepareTargets` (binds the viewport color target), and `SubmitDrawCommands` (walks the `Opaque` range, letting `FDrawSubmitStateCache` filter out redundant binds).
6. **Hot reload.** Editing `Primitive.hlsl` → `TickHotReload` notices the dependency hash changed → `FGraphicsProgram::Create` runs again → input layout, parameter layout, and disk cache all update. The next frame's `BuildMeshDrawCommand` picks up the new program automatically because `GetShader` returns the same `FGraphicsProgram*` slot.

Each of those bullets corresponds to one of the three build systems from §1. The clean separation is the reason changes to one rarely cascade into the others — adding a shader doesn't require pipeline-tree edits, adding a pass doesn't require shader-registry edits, and adding a structured-buffer SRV to `FFrameResources` doesn't require either.

---

## 6. Adding Things — Cookbook

| Goal | Touch points |
|---|---|
| Add a built-in shader | `EShaderType` enum + name lookup in `ShaderManager.cpp`, one `Add(...)` line in `FShaderProgramRegistry::Initialize`. |
| Add a shader variant (define-driven) | `MakeGraphicsProgramDesc` + `AddDefine` in the registry, or use `FShaderVariantCache::GetOrCreate` from a pass. |
| Add a new pass | (a) declare in `ERenderPassNodeType`, (b) `new` it in `FRenderPassRegistry::Initialize`, (c) reference it from the relevant `AddPipeline(...)` in `FRenderPipelineRegistry::Initialize`, (d) implement `PrepareInputs / PrepareTargets / SubmitDrawCommands` (+ `BuildDrawCommands` if it emits its own commands). |
| Add a render path or view-mode subtree | Add an `ERenderPipelineType`, register a new `AddPipeline` block, extend `ShouldExecutePipeline / ShouldExecutePass` so it gates correctly. |
| Add a system SRV slot (e.g. new structured buffer) | Reserve a `t#` constant in `RenderBindingSlots.h::ESystemTexSlot`, add the buffer/SRV pair to `FFrameResources` with a grow-on-demand `Update*` method, bind from the consuming pass. |
| Add a constant buffer slot | Reserve `b#` in `ECBSlot`. For a per-shader CB use `FConstantBufferCache::GetOrCreate(Slot, ByteWidth)`; for per-object stay inside the existing pool; for one-off small payloads use `FCBBindingEntry`. |
| Add a sampler | Add a field to `FFrameResources`, create it in `FFrameResources::Create`, append to the `BindSystemSamplers` array, reserve a slot in `ESamplerSlot`. |
| Add a render state preset | Extend the relevant state manager's bake list and the `EBlendState / EDepthStencilState / ERasterizerState` enum, then update `InitializeDefaultRenderPassPresets` if a logical pass should default to it. |

---

## 7. Reading Order For New Contributors

If you're new to this code and want the shortest path to "I get how the renderer works":

1. [render_pipeline_anatomy.md §1–§3](render_pipeline_anatomy.md) — frame flow, scene view contract.
2. [`FRenderPipelineRegistry::Initialize`](../Execute/Registry/RenderPipelineRegistry.cpp) — see the actual tree.
3. [`FRenderPipelineRunner::ExecutePipelineRecursive`](../Execute/Runner/RenderPipelineRunner.cpp) — see how it's walked (≈100 lines including the gating helpers).
4. [`FRenderPass`](../Execute/Passes/Base/RenderPass.h) + one concrete pass like [`FDepthPrePass`](../Execute/Passes/Scene/DepthPrePass.h) — see the lifecycle.
5. [`FShaderProgramRegistry::Initialize`](../Execute/Registry/ShaderProgramRegistry.cpp) — see the shader table.
6. [`FGraphicsProgram::Create`](../RHI/D3D11/Shaders/GraphicsProgram.cpp) — see compile + reflect.
7. [`FFrameResources`](../Resources/FrameResources.h) — see the per-frame resource pool.
8. [`FDrawCommand`](../Submission/Command/DrawCommand.h) — see what one GPU draw actually carries.

Everything else is variations on these eight pieces.
