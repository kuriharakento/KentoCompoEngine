#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace KCE
{
class Camera;
class CameraManager;
class ISubViewProvider;
class RenderView;

/**
 * @brief カメラの映像を見るプレビューウィンドウ
 *
 * @details CameraManager のカメラを選ぶと、そのカメラの描画結果を ImGui の画像で出す。
 *          「+」でウィンドウを増やせて、窓ごとに別の RenderView（サブビュー）を持つ。
 *          窓が閉じているときはビューを破棄し、タブの裏などで見えていないときは描画を止める。
 *          解像度は窓の中身の大きさに合わせるが、ビューの作り直しは大きさが落ち着いてから行う。
 *          ポストプロセスは掛からない（サブビューはシーンだけのパイプラインで、HDR のシーンカラーをそのまま出す）。
 *
 *          一覧のタイルや Scene 端の小窓（PiP）を足すときは、PreviewTarget の
 *          「カメラ名 → ビュー」の部分（解像度の追従と有効・無効の切り替え）を使い回せる。
 */
class CameraPreviewEditor
{
public:
	CameraPreviewEditor();
	~CameraPreviewEditor();

	/**
	 * @brief 最初のウィンドウを DebugUIManager に登録する
	 * @details DebugUIManager が使える状態で呼ぶ。
	 * @param provider サブビューの作成元。Finalize まで生きている前提
	 * @param cameraManager カメラの取得元。Finalize まで生きている前提
	 */
	void Initialize(ISubViewProvider* provider, CameraManager* cameraManager);

	/**
	 * @brief ビューを破棄して、登録したウィンドウを外す
	 */
	void Finalize();

	/**
	 * @brief 指定したカメラを選んだ状態で、プレビューウィンドウを開く
	 * @details 閉じているウィンドウがあれば使い回し、無ければ増やす。
	 *          シーケンサがシーケンスカメラを選んだ状態で開くのに使う。
	 * @param cameraName 映すカメラの名前
	 */
	void OpenPreview(const std::string& cameraName);

private:
	/** @brief ウィンドウ1つぶんの状態 */
	struct PreviewTarget
	{
		int index = 0;				//!< ウィンドウの通し番号
		std::string windowName;		//!< ImGui のウィンドウ名（登録名）
		std::string cameraName;		//!< 映すカメラの名前。空なら未選択
		RenderView* view = nullptr;	//!< 描画先。ウィンドウが開いている間だけ持つ。所有しない（provider が持つ）
		uint32_t viewWidth = 0;		//!< view の解像度
		uint32_t viewHeight = 0;
		uint32_t wantedWidth = 0;	//!< 窓の大きさから決めた、欲しい解像度
		uint32_t wantedHeight = 0;
		int stableFrames = 0;		//!< 欲しい解像度が変わらずに続いたフレーム数
	};

	/** @brief ウィンドウを1つ増やして登録する */
	PreviewTarget& AddWindow(bool visible);
	/** @brief 毎フレーム、ウィンドウの描画より前に呼ぶ。描画の仕切り直しと、窓を増やす処理 */
	void BeginFrame();
	/** @brief ウィンドウの中身を描く */
	void DrawWindow(PreviewTarget& target);
	/** @brief ビューを指定解像度で作り直す */
	void RecreateView(PreviewTarget& target, uint32_t width, uint32_t height);
	/** @brief ビューを破棄する */
	void ReleaseView(PreviewTarget& target);

private:
	// サブビューの作成元
	ISubViewProvider* provider_ = nullptr;
	// カメラの取得元
	CameraManager* cameraManager_ = nullptr;
	// ウィンドウの一覧。ラムダが参照を持つので、要素のアドレスを変えないよう unique_ptr で持つ
	std::vector<std::unique_ptr<PreviewTarget>> targets_;
	// 「Scene で見る」で差し替えているカメラ
	Camera* sceneViewCamera_ = nullptr;
	int nextIndex_ = 1;
	bool initialized_ = false;
};
} // namespace KCE
