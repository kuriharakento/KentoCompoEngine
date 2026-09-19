#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/GraphicsTypes.h"
#include "gameobject/manager/GameObjectRenderer.h"

namespace KCE
{
class Camera;
class DirectXCommon;
class InstancedModelRenderer;
class LightManager;
class Model;
class Object3d;
class ShadowMapManager;
class SrvManager;

/**
 * @brief 見えている物のうち、同じモデル・同じ見た目の物をまとめて1回で描く
 *
 * - GameObjectRenderer が作った「見えている物のリスト」を受け取り、
 *   まとめて描ける物（instanced）と1体ずつ描く物（single）に分ける
 * - まとめる条件は「同じモデル名」かつ「素材が読み込んだままで、1体ごとにいじられていない」こと。
 *   色を変えた物や、ライティングを切った物、輪郭線を付けた物は1体ずつ描く
 * - まとめて描く分は ModelManager が持つ元のモデルで描く。1体ごとの複製は使わない
 * - 行列の配列もまとめ分けの入れ物も使い回し、毎フレーム確保しない
 * - メインスレッドだけで使う
 */
class GameObjectInstancing
{
public:
	// InstancedModelRenderer を前方宣言のまま unique_ptr で持つので、ここでは宣言だけ
	GameObjectInstancing();
	~GameObjectInstancing();

	/**
	 * @brief 使う物をもらう
	 * @param dxCommon 所有しない。Framework が持つ
	 * @param srvManager 所有しない。Framework が持つ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/** @brief 確保したインスタンス用のバッファを全部捨てる */
	void Finalize();

	/**
	 * @brief 見えている物をまとめ分けする
	 * @param visible GameObjectRenderer::GatherVisible の結果
	 * @param camera まとめて描くときの行列づくりに使うカメラ。nullptr なら全部1体ずつになる
	 * @details 呼んだ後に GetSingles でまとめられなかった物を取り出して、いつも通り描く
	 */
	void Build(const std::vector<const GameObjectRenderer::Entry*>& visible, Camera* camera);

	/** @brief Build でまとめられなかった物。次に Build を呼ぶまで有効 */
	const std::vector<const GameObjectRenderer::Entry*>& GetSingles() const { return singles_; }

	/** @brief まとめた分を描く（不透明・フォワード） */
	void DrawForward(Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager);
	/** @brief まとめた分を描く（G-Buffer） */
	void DrawGBuffer(Camera* camera);
	/** @brief まとめた分を描く（影） */
	void DrawShadow(Camera* camera, ShadowMapManager* shadowMapManager);

	/** @brief Build のときに描画物から拾ったカスケードシャドウの管理。無ければ nullptr */
	ShadowMapManager* GetShadowMapManagerFromObjects() const { return shadowMapManagerFromObjects_; }

	/**
	 * @brief この描画物をまとめて描いてよいか調べる
	 * @param object3d 調べる描画物
	 * @return まとめてよければ ModelManager が持つ元のモデル。だめなら nullptr（所有権は移らない）
	 * @details 物ごとに1回だけ呼ぶ想定（GameObjectRenderer::Collect）。ビューごとに呼ぶと重い
	 */
	static Model* FindSharedModel(const Object3d* object3d);

	/** @brief まとめて描くのを使うか（比べるときに切る） */
	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }

	/** @brief 直前の Build でまとめた物の数 */
	uint32_t GetLastInstancedCount() const { return lastInstancedCount_; }
	/** @brief 直前の Build でできたまとまりの数（＝まとめた分の描画命令の数） */
	uint32_t GetLastGroupCount() const { return lastGroupCount_; }

private:
	struct Group
	{
		// ModelManager が持つ元のモデル。所有しない
		Model* model = nullptr;
		// このまとまりに入った物。配列は使い回す
		std::vector<const GameObjectRenderer::Entry*> entries;
		// このまとまりの物の行列。ビューごとに WVP だけ作り直す。配列は使い回す
		std::vector<TransformationMatrix> instanceData;
		// このまとまり用のバッファ。モデルごとに1つ作って使い回す
		std::unique_ptr<InstancedModelRenderer> renderer;
	};

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	bool enabled_ = true;
	// 描画物から拾ったカスケードシャドウの管理。所有しない（ShadowSystem が持つ）
	ShadowMapManager* shadowMapManagerFromObjects_ = nullptr;

	// 元モデルごとのまとまり。確保し直さないよう持ち続ける
	std::unordered_map<Model*, Group> groups_;
	// このフレームに中身が入ったまとまり。所有は groups_
	std::vector<Group*> activeGroups_;
	// まとめられなかった物。配列は使い回す
	std::vector<const GameObjectRenderer::Entry*> singles_;

	uint32_t lastInstancedCount_ = 0;
	uint32_t lastGroupCount_ = 0;
};
} // namespace KCE
