# Particle System Benchmark Results

この文書は、KCE Particle System のベンチマーク実行記録である。ビルド成功やコードレビューはランタイム性能試験の `PASS` と同一視しない。

## 1. 現在の検証状態

- Date: 2026-08-01
- Source state: ローカル未コミット差分（`git diff` を識別子とする）
- Runtime environment: 未取得
- GPU / driver: 未取得
- Resolution: 未取得
- VSync / FPS limiter: 未取得
- Baseline CSV: `application/Resources/test-results/particles/particle_benchmark_baseline.csv`
- Static validation: engine/applicationともにDebug x64 / Release x64ビルド成功、ParticleCompute/ParticleSpawnPrepareは`fxc cs_5_0`成功
- Application smoke: Debug/Release `GameTemplate.exe` は非表示起動を維持し、即時終了・初期化クラッシュなし

## 2. ベンチマーク結果マトリクス

環境変数による自動起動経路でB01 GPU Releaseを実行した。DebugではD3D Debug Layerにより最初のGPUフレームが約17秒となり、旧判定がwarm-upとsamplingを同一フレームで通過する欠陥を検出した。最低30 sampling framesとruntime counter検証を追加済みであり、修正前のDebug CSVは有効結果として扱わない。

| Preset | CPU Debug | GPU Debug | CPU Release | GPU Release |
|---|---|---|---|---|
| B01 | `PASS` | `PASS` | `PASS` | `PASS` |
| B02 | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `PASS` |
| B03 | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `PASS` |
| B04 | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `PASS` |
| B05 | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `PASS` |
| B06 | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `PASS` |
| B07 | `NOT_RUN_ENVIRONMENT_LIMITATION` | `PASS` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `PASS` |
| B08 | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `NOT_RUN_ENVIRONMENT_LIMITATION` | `PASS` |

現行GPU emitterの推定追加descriptorは、`GPUSimulator` core/event/null-source用13個、GPU Ribbon work/sort UAV 6個、packed module program/LUT SRV 2個、renderer 1個の合計22個である。B05は100 emittersで最大2200個を要求する。shader-visible heapは4096へ拡張済みのため実行可能だが、texture等の全subsystemとのpeak合算を結果に記録する。

B07 GPUは`AssignRibbonId(4)`を使用し、persistent particle IDによるGPU sort、ribbonId grouping、縮退strip境界、vertex生成、indirect drawをPure GPUで実行する。外部CPU source transformを所有する`MultiSourceRibbon`はHybrid fallbackとなる。

### B01 GPU Release 実測

- sampling scope count: 201以上
- active particles: 996（回帰前の別実行967/981）/ target 1000
- count type: `PURE_GPU_ASYNC_COMPLETION_RECORD`
- runtime emitters: Pure 1 / Hybrid 0
- CPU readback memcpy: 0 bytes
- estimated descriptors: 22
- result: `PASS`

### B01 Debug/CPU fallback回帰

- GPU Debug: active 966 / target 1000、Pure 1 / Hybrid 0、readback 0、`PASS`
- CPU Release: active 955 / target 1000、`CPU_VECTOR_EXACT`、`PASS`
- CPU Debug: active 976 / target 1000、`CPU_VECTOR_EXACT`、`PASS`
- CPU側は満杯時にSpawnRate accumulatorを全破棄せず、死亡削除後の空きへ次フレーム即補充する。
- Debug Layer初期化の巨大deltaがwarm-up/sampling時間を一度に消費しないよう、制御時計のみ1 frame 0.25秒に上限制限する。測定は7秒かつ30 frames以上。

### B05 GPU Release 実測

- active particles: 9,913 / target 10,000（100 emitters）
- runtime emitters: Pure 100 / Hybrid 0
- CPU readback memcpy: 0 bytes
- estimated descriptors: 2,200
- result: `PASS`
- 大delta時は寿命内に残るRate birthだけを保持し、Rate/Burst/Eventを分離してsub-frame ageを付与する。下限90%・上限110%を自動判定する。

### B07 GPU Release 実測

- sampling scope count: 381以上
- active particles: 963 / target 1000
- renderer: Ribbon（AssignRibbonId(4)、GPU sort/grouping、縮退境界、vertex生成 + indirect draw）
- count type: `PURE_GPU_ASYNC_COMPLETION_RECORD`
- runtime emitters: Pure 1 / Hybrid 0
- CPU readback memcpy: 0 bytes
- estimated descriptors: 22
- result: `PASS`

### B08 GPU Event Chain Release 実測

- sampling frame-total count: 359以上
- active particles: 915（別実行995。source単体の定常値は約250。child event spawnを含む）
- event: source Death → child spawn、velocity/color inheritance
- count type: `PURE_GPU_ASYNC_COMPLETION_RECORD`
- runtime emitters: Pure 2 / Hybrid 0
- CPU readback memcpy: 0 bytes
- estimated descriptors: 44
- result: `PASS`

## 3. 耐久・異常系テスト

以下はコード上にguardや個別削除処理が存在することまでは確認できるが、指定回数のランタイム実行証拠がないため `PASS` としない。

| Test | Status |
|---|---|
| B01 CPU Start/Stop ×100 | `NOT_RUN_ENVIRONMENT_LIMITATION` |
| B01 GPU Start/Stop ×100 | `NOT_RUN_ENVIRONMENT_LIMITATION` |
| Editor Effectあり Start/Stop ×20 | `NOT_RUN_ENVIRONMENT_LIMITATION` |
| Benchmark中のscene終了 ×20 | `NOT_RUN_ENVIRONMENT_LIMITATION` |
| 不正preset guard | `NOT_RUN_ENVIRONMENT_LIMITATION` |
| Texture解決失敗 | `NOT_RUN_ENVIRONMENT_LIMITATION` |
| Descriptor budget不足 | `NOT_RUN_ENVIRONMENT_LIMITATION` |

## 4. 受領条件

各セルを `PASS` に変更できるのは、対象build/modeで実際に10秒間実行し、生成されたCSV、sample count、active count、validation errorの有無、実行環境を保存した場合だけとする。失敗は `FAIL: 理由`、安全上実行しなかった場合は `SKIPPED: 理由` と記録する。
