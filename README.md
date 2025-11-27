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

**パーティクルシステムもコンポーネント設計：**
```
IParticleComponent（パーティクルの基底）
├── IParticleBehaviorComponent（個別パーティクルの振る舞い）
│   ├── AccelerationComponent（加速度）
│   ├── DragComponent（空気抵抗）
│   ├── RotationComponent（回転）
│   └── BounceComponent（地面反発）
│
└── IParticleGroupComponent（パーティクルグループ全体の制御）
    ├── UVTranslateComponent（UVアニメーション）
    └── UVScaleComponent（テクスチャスケール変更）
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

// パーティクルエミッターに加速度と回転を追加
emitter->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0, -9.8f, 0)));
emitter->AddComponent(std::make_shared<RotationComponent>(Vector3(0, 1. 0f, 0)));
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

### 🌟 パーティクルシステム
- **ParticleEmitter**: コンポーネント方式のパーティクルエミッター
- **個別制御**: 各パーティクルに独立した振る舞いを設定
- **グループ制御**: パーティクル群全体のアニメーション制御

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
- **ポストプロセス**: ブルーム、ブラーなどのエフェクト
- **Skybox**: 環境マッピング対応
- **RenderTexture**: オフスクリーンレンダリング

### 🎯 入力システム
- **アクションマッピング**: デバイスに依存しない入力定義
- **キーボード/ゲームパッド**: 複数デバイス対応
- **ボタンリマッピング**: カスタマイズ可能な入力設定

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
   - `Debug`: 開発・デバッグ用（ImGui有効）
   - `Release`: リリース用（最適化有効）

4. **ビルドして実行**
   - `F5` キーでビルド＆実行

---

## プロジェクト構成

```
KentoCompoEngine/
├── project/
│   ├── engine/              # エンジンコア
│   │   ├── base/            # DirectX基盤、ウィンドウ管理
│   │   ├── effects/         # パーティクルシステム
│   │   ├── framework/       # フレームワーク基底
│   │   ├── graphics/        # 描画システム（2D/3D）
│   │   ├── input/           # 入力管理
│   │   ├── scene/           # シーン管理
│   │   └── time/            # 時間管理
│   │
│   └── application/         # ゲームロジック
│       └── GameObject/      # ゲームオブジェクト＆コンポーネント
│           ├── component/   # 各種コンポーネント実装
│           │   ├── base/    # コンポーネント基底
│           │   ├── action/  # アクションコンポーネント
│           │   └── collision/ # 衝突判定コンポーネント
│           └── base/        # GameObject基底クラス
│
└── README.md
```

---

## 技術スタック

- **言語**: C++ (97. 4%), HLSL (1.7%)
- **グラフィックスAPI**: DirectX 12
- **入力**: DirectInput, XInput
- **ビルドシステム**: Visual Studio Solution
- **CI/CD**: GitHub Actions

---

## コンポーネント実装例

より詳しい実装は以下から確認できます：

- [IGameObjectComponent](https://github.com/kuriharakento/KentoCompoEngine/blob/master/project/application/GameObject/component/base/IGameObjectComponent. h) - コンポーネント基底
- [IActionComponent](https://github.com/kuriharakento/KentoCompoEngine/blob/master/project/application/GameObject/component/base/IActionComponent.h) - アクション基底
- [MoveComponent](https://github.com/kuriharakento/KentoCompoEngine/blob/master/project/application/GameObject/component/action/MoveComponent.h) - 移動システム
- [IParticleComponent](https://github. com/kuriharakento/KentoCompoEngine/blob/master/project/engine/effects/particle/component/interface/IParticleComponent.h) - パーティクル基底

[🔍 さらに多くのコンポーネントを見る](https://github.com/kuriharakento/KentoCompoEngine/search?q=Component)

---

## ライセンス

このプロジェクトはライセンス未設定です。使用の際は作者にご確認ください。

---

## 作者

**kuriharakento**  
[@kuriharakento](https://github.com/kuriharakento)
