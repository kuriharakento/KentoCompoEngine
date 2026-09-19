# Pure GPU Particle Implementation Status

## Implemented vertical slice

GPU mode now selects a Pure GPU path when all modules in an emitter are supported and the renderer is Sprite, Mesh, or Ribbon. Sprite uses indirect draw directly; Mesh copies the GPU-produced alive instance count into indexed indirect arguments without CPU readback. Ribbon compacts alive particles, sorts them by `(ribbonId, persistent particleId)`, generates grouped strip segments, and writes indirect arguments entirely on the GPU.

- GPU-resident fixed particle pool
- atomic dead-slot allocation using a raw UAV counter
- SpawnRate accumulation without CPU particle construction
- deterministic GPU initialization for velocity, lifetime, scale and color ranges
- GPU lifetime/death/reuse
- no per-frame particle snapshot readback
- no per-frame particle payload upload
- finite-emitter completion tracking without particle snapshot readback
- asynchronous 4-byte Completion Record readback from GPU alive instance count
- GPU-resident Spawn/Death event stream with bounded atomic append
- same-effect child emitters consume GPU events directly by source emitter index
- OnSpawn/OnDeath filtering, probability, position, velocity and color inheritance
- GPU Ribbon vertex generation and indirect triangle-strip draw
- explicit Hybrid fallback for unknown or unsupported modules

The one-time zero upload used to initialize the pool is not a simulation snapshot upload.

## Supported Pure GPU modules

- SpawnRate
- SpawnBurst with GPU-resident delay, interval, loop and serial state
- SpawnShape: Point, Sphere, Circle, Box, Cone and Line (volume/surface/edge where applicable)
- InitialVelocity
- InitialLifetime
- InitialScale
- InitialColor
- AssignRibbonId
- Gravity
- Drag
- ColorFade
- ScaleOverLifetime
- Noise
- RotationOverLifetime
- AlphaFade
- VelocityOverLifetime
- StretchByVelocity
- Flicker
- FaceVelocity

The data-driven module runtime provides a shared descriptor registry for all 44 built-in modules, typed emitter parameters, validated compiled pass lists, and a packed GPU module program. All Pure-GPU update operations except the dedicated Gravity input execute through ordered packed opcodes. ColorGradient and ScaleOverLifetime curves use a GPU LUT.

## Remaining work

- move outer Effect duration/startDelay policy into the GPU control pass where gameplay compatibility permits
- optional GPU interpolation/tessellation between sparse ribbon samples
- analytic/depth collision events and custom event writers

Sprite and Mesh compact alive particles and draw through GPU-generated indirect arguments. Ribbon sorts alive entries by `(ribbonId, persistent particleId)`, emits independent six-vertex strip segments, and inserts degenerate boundary blocks between groups. `AssignRibbonId` is Pure GPU capable; `MultiSourceRibbon` remains Hybrid because its external CPU source transforms do not yet have a GPU scene-data provider.
