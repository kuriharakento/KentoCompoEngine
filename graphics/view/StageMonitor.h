#pragma once
#include <cstdint>
#include <string>

#include "graphics/view/RenderLayer.h"

namespace KCE
{
class Camera;
class CameraManager;
class GameObject;
class ISubViewProvider;
class RenderView;

/**
 * @brief 別カメラの映像を、ステージ上の画面オブジェクトに映すモニター
 *
 * - カメラは CameraManager に名前付きで作るので、カメラトラックからも動かせる
 * - 画面オブジェクトはモニター用レイヤーに移し、モニター自身の映像には映らないようにする
 * - 映るのは1フレーム前の映像（本編の描画後にサブビューを描くため）
 */
class StageMonitor
{
public:
	/** @brief 画面オブジェクトを置くレイヤー。モニターのカメラからは見えない */
	static constexpr uint32_t kScreenLayerIndex = 1;
	static constexpr RenderLayerMask kScreenLayer = MakeRenderLayerMask(kScreenLayerIndex);
	/** @brief 映像を描き直す回数（1秒あたり）。本物のテレビのように 24fps で十分 */
	static constexpr float kDefaultFramesPerSecond = 24.0f;

	~StageMonitor();

	/**
	 * @brief サブビューとカメラを用意する
	 * @param provider サブビューの作成元。Finalize まで生きている前提
	 * @param cameraManager カメラの登録先
	 * @param cameraName モニター用カメラの名前
	 * @param width 映像の幅（ピクセル）
	 * @param height 映像の高さ（ピクセル）
	 * @return 用意できたら真
	 */
	bool Initialize(ISubViewProvider* provider, CameraManager* cameraManager, const std::string& cameraName, uint32_t width, uint32_t height);

	/**
	 * @brief サブビューを指定解像度で作り直す。
	 * @details Update 中に呼ぶ。前フレームの GPU 完了待ち後なので古いビューを安全に破棄できる。
	 * @param width 幅
	 * @param height 高さ
	 * @return 作り直せたら真
	 */
	bool RecreateView(uint32_t width, uint32_t height);

	/**
	 * @brief 映像を映す画面オブジェクトを決める
	 * @param screen Object3d を持つ GameObject。Finalize まで生きている前提
	 */
	void SetScreen(GameObject* screen);

	/**
	 * @brief 毎フレーム呼ぶ。映像が1回描かれた後で画面にテクスチャを差す
	 */
	void Update();

	/**
	 * @brief 画面のテクスチャを戻して、サブビューを破棄する
	 */
	void Finalize();

	/** @brief モニター用カメラ（CameraManager 所有） */
	Camera* GetCamera() const { return camera_; }

	/**
	 * @brief 映像を1秒に何回描き直すか
	 * @param framesPerSecond 回数。0 以下なら毎フレーム（デバッグ用のカメラ映像など、遅れが困るとき）
	 */
	void SetFramesPerSecond(float framesPerSecond);

private:
	/** @brief 画面の Model にサブビューの映像を差す／外す */
	void BindScreenTexture(bool bind);

	// サブビューの作成元。所有しない（Framework が先に死なない前提）
	ISubViewProvider* provider_ = nullptr;
	// provider_ が所有するビュー
	RenderView* view_ = nullptr;
	// CameraManager が所有するカメラ
	Camera* camera_ = nullptr;
	// CameraManager に自分で追加したカメラだけを Finalize で削除する
	CameraManager* cameraManager_ = nullptr;
	std::string cameraName_;
	bool ownsCamera_ = false;
	// シーンが所有する画面オブジェクト
	GameObject* screen_ = nullptr;
	bool screenBound_ = false;
	// 作ってから数えた Update 回数。描く前の映像（RT 状態のまま）を読まないために使う
	uint32_t updateCount_ = 0;
};
} // namespace KCE
