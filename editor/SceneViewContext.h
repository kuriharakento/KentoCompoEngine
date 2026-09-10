#pragma once
#include <memory>

namespace KCE
{
class Camera;

/**
 * @brief シーンビューの矩形（スクリーン座標、ピクセル）
 */
struct SceneViewRect
{
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;

	/**
	 * @brief 描画に使える大きさを持つか
	 * @return 幅と高さが正なら真
	 */
	bool IsValid() const { return width > 0.0f && height > 0.0f; }
};

/**
 * @brief シーンを表示しているImGuiウィンドウの情報を共有するクラス
 *
 * @details ギズモはシーン画像の上に重ねて描く必要があるが、
 *          シーン画像を描いているのはアプリ側のエディタUIで、
 *          ギズモを描きたいのはエンジン側のシーケンサである。
 *          その橋渡しとして、画像の矩形と現在のカメラをここで共有する。
 *
 *          アプリはシーン画像を描いた直後に SetViewportRect() を呼ぶこと。
 */
class SceneViewContext
{
public:
	static SceneViewContext* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief シーン画像の矩形を設定する
	 * @param rect スクリーン座標での矩形
	 */
	void SetViewportRect(const SceneViewRect& rect) { rect_ = rect; }

	const SceneViewRect& GetViewportRect() const { return rect_; }

	/**
	 * @brief シーンを描いているカメラを設定する
	 * @param camera カメラ。所有権は持たない
	 */
	void SetCamera(Camera* camera) { camera_ = camera; }

	Camera* GetCamera() const { return camera_; }

	/**
	 * @brief ギズモを描ける状態か
	 * @return 矩形とカメラが揃っていれば真
	 */
	bool IsReady() const { return rect_.IsValid() && camera_ != nullptr; }

public:
	~SceneViewContext() = default;

private:
	static std::unique_ptr<SceneViewContext> instance_;
	friend std::unique_ptr<SceneViewContext> std::make_unique<SceneViewContext>();

	SceneViewContext() = default;
	SceneViewContext(const SceneViewContext&) = delete;
	SceneViewContext& operator=(const SceneViewContext&) = delete;

	// シーン画像の矩形
	SceneViewRect rect_;
	// シーンを描いているカメラ
	Camera* camera_ = nullptr;
};
} // namespace KCE
