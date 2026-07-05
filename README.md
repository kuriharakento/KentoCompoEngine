<table align="center" border="0" cellpadding="10" cellspacing="0">
  <tr>
    <td align="left" valign="middle">
      <h1>KentoCompoEngine</h1>
      <p>コンポーネント指向（ECS）を採用した、DirectX 12ベースの自社製3Dゲームエンジンです。</p>
    </td>
    <td align="center" valign="middle" width="150">
      <img src="https://github.com/kuriharakento.png" width="120" height="120" style="border-radius: 50%; border: 3px solid #0078d4;" alt="kuriharakento" />
    </td>
  </tr>
</table>

## ビルド・デプロイステータス

[![DebugBuild](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/DebugBuild.yml)  
[![ReleaseBuild](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/ReleaseBuild.yml)  
[![Deploy Documentation](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/docs.yml/badge.svg)](https://github.com/kuriharakento/KentoCompoEngine/actions/workflows/docs.yml)

## API ドキュメント

エンジンの詳細なクラス図や各関数の使い方（APIリファレンス）は、以下の GitHub Pages にて自動公開されています。
* **[KentoCompoEngine API Reference](https://kuriharakento.github.io/KentoCompoEngine/)**

---

## 導入方法 (Submodule)

他のプロジェクトに本エンジンをサブモジュールとして追加する際は、プロジェクトのルートディレクトリで以下のコマンドを実行してください。

```bash
# サブモジュールの追加
git submodule add https://github.com/kuriharakento/KentoCompoEngine.git engine

# サブモジュールの初期化と更新
git submodule update --init --recursive
```

---

## 技術スタック

| カテゴリ | 技術 |
|---------|------|
| **言語** | C++, HLSL |
| **グラフィックス** | DirectX 12 |
| **入力** | DirectInput, XInput |
| **UI** | ImGui（デバッグ・エディタ） |
| **シリアライズ** | nlohmann/json |
| **ビルド** | Visual Studio Solution |
| **CI/CD** | GitHub Actions |

---

## ディレクトリ構造

* `base/` - DirectX12の初期化や描画デバイス、ウィンドウ管理などの基盤システム
* `ecs/` - Entity Component System (ECS) の管理コア
* `graphics/` - パイプライン、テクスチャ、各種描画レンダラー
* `gameobject/` - アクターやゲームオブジェクトの基底設計
* `camerawork/` - カメラ制御機能
* `light/` - ライト関連のオブジェクト・パラメータ制御
* `input/` - キーボード、マウス、ゲームパッドの入力検知
* `audio/` - 音声の再生・管理システム
* `math/` - 行列、ベクトル、クォータニオンなどの3D数学演算
* `externals/` - 外部サードパーティライブラリ

---

## 開発ガイドライン

本エンジンを拡張・開発する際は、以下の設計思想および規約を遵守してください。

### 1. メモリ管理と所有権 (Ownership)
* **Unique Ownership**: リソースの所有権は原則として `std::unique_ptr` で管理し、`std::make_unique` を優先して使用してください。
* **Shared Ownership**: `std::shared_ptr` の使用は原則禁止です（導入時は循環参照対策や代替不可な理由を明記してください）。
* **生ポインタ**: 生ポインタ（`T*`）は「非所有参照（寿命を管理しない）」用途に限定し、メンバ変数として保持する場合は所有者と寿命をコメントしてください。

### 2. コーディングスタイル
* **ブレース**: Allman Style（中括弧を手前で改行する形式）を採用します。
  ```cpp
  if (condition)
  {
      // 処理
  }
  ```
* **コメント規約**: クラス/関数の説明は Doxygen 形式（`/** ... */`）を使用し、`@brief` や `@param`、`@return` を記述してください。
  ```cpp
  /**
   * @brief 関数の説明
   * @param param 引数の説明
   * @return 戻り値の説明
   */
  ```

---

## 作者

**kuriharakento**  
[@kuriharakento](https://github.com/kuriharakento)
