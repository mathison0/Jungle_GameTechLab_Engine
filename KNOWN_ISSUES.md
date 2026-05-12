# Known Issues

## Deferred / Forward Decal Path

Status: Known broken

Current decal behavior is not stable enough to keep iterating inside the shader refactor phase.

Observed issues:
- Deferred rendering path can produce incorrect decal lighting or invalid normal/surface results.
- Forward rendering path may fail to render decals at all.
- Recent attempts to patch the deferred decal path caused broader regressions such as full-screen gray coverage while decals were visible.

Reason for deferral:
- The current decal implementation mixes fullscreen projection, reconstructed world position, legacy surface mutation, and hybrid forward/deferred binding rules.
- This makes the decal path expensive to stabilize during the current 1.5 shader-structure refactor.
- The project may replace the current decal path with a screen-space decal implementation later, which would make further investment in the current path lower value.

Decision:
- Keep decal issues out of scope for the current shader refactor phase.
- Preserve the shader readability / shared-helper refactor work.
- Treat decal support as a separate follow-up task or redesign task.

Recommended next step:
- Revisit decals in a dedicated task.
- Prefer evaluating a screen-space decal implementation over continuing to patch the current volume/fullscreen hybrid path.
