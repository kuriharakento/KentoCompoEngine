# KentoCompoEngine

**本格的なコンポーネント指向を追求した自作ゲームエンジン**

## ビルドステータス
[![DebugBuild](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/DebugBuild.yml)  
[![ReleaseBuild](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/ReleaseBuild.yml)

> 継続的インテグレーションにより、常にエンジンが動作する状態であることが保証されます。

---

## 概要

**KentoCompoEngine** は、C++で開発された本格的なコンポーネントベースゲームエンジンです。  
DirectX 12をベースに、Entity-Component-System（ECS）パターンを採用し、高い拡張性と再利用性を実現しています。

---

## このエンジンの強み 〜コンポーネント指向設計を徹底追求〜

### 🎯 本格的なコンポーネントベース設計

オブジェクトの振る舞いを「コンポーネント」として完全に分離し、自由に組み合わせられる設計を徹底しています。

**コンポーネント階層構造：**
```
IGameObjectComponent（全コンポーネントの基底）
├── IActionComponent（動的な振る舞い）
│   ├── MoveComponent（移動・回避システム）
│   ├── BulletComponent（弾丸の挙動）
│   ├── AssaultRifleComponent（武器制御）
│   └── その他アクションコンポーネント
│
└── ICollisionComponent（衝突判定）
    ├── サブステップ判定対応
    └── 高速オブジェクトのすり抜け防止
```

### ⚡ 柔軟かつ拡張性の高い開発フロー

- **簡単な機能追加**：新しいコンポーネントを作成するだけで、既存のゲームオブジェクトに機能を追加可能
- **動的な組み合わせ**：実行時にコンポーネントの追加・削除が可能
- **疎結合な設計**：各コンポーネントは独立しており、他のコンポーネントに依存しない

**使用例：**
```cpp
// プレイヤーオブジェクトに移動と射撃機能を追加
player->AddComponent("move", std::make_unique<MoveComponent>(enemyManager, camera));
player->AddComponent("shoot", std::make_unique<AssaultRifleComponent>());

// パーティクルエフェクトをJSONから読み込んで再生
ParticleManager::GetInstance()->LoadEffectDefinition("bulletTrail");
ParticleManager::GetInstance()->Play("bulletTrail", bullet->GetTransform());
```

### 🔧 高い再利用性と保守性

**MoveComponent の豊富な機能例：**
- 回避アクション（ローリング、ダッシュ）
- 無敵時間の管理
- 回避エフェクトの自動生成
- バレットタイム対応
- 滑らかな回転補間

これらの機能は設定パラメータで調整可能で、プレイヤー・敵・NPCなど、あらゆるキャラクターに転用できます。

**ICollisionComponent の高度な機能：**
- サブステップ判定による高速オブジェクトのすり抜け防止
- 自動的な衝突管理（CollisionManagerへの自動登録・解除）
- 判定サイズのオフセット調整

---

## 主な機能

### 🎮 ゲームオブジェクトシステム
- **GameObject**: 階層構造に対応したゲームオブジェクト基底クラス
- **コンポーネントの動的追加・削除**: 実行時に機能を変更可能
- **親子関係のサポート**: ワールド行列の自動伝播

### 🌟 Niagara風パーティクルシステム

Unreal EngineのNiagaraにインスパイアされた、モジュラー設計の高機能パーティクルシステム。

**Spawnモジュール（生成時設定）：**
```
├── InitialModules（初期状態設定）
│   ├── InitializePositionModule（位置）
│   ├── InitializeVelocityModule（速度）
│   ├── InitializeSizeModule（サイズ）
│   ├── InitializeColorModule（カラー）
│   └── InitializeLifetimeModule（寿命）
│
├── SpawnModules（生成制御）
│   ├── SpawnRateModule（毎秒生成レート）
│   ├── SpawnBurstModule（バースト生成）
│   └── SubEmitterModule（サブエミッター）
│
└── SpawnShapeModules（生成形状）
    ├── SphereLocationModule（球体）
    ├── BoxLocationModule（ボックス）
    ├── CircleLocationModule（円形）
    └── ConeLocationModule（コーン）
```

**Updateモジュール（更新時処理）：**
```
├── BehaviorModules（振る舞い）
│   ├── GravityModule（重力）
│   ├── DragModule（空気抵抗）
│   ├── TurbulenceModule（乱流ノイズ）
│   ├── ColorOverLifetimeModule（寿命による色変化）
│   ├── SizeOverLifetimeModule（寿命によるサイズ変化）
│   └── RotationOverLifetimeModule（寿命による回転）
│
├── ForceFieldModules（フォースフィールド）
│   ├── PointAttractorModule（点吸引）
│   ├── VortexModule（渦巻き）
│   └── CurlNoiseModule（カールノイズ）
│
├── MotionEffectModules（モーション特性）
│   ├── OrbitModule（軌道運動）
│   ├── SpiralMotionModule（螺旋運動）
│   └── WaveMotionModule（波動運動）
│
└── RibbonModules（トレイル描画）
    └── AssignRibbonIdModule（マルチトレイルID割り当て）
```

**マルチレンダラーシステム：**
- **SpriteRenderer**: ビルボードスプライト描画
- **MeshRenderer**: 3Dメッシュパーティクル
- **TrailRenderer**: Niagara風マルチリボントレイル（ribbonId対応）

**ツールチェーン：**
- **ParticleEditor**: ImGuiベースのビジュアルエフェクトエディタ
- **ParticleEffectSerializer**: JSON形式でのエフェクト保存・読み込み
- **ParticleManager**: エフェクト定義管理と再生API

### ✨ 豊富なゲームエフェクト

実装済みのリッチな演出エフェクト：
- **BulletTrailManager**: 弾丸トレイル（JSONベース自動管理）
- **AssaultRifleHitEffect**: 着弾エフェクト
- **EnemyDeathEffect / PlayerDeathEffect**: 死亡演出
- **DodgeEffectParticle**: 回避アクションエフェクト
- **CarnageModeEffect**: カーネージモード演出
- **TitleFireEffect**: タイトル画面の炎エフェクト
- **SceneTransitionEffect**: シーン遷移演出
- **CinematicLetterbox**: シネマティックレターボックス
- **AreaEffect**: 範囲エフェクト

### 🎬 シーン管理
- **SceneManager**: シーンの切り替えと管理
- **SceneFactory**: Factory パターンによるシーン生成
- **シーンコンテキスト**: シーン間でのデータ共有

### ⏱️ 時間管理
- **TimeManager**: ゲーム時間とリアルタイムの分離管理
- **Timer**: コールバック機能付きタイマー
- **タイムスケール**: スローモーションやポーズの実装が容易

### 🎨 レンダリング
- **DirectX 12**: 最新のグラフィックスAPI
- **ポストプロセス**: ブルーム、ブラー、ビネットなどのエフェクト
- **Skybox**: 環境マッピング対応
- **RenderTexture**: オフスクリーンレンダリング

### 🎯 入力システム
- **アクションマッピング**: デバイスに依存しない入力定義
- **キーボード/ゲームパッド**: 複数デバイス対応
- **ボタンリマッピング**: カスタマイズ可能な入力設定

### 🎥 カメラワーク
- **Camerawork System**: 多彩なカメラ演出
- **シネマティックカメラ**: 演出用カメラワーク
- **フォロー/オービット**: ゲームプレイ用カメラ

### 🔊 オーディオ
- **Audio System**: ゲーム用オーディオ管理
- **SE/BGM**: サウンドエフェクト・BGM対応

### 💡 ライティング
- **LightManager**: 複数光源の管理
- **動的ライティング**: リアルタイムライト更新

---

## セットアップとインストール

### 必要な環境
- **Visual Studio 2019** 以降
- **Windows 10** 以降
- **DirectX 12** 対応GPU

### ビルド手順

1. **リポジトリのクローン**
   ```bash
   git clone https://github.com/kuriharakento/KentoCompoEngine.git
   cd KentoCompoEngine
   ```

2. **Visual Studio でプロジェクトを開く**
   ```
   project/KentoCompo.sln
   ```

3. **ビルド設定を選択**
   - `Debug`: 開発・デバッグ用（ImGui・エディタ有効）
   - `Release`: リリース用（最適化有効）

4. **ビルドして実行**
   - `F5` キーでビルド＆実行

---

## プロジェクト構成

```
KentoCompoEngine/
├── project/
│   ├── engine/                  # エンジンコア
│   │   ├── base/                # DirectX基盤、ウィンドウ管理
│   │   ├── effects/             # エフェクトシステム
│   │   │   ├── particle/        # パーティクルシステム
│   │   │   │   ├── module/      # Niagara風モジュール
│   │   │   │   │   ├── spawn/   # 生成モジュール
│   │   │   │   │   └── update/  # 更新モジュール
│   │   │   │   ├── renderer/    # マルチレンダラー
│   │   │   │   ├── editor/      # ビジュアルエディタ
│   │   │   │   └── serialization/ # JSON永続化
│   │   │   └── postprocess/     # ポストプロセス
│   │   ├── graphics/            # 描画システム（2D/3D）
│   │   ├── camerawork/          # カメラワークシステム
│   │   ├── audio/               # オーディオシステム
│   │   ├── light/               # ライティング
│   │   ├── input/               # 入力管理
│   │   ├── scene/               # シーン管理
│   │   ├── time/                # 時間管理
│   │   └── math/                # 数学ライブラリ
│   │
│   ├── application/             # ゲームロジック
│   │   ├── GameObject/          # ゲームオブジェクト
│   │   │   └── component/       # コンポーネント実装
│   │   │       ├── action/      # アクションコンポーネント
│   │   │       ├── collision/   # 衝突判定コンポーネント
│   │   │       └── base/        # コンポーネント基底
│   │   ├── effect/              # ゲームエフェクト
│   │   ├── scene/               # ゲームシーン
│   │   └── stage/               # ステージシステム
│   │
│   └── Resources/               # リソースファイル
│       └── effects/             # エフェクトJSON定義
│
└── README.md
```

---

## 技術スタック

| カテゴリ | 技術 |
|---------|------|
| **言語** | C++17, HLSL |
| **グラフィックス** | DirectX 12 |
| **入力** | DirectInput, XInput |
| **UI** | ImGui（デバッグ・エディタ） |
| **シリアライズ** | nlohmann/json |
| **ビルド** | Visual Studio Solution |
| **CI/CD** | GitHub Actions |

---

## 今後の展望

- [ ] GPU Particle System対応
- [ ] LOD（Level of Detail）システム
- [ ] マテリアルシステムの拡張
- [ ] スクリプティング対応

---

## ライセンス

このプロジェクトはライセンス未設定です。使用の際は作者にご確認ください。

---

## 作者

**kuriharakento**  
[@kuriharakento](https://github.com/kuriharakento)
