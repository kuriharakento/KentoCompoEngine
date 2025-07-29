# KentoCompoEngine

**ようこそ、自作ゲームエンジン「KentoCompoEngine」へ！**

## ビルドステータス
[![DebugBuild](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/DebugBuild.yml)  
[![ReleaseBuild](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/ReleaseBuild.yml)

> 継続的インテグレーションにより、常にエンジンが動作する状態であることが保証されます。  
> これらのバッジは、`Debug` と `Release` のビルド設定の最新の状態を示しています。

## このエンジンの強み 〜“コンポーネント指向設計”を徹底追求〜

- **本格的なコンポーネントベース設計**  
  オブジェクトの振る舞いを「コンポーネント」として分離・再利用できる設計を徹底しています。  
  例えば「移動」「攻撃」「弾」「パーティクル挙動」など、個別の機能をコンポーネント（MoveComponent, BulletComponent, ForceFieldComponent等）として実装。  
  必要な機能を組み合わせるだけで多彩なゲームオブジェクトを自在に構築できます。

- **柔軟かつ拡張性の高い開発フロー**  
  IGameObjectComponentやIParticleBehaviorComponentなどのインタフェースを基盤に、  
  新しい振る舞いもコンポーネント単位で簡単に追加・差し替え可能。  
  これにより、複雑化しがちなゲームロジックもシンプルかつ疎結合に保てます。

- **高い再利用性と保守性**  
  「移動」や「回避」「弾発射」などのパラメータやロジックは各コンポーネント内で完結。  
  例えばMoveComponentは回避アクションやインビジブル状態、残像エフェクトなど豊富な機能を内包し、他のオブジェクトにも容易に転用できます。

## セットアップとインストール

**KentoCompoEngine** を始める手順は以下の通りです：

### 必要な環境
- Visual Studio 2019 以降
- Windows 10 以降

### ビルド手順

1. このリポジトリをローカルにクローンします：
   ```bash
   git clone https://github.com/kuriharakento/KentoCompoEngine.git
   ```

2. Visual Studio で `project/KentoCompo.sln` を開きます。

3. 必要に応じて `Debug` または `Release` モードを選択し、ビルドしてください。

