# Particle Module Runtime

## Asset and registry contract

- Effect JSON format version: `2`; missing version is migrated as version `1`.
- Every module resolves through `ModuleDescriptorRegistry` by stable ID.
- Descriptors own stage, version, CPU/Pure-GPU capability, kernel ID, attribute declarations, parameter schema, and factory.
- Unknown IDs, newer module versions, invalid enum values, non-finite numbers, oversized arrays, and excessive capacities fail loading instead of being ignored.
- `CompiledEmitter` validates stage compatibility, produces an ordered pass list, a packed metadata block, and a deterministic layout hash.
- The registry contains 44 factories and every descriptor has a non-empty typed parameter schema. Registry construction rejects empty/duplicate IDs and default-value/type mismatches.
- `KCE_PARTICLE_SERIALIZATION_SELF_TEST=1` creates all 44 registered modules, saves JSON v2, reloads it, and checks module order/type plus representative nested and newly-added fields. Debug and Release both report `PASS` with `registry_modules=44`.

## Typed parameters

Emitter parameters support float, unsigned/signed integer, bool, Vector3, Vector4, and string values. Schema metadata additionally distinguishes enum, curve, gradient, and struct-array editor shapes. Values are serialized with explicit type tags. DynamicFloat, DynamicVector3, and DynamicColor can bind to this namespace with a typed fallback.

## GPU program ABI

The module program is a 16384-byte raw SRV bound at `t2` and supports up to 255 operations.

- byte 0: operation count (`uint`)
- bytes 4-15: reserved
- byte 16 onward: 64-byte operation records
- record byte 0: opcode (`uint`)
- remaining record bytes: opcode-specific packed values

The compute shader evaluates pre-integration and post-integration operations in module-program order. Current opcodes are Drag, VelocityOverLifetime, Noise, ColorFade, ScaleOverLifetime, StretchByVelocity, FaceVelocity, RotationOverLifetime, AlphaFade, and Flicker. Gravity and spawn initialization remain dedicated control/spawn inputs because they belong to different execution stages.

ColorGradient and AnimationCurve keys are baked each frame into a bounded structured LUT (`t3`, 1024 float4 entries, 32 samples per active curve). Opcode records carry the LUT offset/count and the shader linearly samples adjacent entries. CPU and Hybrid paths evaluate the original keys directly.

The compatibility constant fields remain in the ABI for old assets and Hybrid execution, but migrated operations clear their legacy enable flag before dispatch to prevent double execution.
