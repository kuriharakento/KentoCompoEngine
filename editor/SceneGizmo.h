#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "editor/SelectionContext.h"
#include "math/MatrixFunc.h"
#include "math/Quaternion.h"
#include "math/Vector3.h"

namespace KCE
{
/** @brief ギズモで使える操作。ビットで組み合わせる */
enum GizmoOperationFlags : uint32_t
{
	kGizmoTranslate = 1u << 0,	//!< 移動
	kGizmoRotate = 1u << 1,		//!< 回転
	kGizmoScale = 1u << 2,		//!< 拡大縮小
	kGizmoAll = kGizmoTranslate | kGizmoRotate | kGizmoScale,
};

/**
 * @brief ギズモで動かした結果。ワールド行列を位置・回転・拡大率に分けたもの
 */
struct GizmoResult
{
	Vector3 translate{};
	Quaternion rotate = Quaternion::Identity();
	Vector3 scale{ 1.0f, 1.0f, 1.0f };
};

/**
 * @brief ギズモで動かせる物1種類ぶんの窓口
 *
 * - getPose: 今のワールド行列と使える操作（GizmoOperationFlags）を返す。false ならギズモを出さない
 * - apply: 動かした結果を受け取る。dragId が同じ間は同じドラッグなので、Undo のコマンドはこれで1つにまとめる
 */
struct GizmoTarget
{
	std::function<bool(const SelectionItem& item, Matrix4x4& world, uint32_t& operations)> getPose;
	std::function<void(const SelectionItem& item, const GizmoResult& after, uint32_t dragId)> apply;
};

/**
 * @brief Scene 画像の上にギズモを1つ出して、選んでいる物を動かす
 *
 * - 置ける物の持ち主が、選択の種類ごとに GizmoTarget を登録する（DebugUIManager::RegisterInspector と同じ形）
 * - 主選択の種類に登録が無いか、getPose が false なら、SelectionKind::None の登録を順に試す（シーケンスのカメラなど）
 * - 移動・回転・拡大の切り替えとワールド／ローカルは、Scene の左上のツールバーで選ぶ
 * - 描くのは DebugUIManager の Scene 重ね描きの中。USE_IMGUI が無いときは何もしない
 */
class SceneGizmo
{
public:
	static SceneGizmo* GetInstance();
	static bool HasInstance();

	/** @brief DebugUIManager に Scene 重ね描きを登録する。DebugUIManager の初期化の後に呼ぶ */
	void Initialize();
	/** @brief 登録を全部外して片付ける。DebugUIManager の終了より前に呼ぶ */
	void Finalize();

	/**
	 * @brief ギズモで動かせる物を登録する
	 * @param owner 登録元。Unregister で外すときの鍵。所有しない
	 * @param kind 受け持つ選択の種類。None は「他に出すものが無いとき」の候補
	 * @param target 窓口
	 */
	void RegisterTarget(void* owner, SelectionKind kind, GizmoTarget target);
	/** @brief owner が登録した物をすべて外す */
	void Unregister(void* owner);

	~SceneGizmo() = default;

private:
	static std::unique_ptr<SceneGizmo> instance_;
	friend std::unique_ptr<SceneGizmo> std::make_unique<SceneGizmo>();

	SceneGizmo() = default;
	SceneGizmo(const SceneGizmo&) = delete;
	SceneGizmo& operator=(const SceneGizmo&) = delete;

	/** @brief 登録1つぶん */
	struct Entry
	{
		void* owner = nullptr;					//!< 登録元。所有しない
		SelectionKind kind = SelectionKind::None;
		GizmoTarget target;
	};

	/** @brief Scene 重ね描き。ツールバーとギズモを描く */
	void Draw();
	/**
	 * @brief 選択の種類に合う登録を探して、姿勢を取る
	 * @return 見つかった登録（所有しない、このフレームの間だけ使う）。無ければ nullptr
	 */
	const Entry* FindTarget(SelectionKind kind, const SelectionItem& item, Matrix4x4& world, uint32_t& operations) const;
	/** @brief 移動・回転・拡大の切り替えとワールド／ローカルを描く */
	void DrawToolbar(uint32_t operations);

	std::vector<Entry> entries_;
	// 選んでいる操作（kGizmoTranslate / kGizmoRotate / kGizmoScale のどれか1つ）
	uint32_t operation_ = kGizmoTranslate;
	// ワールド座標で動かすか（拡大縮小は常にローカル）
	bool worldSpace_ = true;
	// ギズモを掴んだ回数。ドラッグごとに別の Undo にするための番号
	uint32_t dragId_ = 0;
	// 前のフレームでギズモを掴んでいたか
	bool wasUsing_ = false;
};
} // namespace KCE
