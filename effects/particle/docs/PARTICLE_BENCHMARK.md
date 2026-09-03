# Particle System Performance Benchmark Guide

この文書は、KCE Particle System の性能測定ベンチマーク（B01〜B08）の実行方法、仕様、結果フォーマット、および測定バイアスについて説明するガイドである。

---

## 1. 起動およびテスト方法

### 起動手順
1. アプリケーションをビルド（既定構成は `Debug|x64` または `Release|x64`）し、実行する。
2. アプリケーション起動後、シーンを `ParticleTestScene` に切り替える。
3. 画面上に表示される「**Particle Benchmark**」の ImGui UI を使用して操作する。

### 設定と実行
1. **Preset の選択**: 測定したいプリセット（B01〜B08）をドロップダウンメニューから選択する。
2. **Simulation Mode の選択**: 
   - `CPU Sim`: すべてのシミュレーション計算を CPU 側で処理する。
   - `GPU Sim (Hybrid)`: 更新処理を Compute Shader で行い、毎フレーム CPU への readback と upload を行うハイブリッド動作。
3. **ベンチマークの開始**: 「**Start Benchmark**」ボタンを押す。
   - 開始時にカメラは `{0, 10, -30}` に固定され、ベンチマーク中のマウス/キーボード入力によるカメラの移動は完全にロック（強制リセット）され再現性を担保する。
   - 乱数シードは `srand(12345)` によって初期化され、可能な限り再現性のあるエミッションを行う。

---

## 2. ベンチマーク・ライフサイクル（Warm-up & Sampling）

ベンチマークは開始から **10.0 秒間** 自動実行され、終了時に測定データが CSV ファイル（`./application/Resources/test-results/particles/particle_benchmark_baseline.csv`）へ自動エクスポートされた後、自動的に終了する。

- **0.0s 〜 3.0s: Warm-up 期間**
  - 粒子生成と消滅が定常状態に達するまでの移行期間。
  - この期間中、性能プロファイラの測定データは毎フレームリセットされ、サンプリング結果から除外される。
- **3.0s 〜 10.0s: Sampling 期間 (7.0秒間)**
  - 粒子数が一定値に達した安定状態（理論限界付近）での性能データ。
  - プロファイル時間およびデータ転送カウンタの集計が行われる。
- **10.0s: Auto Export & Stop**
  - サンプリング終了時に自動的に CSV 保存が走り、ベンチマークが停止する。

---

## 3. ベンチマークプリセット仕様（B01〜B08）

| ID | タイプ | 目標最大粒子数 | Emitter 数 | Emitter 個別容量 | 適用モジュール |
|---|---|---|---|---|---|
| **B01** | Sprite | 1,000 | 1 | 1,000 | SpawnRate, InitialLifetime, InitialVelocity, InitialColor |
| **B02** | Sprite | 10,000 | 1 | 10,000 | G01構成 ＋ Gravity, Drag |
| **B03** | Sprite | 65,536 | 1 | 65,536 | G01構成 ＋ Gravity, Drag (デフォルト最大粒子数) |
| **B04** | Sprite | 65,536 | 10 | 8,000 | G01構成 ＋ Gravity, Drag (マルチエミッター・高密度) |
| **B05** | Sprite | 10,000 | 100 | 200 | G01構成 (マルチエミッター・超高並列) |
| **B06** | Mesh | 10,000 | 1 | 10,000 | G01構成 ＋ Gravity, Drag, MeshRenderer (Cube形状) |
| **B07** | Ribbon | 1,000 | 1 | 1,000 | SpawnRate, InitialLifetime, AssignRibbonId(4), TrailRenderer |
| **B08** | Sprite event chain | 1,000 each | 2 | 約1,000 | Source Death event → child GPU spawn, velocity/color inheritance |

> [!NOTE]
> B04における「個別容量」は 1 エミッターあたり 8,000 であり、全10エミッターの総目標粒子数は 65,536 である。

---

## 4. プロファイル項目と時間分類

プロファイルデータとして出力される各測定項目（Scope Name）が、CPU処理、GPU処理、Command Recordingのいずれに属するかを以下に分類する。

