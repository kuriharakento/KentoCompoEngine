# Particle Module Capability Matrix

この文書は、KCE Particle System に存在するすべてのパーティクルモジュールについて、CPU/GPUでの対応状況、実行ステージ、およびシリアライザやエディタでの対応有無を整理したものである。

## 概要

- **モジュール総数**: 44
- **主分類別件数**:
  - `CPU_EXECUTED_THEN_UPLOAD`: 13件 (Spawnステージモジュール。CPUで生成計算後、GPUへ粒子配列をUpload)
  - `MAIN_CS`: 11件 (`ParticleCompute.hlsl`内の共通シミュレーションCSで実行)
  - `SEPARATE_CS`: 3件 (`IsGPUSupported() == true` で個別CSをDispatch)
  - `IGNORED`: 17件 (GPUモード時、Compute Shaderに実装がないため無視されるモジュール)
  
- **集計整合性**: 13 + 11 + 3 + 17 = 44 件 (全モジュールが主表に重複なく1度だけ存在し、総数と一致する)

---

## 1. モジュール対応マトリクス

| C++ クラス名 | GetName() | 実行フェーズ | 優先度 | Serializer Load | Serializer Save | Editor Add | Editor Properties | Hybrid GPU mode behavior | 判定根拠・symbol |
|---|---|---|---|---|---|---|---|---|---|
| InitialPositionModule | `"InitialPosition"` | Spawn | 10 | × | × | × | × | `CPU_EXECUTED_THEN_UPLOAD` (未使用) | `InitialModules.h:InitialPositionModule` |
| InitialVelocityModule | `"InitialVelocity"` | Spawn | 20 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="InitialVelocity") / `SaveEmitter`, `ParticleEditor.cpp:AddModuleDialog` |
| InitialLifetimeModule | `"InitialLifetime"` | Spawn | 5 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="InitialLifetime") / `SaveEmitter`, `ParticleEditor.cpp:AddModuleDialog` |
| InitialColorModule | `"InitialColor"` | Spawn | 30 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="InitialColor") / `SaveEmitter`, `ParticleEditor.cpp:AddModuleDialog` |
| InitialScaleModule | `"InitialScale"` | Spawn | 25 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="InitialScale") / `SaveEmitter`, `ParticleEditor.cpp:AddModuleDialog` |
| AssignRibbonIdModule | `"AssignRibbonId"` | Spawn | 35 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="AssignRibbonId") / `SaveEmitter`, `ParticleEditor.cpp:AddModuleDialog` |
| SpawnRateModule | `"SpawnRate"` | Spawn | 0 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="SpawnRate") / `SaveEmitter`, `ParticleEditor.cpp:AddModuleDialog` |
| SpawnBurstModule | `"SpawnBurst"` | Spawn | 0 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="SpawnBurst") / `SaveEmitter`, `ParticleEditor.cpp:AddModuleDialog` |
| SpawnShapeModule | `"SpawnShape"` | Spawn | 15 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="SpawnShape") / `SaveEmitter`, `ParticleEditor.cpp:AddModuleDialog` |
| InitialRotationModule | `"InitialRotation"` | Spawn | 35 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="InitialRotation") / `SaveEmitter`, `ParticleEditor.cpp:AddModuleDialog` |
| SubEmitterModule | `"SubEmitter"` | Update | 200 | × | × | ◯ | × | `IGNORED` | `ParticleEditor.cpp:AddModuleOption` ("Sub Emitter" / case 18), `SubEmitterModule.h` |
| RotationOverLifetimeModule | `"RotationOverLifetime"` | Update | 50 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="RotationOverLifetime"), `GPUSimulator.cpp:UpdateConstantBuffer` |
| OrbitModule | `"Orbit"` | Update | 70 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="Orbit"), `ParticleEditor.cpp:AddModuleOption` |
| NoiseModule | `"Noise"` | Update | 60 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="Noise"), `GPUSimulator.cpp:UpdateConstantBuffer` |
| VelocityLimitModule | `"VelocityLimit"` | Update | 40 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="VelocityLimit"), `ParticleEditor.cpp:AddModuleOption` |
| AccelerationModule | `"Acceleration"` | Update | 0 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="Acceleration"), `ParticleEditor.cpp:AddModuleOption` |
| CurlNoiseModule | `"CurlNoise"` | Update | 60 | ◯ | ◯ | ◯ | ◯ | `SEPARATE_CS` | `ParticleEffectSerializer.cpp` (type=="CurlNoise"), `CurlNoiseModule.h:IsGPUSupported` |
| SizeBySpeedModule | `"SizeBySpeed"` | Update | 35 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="SizeBySpeed"), `ParticleEditor.cpp:AddModuleOption` |
| ColorBySpeedModule | `"ColorBySpeed"` | Update | 35 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="ColorBySpeed"), `ParticleEditor.cpp:AddModuleOption` |
| CollisionModule | `"Collision"` | Update | 100 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="Collision"), `ParticleEditor.cpp:AddModuleOption` |
| KillZoneModule | `"KillZone"` | Update | 90 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="KillZone"), `ParticleEditor.cpp:AddModuleOption` |
| SprintToTargetModule | `"SprintToTarget"` | Update | 10 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="SprintToTarget"), `ParticleEditor.cpp:AddModuleOption` |
| AttractorModule | `"Attractor"` | Update | 70 | ◯ | ◯ | ◯ | ◯ | `SEPARATE_CS` | `ParticleEffectSerializer.cpp` (type=="Attractor"), `AttractorModule.h:IsGPUSupported` |
| VortexModule | `"Vortex"` | Update | 60 | ◯ | ◯ | ◯ | ◯ | `SEPARATE_CS` | `ParticleEffectSerializer.cpp` (type=="Vortex"), `VortexModule.h:IsGPUSupported` |
| RadialVelocityModule | `"RadialVelocity"` | Spawn | 22 | ◯ | ◯ | ◯ | ◯ | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp` (type=="RadialVelocity"), `ParticleEditor.cpp:AddModuleDialog` |
| VelocityOverLifetimeModule | `"VelocityOverLifetime"` | Update | -45 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="VelocityOverLifetime"), `GPUSimulator.cpp:UpdateConstantBuffer` |
| StretchByVelocityModule | `"StretchByVelocity"` | Update | 44 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="StretchByVelocity"), `GPUSimulator.cpp:UpdateConstantBuffer` |
| WindModule | `"Wind"` | Update | -25 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="Wind"), `ParticleEditor.cpp:AddModuleOption` |
| FlickerModule | `"Flicker"` | Update | 55 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="Flicker"), `GPUSimulator.cpp:UpdateConstantBuffer` |
| AlphaFadeModule | `"AlphaFade"` | Update | 48 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="AlphaFade"), `GPUSimulator.cpp:UpdateConstantBuffer` |
| RotationBySpeedModule | `"RotationBySpeed"` | Update | 62 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="RotationBySpeed"), `ParticleEditor.cpp:AddModuleOption` |
| SineWaveModule | `"SineWave"` | Update | -28 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="SineWave"), `ParticleEditor.cpp:AddModuleOption` |
| SpiralModule | `"Spiral"` | Update | -22 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="Spiral"), `ParticleEditor.cpp:AddModuleOption` |
| TwistModule | `"Twist"` | Update | -18 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="Twist"), `ParticleEditor.cpp:AddModuleOption` |
| FaceVelocityModule | `"FaceVelocity"` | Update | 60 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="FaceVelocity"), `GPUSimulator.cpp:UpdateConstantBuffer` |
| JitterModule | `"Jitter"` | Update | 45 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="Jitter"), `ParticleEditor.cpp:AddModuleOption` |
| ForceOverLifetimeModule | `"ForceOverLifetime"` | Update | 10 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="ForceOverLifetime"), `ParticleEditor.cpp:AddModuleOption` |
| RibbonInterpolationModule | `"RibbonInterpolation"` | Spawn | 100 | ◯ | ◯ | × | × | `CPU_EXECUTED_THEN_UPLOAD` (未使用) | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="RibbonInterpolation") / `SaveEmitter` |
| MultiSourceRibbonModule | `"MultiSourceRibbon"` | Spawn | 0 | ◯ | ◯ | × | × | `CPU_EXECUTED_THEN_UPLOAD` | `ParticleEffectSerializer.cpp:LoadEmitter` (type=="MultiSourceRibbon") / `SaveEmitter` |
| TextureSheetModule | `"TextureSheet"` | Update | 80 | ◯ | ◯ | ◯ | ◯ | `IGNORED` | `ParticleEffectSerializer.cpp` (type=="TextureSheet"), `ParticleEditor.cpp:AddModuleOption` |
| GravityModule | `"Gravity"` | Update | -30 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="Gravity") / `SaveEmitter`, `GPUSimulator.cpp:UpdateConstantBuffer` |
| DragModule | `"Drag"` | Update | -20 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="Drag") / `SaveEmitter`, `GPUSimulator.cpp:UpdateConstantBuffer` |
| ColorFadeModule | `"ColorFade"` | Update | 60 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="ColorFade") / `SaveEmitter`, `GPUSimulator.cpp:UpdateConstantBuffer` |
| ScaleOverLifetimeModule | `"ScaleOverLifetime"` | Update | 40 | ◯ | ◯ | ◯ | ◯ | `MAIN_CS` | `ParticleEffectSerializer.cpp` (type=="ScaleOverLifetime") / `SaveEmitter`, `GPUSimulator.cpp:UpdateConstantBuffer` |

---

## 2. Serializer / Editor / Runtime の不一致詳細

1. **InitialPositionModule**
   - **不一致**: C++ クラス (`InitialModules.h:InitialPositionModule`) は存在するが、シリアライザの読込/保存、エディタの追加/プロパティ UI すべてにコードが存在しない。
   - **影響**: JSONファイルからロードできず、エディタ上でも追加不可能。実質的に利用不可能なデッドコード。
2. **SubEmitterModule**
   - **不一致**: エディタの追加メニューには "Sub Emitter" (`ParticleEditor.cpp:L2434`) があるが、シリアライザの読込/保存 (`ParticleEffectSerializer.cpp`)、およびエディタのプロパティ描画 UI (`DrawModuleProperties`) にコードが存在しない。
   - **影響**: エディタ上で追加しても、JSONファイルに保存すると消失する。また、プロパティをエディタ上で調整することもできない。
3. **RibbonInterpolationModule** / **MultiSourceRibbonModule**
   - **不一致**: シリアライザのロード/セーブには対応しているが、エディタの追加ダイアログおよびプロパティ UI にコードが存在しない。
   - **影響**: エディタ上で新規に作成・追加することはできないが、あらかじめ JSON ファイルを直接編集して定義した場合は正常に読み込み・動作する。

---

## 3. GPUシミュレーション時における非対応モジュールの挙動・セマンティクス差異

### 1) SpawnフェーズモジュールのCPU実行
現行の「GPUシミュレーション（ハイブリッド）」モードでは、毎フレーム GPU バッファを CPU に readback し、CPU側でスポン（新規生成）処理を行い、再度 upload する。そのため、`CPU_EXECUTED_THEN_UPLOAD` に分類されるモジュール群は CPU 上で決定論的に実行され、その結果が GPU 側へ引き継がれる。

### 2) 無視される Updateフェーズモジュール (`IGNORED`)
GPUモードでは C++ 側の Update 処理がバイパスされるため、`IsGPUSupported() == false` であり、かつ `GPUSimulator::UpdateConstantBuffer` で定数バッファパラメータとして転送されず hlsl 側の処理も存在しない Update モジュール（例: `Orbit`, `VelocityLimit`, `Wind` など計17件）は、**GPUモードでは何の効果も発揮しない**。

### 3) 重力（GravityModule）のセマンティクス差異
- **CPUモード**: `GravityModule` の min/max 範囲から決定論的乱数に基づき粒子ごとに異なる重力が適用される。
- **GPUモード**: エミッター内の最初の `GravityModule` の `minGravity` 値が一律で定数バッファ `constantData_->gravity` を介して GPU へ送られ、全粒子に同一の重力が適用される（分散は無視される）。

---

## 4. 監査根拠および検証状況

- **監査根拠**: `ParticleEffectSerializer.cpp`, `ParticleEditor.cpp`, `GPUSimulator.cpp` の静的ソースコード監査。
- **検証状況**: `UNKNOWN_NEEDS_TEST: 0` とする。全44モジュールについて、C++ のクラス階層（IModule 派生クラス）および各モジュールのファイル内の定義と上記の登録箇所を一致確認済み。
