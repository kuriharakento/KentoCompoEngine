#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/Guid.h"

namespace KCE
{
class GameObject;

/**
 * @brief 選択されている対象の種別
 */
enum class SelectionKind
{
	None,			//!< 何も選択していない
	GameObject,		//!< シーン上のオブジェクト
	SequenceTrack,	//!< シーケンスのトラック
	SequenceKey,	//!< シーケンスのキーフレーム
	Light,			//!< ライト（種類と名前で指す）
	Camera,			//!< カメラ（名前で指す）
	Text3D,			//!< 3D 文字（名前で指す）
	StageMonitor,	//!< ステージのモニター（名前で指す）
	FontSprite,		//!< FontSprite（デバッグ用の一意な名前で指す）
	ParticleEffect,	//!< パーティクルの effect / emitter（デバッグ用の一意な名前で指す）
};

/**
 * @brief 選択しているライトの種類
 */
enum class SelectionLightType
{
	None,			//!< ライト以外
	Directional,	//!< 平行光源（シーンに1つなので名前を使わない）
	Point,			//!< ポイントライト
	Spot,			//!< スポットライト
};

/**
 * @brief 1つの選択対象
 * @details GameObject は GUID で持つ。ポインタで持つとオブジェクト破棄後に
 *          ダングリングし、名前で持つと改名で壊れるため。
 *          トラック・キーはシーケンス内のインデックスで指す。
 *          ライトとカメラは管理側が名前で持っているので、名前で指す。
 */
struct SelectionItem
{
	SelectionKind kind = SelectionKind::None;
	Guid objectGuid{};		 //!< SelectionKind::GameObject のとき有効
	int trackIndex = -1;	 //!< SequenceTrack / SequenceKey のとき有効
	int clipIndex = -1;		 //!< SequenceKey のとき、キーが属するクリップ
	int keyIndex = -1;		 //!< SequenceKey のとき有効
	int channelIndex = -1;	 //!< SequenceKey のとき、キーが属するカーブ（成分）
	SelectionLightType lightType = SelectionLightType::None;	//!< SelectionKind::Light のとき有効
	std::string name;		 //!< 名前で管理されている選択対象のとき有効

	bool operator==(const SelectionItem& other) const
	{
		return kind == other.kind && objectGuid == other.objectGuid && trackIndex == other.trackIndex && clipIndex == other.clipIndex && keyIndex == other.keyIndex && channelIndex == other.channelIndex
			&& lightType == other.lightType && name == other.name;
	}
	bool operator!=(const SelectionItem& other) const { return !(*this == other); }
};

/**
 * @brief エディタの選択状態を一元管理するクラス
 *
 * @details SEQUENCER_PLAN 5.1。Hierarchy・Inspector・タイムライン・ギズモが
 *          同じ「選択中」を共有するための唯一の窓口。各UIが独自の選択状態を
 *          持つと必ず食い違うため、選択の読み書きは必ずここを経由する。
 *
 *          選択の変更はUndo対象にしない。編集ではなく閲覧操作であり、
 *          履歴に積むとUndoが選択の巻き戻しで埋まって使いづらくなるため。
 */
class SelectionContext
{
public:
	static SelectionContext* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief 選択を置き換える（単一選択）
	 * @param item 選択する対象
	 */
	void Select(const SelectionItem& item);

	/**
	 * @brief GameObjectを選択する（単一選択）
	 * @param gameObject 選択するオブジェクト。nullptrなら選択解除
	 */
	void SelectGameObject(GameObject* gameObject);

	/**
	 * @brief 選択に追加する（複数選択）
	 * @details 既に選択済みの場合は何もしない。
	 * @param item 追加する対象
	 */
	void AddToSelection(const SelectionItem& item);

	/**
	 * @brief 選択状態を反転する（Ctrlクリック相当）
	 * @param item 対象
	 */
	void ToggleSelection(const SelectionItem& item);

	/**
	 * @brief 選択から外す
	 * @param item 対象
	 */
	void RemoveFromSelection(const SelectionItem& item);

	/**
	 * @brief 選択を全て解除する
	 */
	void ClearSelection();

	/**
	 * @brief 対象が選択されているか
	 * @param item 対象
	 * @return 選択されていれば真
	 */
	bool IsSelected(const SelectionItem& item) const;

	/**
	 * @brief 何も選択していないか
	 * @return 選択が空なら真
	 */
	bool IsEmpty() const { return items_.empty(); }

	/**
	 * @brief 選択中の対象すべて
	 * @return 選択リスト
	 */
	const std::vector<SelectionItem>& GetItems() const { return items_; }

	/**
	 * @brief 主対象（最後に選択されたもの）
	 * @details Inspector に表示する対象や、ギズモの基準に使う。
	 * @return 主対象。選択が空なら kind == None のアイテム
	 */
	const SelectionItem& GetPrimary() const;

	/**
	 * @brief 主対象がGameObjectならそれを解決して返す
	 * @details GUIDから GameObjectManager 経由で引くため、
	 *          破棄済みのオブジェクトを掴むことがない。
	 * @return 解決されたGameObject。該当しなければnullptr
	 */
	GameObject* GetPrimaryGameObject() const;

	/**
	 * @brief 選択中のGameObjectをすべて解決して返す
	 * @details 既に破棄されたオブジェクトは結果に含まれない。
	 * @return 解決されたGameObjectのリスト
	 */
	std::vector<GameObject*> GetSelectedGameObjects() const;

	/**
	 * @brief 選択が変更された回数
	 * @details 各UIが「前回描画時から選択が変わったか」を判定するために使う。
	 * @return 変更回数
	 */
	uint64_t GetRevision() const { return revision_; }

public:
	~SelectionContext() = default;

private:
	static std::unique_ptr<SelectionContext> instance_;
	friend std::unique_ptr<SelectionContext> std::make_unique<SelectionContext>();

	SelectionContext() = default;
	SelectionContext(const SelectionContext&) = delete;
	SelectionContext& operator=(const SelectionContext&) = delete;

	// 選択中の対象。末尾が主対象
	std::vector<SelectionItem> items_;
	// 選択が変わるたびに増える通し番号
	uint64_t revision_ = 0;
	// GetPrimary() が選択なしのときに返す固定値
	static const SelectionItem kEmptyItem;
};
} // namespace KCE