| 測定スコープ名 | 計測対象と処理フェーズ | 時間分類 |
|---|---|---|
| `ManagerUpdate` | `ParticleManager::Update` 全体の実行時間 | **CPU 実行時間** |
| `EmitterUpdateCPU` | `ParticleEmitter::UpdateCPU` (全CPU更新モジュール実行) | **CPU 実行時間** |
| `EmitterUpdateGpuPerCall` | `ParticleEmitter::UpdateGPU` 1回 (準備・CS Dispatch発行・Readback/Upload) | **CPU 実行時間** |
| `EmitterUpdateGpuFrameTotal` | 1フレーム内の全 `ParticleEmitter::UpdateGPU` 合計 | **CPU 実行時間** |
| `ReadbackMapCopy` | Readbackバッファの `Map` と `memcpy` によるCPU読み出し時間 | **CPU 実行時間** |
| `RemoveDeadParticles` | CPU側での生存フラグチェックと死亡粒子の除外処理時間 | **CPU 実行時間** |
| `UpdateGPUSpawns` | GPUシミュレーション前にCPUで行う新規スポン処理時間 | **CPU 実行時間** |
| `UploadMapCopy` | `memcpy` によるCPUアップロードとGPU転送コマンド記録時間 | **CPU 実行時間** / **Command Recording** |
| `RendererUpdate` | 各レンダラーの `Update` (インスタンスデータ構築など) | **CPU 実行時間** |
| `RendererDrawRecording` | `Draw` メソッド内のGPU描画コマンドの記録所要時間 | **Command Recording** |

---

## 5. 保存結果フォーマット（CSV Schema）

自動・手動エクスポートされる CSV ファイル（`./application/Resources/test-results/particles/particle_benchmark_baseline.csv`）は、以下のスキーマで保存される。

| 列名 | 単位 | 説明 |
|---|---|---|
| `preset` | 整数 (1~8) | 実行したベンチマークプリセットID |
| `simulation_mode` | 文字列 | `CPU` または `GPU` (シミュレーション動作モード) |
| `build` | 文字列 | `Debug` または `Release` (コンパイル構成) |
| `scope_name` | 文字列 | プロファイルスコープ名 (上記セクション4参照) |
| `last_ms` | ミリ秒 (double) | 直近フレームの測定時間 |
| `average_ms` | ミリ秒 (double) | 指数移動平均 (EMA, alpha=0.05) による平均時間 |
| `min_ms` | ミリ秒 (double) | サンプリング期間中の最小時間 |
| `max_ms` | ミリ秒 (double) | サンプリング期間中の最大時間 |
| `sample_count` | 整数 | サンプリングされたフレーム総数 |
| `cpu_upload_memcpy_bytes` | バイト数 | CPU側でアップロード用に `memcpy` した実データサイズ |
| `gpu_upload_copy_bytes` | バイト数 | GPUの `CopyBufferRegion` コマンドで要求したアップロードサイズ |
| `gpu_readback_copy_bytes` | バイト数 | GPUの `CopyBufferRegion` コマンドで要求したリードバックサイズ |
| `cpu_readback_memcpy_bytes` | バイト数 | CPU側でリードバック取得のために `memcpy` した実データサイズ |
| `emitter_count` | 整数 | 動作しているエミッターの総数 |
| `active_particle_count_type` | 整数 | 測定終了時のアクティブな合計生存粒子数 |

---

## 6. 性能測定における測定バイアスと非決定論的要素

性能データの分析にあたっては、以下のバイアスが存在することを考慮する必要がある。

1. **std::random_device による非決定性 (測定バイアス1)**
   - `srand(12345)` をベンチマーク開始時に呼び出して C標準の乱数は固定しているが、`SubEmitterModule` や `MathUtils::Random` などの一部のモジュールは `std::random_device` (ハードウェア乱数) を使用しているため、毎回の実行でエミッションされる位置や速度の揺らぎが完全に一致しない。これにより、フレームごとの粒子生存数にわずかな非決定性が生じる。
2. **ダブルバッファリング Readback による 1 フレーム遅延 (測定バイアス2)**
   - GPU シミュレーションでのストールを避けるため、`GPUSimulator` は 1 フレーム前のコピーデータを読み戻す。これにより、UIに表示される `Hybrid CPU vector count` および CPU で認識される生存粒子数は GPU 上の最新フレームの状態から常に 1 フレーム遅れて表示される。
3. **GPU 実行時間の未計測 (測定バイアス3)**
   - プロファイラーで計測されるのはすべて **CPU上の実行時間** または **Command Recording（コマンド記録）時間** である。GPU上での Compute Shader やレンダリングの純粋な物理的実行時間は、本計測器ではカバーしておらず（タイムスタンプクエリ等を使用しない限り）、GPUの純粋な並列計算能力はCPUスレッドの解放速度から推測するのみとなる。
