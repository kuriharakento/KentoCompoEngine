#pragma once
#include <cstdint>
#include <vector>

#include "base/GraphicsTypes.h"
#include "graphics/3d/IRenderable3d.h"
#include "graphics/view/RenderLayer.h"
#include "math/AABB.h"

namespace KCE
{
class GameObject;
class IRenderable3d;

/**
 * @brief GameObject の描画をまとめて受け持つ
 *
 * - フレームの頭（描画パイプラインの前）に、登録された GameObject と子を1回だけ見て「描く物の一覧」を作る。
 *   このときワールド行列を確定させ、ワールドの境界箱も1回だけ計算しておく
 * - ビュー（本編・モニター・反射・カスケード・スポット影）ごとに、その一覧から「見えている物のリスト」を作って描く。
 *   視錐台の判定はワールドの境界箱と6枚の面で行う（ビューごとに行列を掛け直さない）
 * - 一覧もリストも配列を使い回し、毎フレーム確保しない
 * - 親子の描かれ方は、親ではなくその物自身の描画キュー・描き方・レイヤーで決まる
 * - メインスレッドだけで使う
 */
class GameObjectRenderer
{
public:
	/** @brief 描く物1つぶん。一覧はそのフレームの間だけ有効（GameObject を消すと次のフレームで作り直す） */
	struct Entry
	{
		// 所有しない。GameObjectManager に登録された木のどこか
		GameObject* object = nullptr;
		// 所有しない。object が持つ
		IRenderable3d* renderable = nullptr;
		AABB worldBounds;
		// 境界箱を持たない物（スキンメッシュなど）は判定せず必ず描く
		bool hasBounds = false;
		RenderLayerMask layer = kRenderLayerDefault;
		RenderQueue queue = RenderQueue::Opaque;
		RenderingType renderingType = RenderingType::Forward;
		bool castShadow = true;
	};

	/** @brief どのパスのリストを作るか */
	enum class Pass
	{
		GBuffer,		//!< 不透明・ディファード
		Forward,		//!< 不透明・フォワード
		Transparent,	//!< 半透明（並び替えは呼ぶ側）
		Shadow,			//!< 影を落とす物（キューは問わない）
	};

	/**
	 * @brief 描く物の一覧を作り直す。フレームに1回、描画の前に呼ぶ
	 * @param roots 登録された GameObject（子は中でたどる）
	 */
	void Collect(const std::vector<GameObject*>& roots);

	/**
	 * @brief このフレームの一覧がまだ作られていなければ作る
	 * @details 描画パイプラインを通らずに描かれたときの保険。作ってあれば何もしない
	 */
	void EnsureCollected(const std::vector<GameObject*>& roots);

	/**
	 * @brief パスとビューに合う、見えている物のリストを作る
	 * @param pass 対象のパス
	 * @param viewProjection 判定に使う行列。nullptr なら判定せず全部入れる
	 * @param layerMask ビューのレイヤー。影は見ない
	 * @return 見えている物。次に GatherVisible を呼ぶまで有効
	 */
	const std::vector<const Entry*>& GatherVisible(Pass pass, const Matrix4x4* viewProjection, RenderLayerMask layerMask);

	/** @brief 視錐台カリングを使うか */
	void SetCullingEnabled(bool enabled) { cullingEnabled_ = enabled; }
	bool IsCullingEnabled() const { return cullingEnabled_; }

	/** @brief 描いた数・省いた数を数える（GatherVisible 以外の経路で判定したときに足す） */
	void CountDrawn() { RollCounters(); ++drawnCount_; }
	void CountCulled() { RollCounters(); ++culledCount_; }
	/** @brief 前のフレームの、全ビューと影の合計 */
	uint32_t GetLastFrameDrawnCount() const { return lastFrameDrawnCount_; }
	uint32_t GetLastFrameCulledCount() const { return lastFrameCulledCount_; }
	/** @brief 今の一覧に入っている物の数 */
	size_t GetEntryCount() const { return entries_.size(); }

private:
	/** @brief 一覧に1つ足す。本体の描画物と、描画物を持つコンポーネントの両方から呼ぶ */
	void AddEntry(GameObject* object, IRenderable3d* renderable, bool castShadow);

	/** @brief フレームが変わっていたら、数を前のフレームへ締める */
	void RollCounters();

	// 描く物の一覧。フレームごとに作り直すが、配列は使い回す
	std::vector<Entry> entries_;
	// GatherVisible の結果。配列は使い回す
	std::vector<const Entry*> visible_;
	// 一覧を作ったフレーム（TimeManager のフレーム番号）
	uint64_t collectedFrame_ = UINT64_MAX;
	bool cullingEnabled_ = true;
	uint32_t drawnCount_ = 0;
	uint32_t culledCount_ = 0;
	uint32_t lastFrameDrawnCount_ = 0;
	uint32_t lastFrameCulledCount_ = 0;
	uint64_t countedFrame_ = 0;
};
} // namespace KCE
