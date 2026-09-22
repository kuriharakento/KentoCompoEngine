#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/scene/interface/BaseScene.h"
#include "scene/SceneContext.h"

namespace KCE
{
class SceneFactory;
class ShadowMapManager;

class SceneManager
{
public: //メンバ関数
	//デストラクタ
	~SceneManager();
	//コンストラクタ
	SceneManager(SceneFactory* sceneFactory) : currentScene_(nullptr), sceneFactory_(sceneFactory), context_{} {}

	/**
	 * @brief 最初に開くシーンを決める。Initialize より前に呼ぶ。
	 * @param sceneName "Title" でも "TitleScene" でもよい
	 */
	void SetStartSceneName(const std::string& sceneName);

	//初期化
	void Initialize(const SceneContext& context);
	//更新
	void Update();
	//描画
	void Draw3D();
	void DrawTransparent();
	void Draw2D();
	//シャドウ描画
	void DrawShadow();
	//G-Buffer描画（ディファードレンダリング）
	void DrawGBuffer();

	/**
	 * @brief シーンの切り替えを予約する。次の Update の頭で切り替わる。
	 * @param sceneName "Title" でも "TitleScene" でもよい
	 * @return 予約できたら true。登録されていない名前ならログを出して false（今のシーンのまま）
	 * @details 同じフレームに何回呼んでも、最後の1回が効く。シーンを作るのは切り替えのときなので、
	 *          予約した時点では次のシーンのコンストラクタは走らない
	 */
	bool ChangeScene(const std::string& sceneName);

	/** @brief 切り替えを予約しているか */
	bool HasPendingScene() const { return !nextSceneName_.empty(); }

public: //アクセッサ
	//カメラマネージャーの取得
	CameraManager* GetCameraManager() const { return context_.cameraManager; }
	//スプライト共通部の取得
	SpriteCommon* GetSpriteCommon() const { return context_.spriteCommon; }
	//3Dオブジェクト共通部の取得
	Object3dCommon* GetObject3dCommon() const { return context_.object3dCommon; }
	//ライトマネージャーの取得
	LightManager* GetLightManager() const { return context_.lightManager; }
	//ポストプロセスパスの取得
	PostProcessManager* GetPostProcessManager() const { return context_.postProcessManager; }
	//シャドウマップマネージャーの取得
	ShadowMapManager* GetShadowMapManager() const { return context_.shadowMapManager; }
	//サブビューの作成元の取得
	ISubViewProvider* GetSubViewProvider() const { return context_.subViewProvider; }
	//被写界深度の取得
	DepthOfFieldRenderer* GetDepthOfField() const { return context_.depthOfField; }
	//ボリュメトリックの取得
	VolumetricLightRenderer* GetVolumetricLight() const { return context_.volumetricLight; }
	//床の平面反射の取得
	PlanarReflection* GetPlanarReflection() const { return context_.planarReflection; }
	//3D 空間の文字の登録先の取得
	Text3DRenderer* GetText3D() const { return context_.text3D; }

private: //メンバ関数
	//次のシーンが予約されているか
	void ReserveNextScene();

#ifdef USE_IMGUI
	/**
	 * @brief メニューからシーンを切り替える。保存していない変更があれば確認の小窓を出す
	 * @param sceneName 末尾の "Scene" を除いた名前（ChangeScene と同じ）
	 */
	void RequestSceneChangeFromMenu(const std::string& sceneName);
	/** @brief メニューバーの「シーン」の中身を描く */
	void DrawSceneMenu();
	/** @brief 保存していない変更を捨てて切り替えるかの確認を描く。毎フレーム呼ばれる */
	void DrawSceneChangeDialog();
#endif

private: //メンバ変数
	const std::string sceneStr = "Scene";

	//今のシーン
	std::unique_ptr<BaseScene> currentScene_;

	//シーンファクトリー
	SceneFactory* sceneFactory_;

	//シーンの名前
	std::string currentSceneName_ = "";
	// 最初に開くシーン（"〇〇Scene" の形）。ゲーム側が GameConfig で決める
	std::string startSceneName_ = "TitleScene";
	// 予約中の切り替え先（"〇〇Scene" の形）。空なら予約なし
	std::string nextSceneName_ = "";

	//シーンコンテキスト
	SceneContext context_{};

#ifdef USE_IMGUI
	// メニューに並べるシーン名（末尾の "Scene" を除いたもの）。メニューを開いたときだけ作り直す
	std::vector<std::string> menuSceneNames_;
	// 確認の後で切り替える先。空なら確認待ちなし
	std::string pendingSceneName_;
	// 次のフレームで確認の小窓を開く。メニューの中で開くとメニューと一緒に閉じるため
	bool sceneChangePopupRequested_ = false;
#endif
};
} // namespace KCE
