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
 * - まとめる条件は「同じモデル名」かつ「素材が、色以外は読み込んだままで、1体ごとにいじられていない」こと。
 *   色は1体ごとのデータとして渡すので、色が違ってもまとめられる。ライティングを切った物や、輪郭線を付けた物は1体ずつ描く
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

	/**
	 * @brief 影のパス用にまとめ分けする
	 * @param visible そのライト（カスケード1枚・スポット1つ）で見えている物
	 * @details 影のシェーダーはワールド行列とライトの行列しか使わないので、ビューの行列は要らない。
	 *          見える物はライトごとに違うので、ライトごとに呼ぶ
	 */
	void BuildForShadow(const std::vector<const GameObjectRenderer::Entry*>& visible);

	/** @brief Build でまとめられなかった物。次に Build を呼ぶまで有効 */
	const std::vector<const GameObjectRenderer::Entry*>& GetSingles() const { return singles_; }

	/** @brief まとめた分を描く（不透明・フォワード） */
	void DrawForward(Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager);
	/** @brief まとめた分を描く（G-Buffer） */
	void DrawGBuffer(Camera* camera);
	/** @brief まとめた分を描く（影） */
	void DrawShadow(ShadowMapManager* shadowMapManager);

	/**
	 * @brief 影の管理を直接渡す。描画パスが毎フレーム渡す（所有しない）
	 * @details GameObject の Object3d は ShadowSystem に登録されないので、影の管理を持たない。
	 *          これが無いと、まとめて描く影はライトの行列を結び付けないまま描かれ、でたらめな影になる。
	 *          まとめて描く Forward も、シャドウマップを受け取れない
	 */
	void SetShadowMapManager(ShadowMapManager* shadowMapManager) { shadowMapManager_ = shadowMapManager; }
	/** @brief まとめて描くときに使う影の管理。直接渡された物を優先し、無ければ Build のときに描画物から拾った物。どちらも無ければ nullptr */
	ShadowMapManager* GetShadowMapManagerFromObjects() const { return shadowMapManager_ ? shadowMapManager_ : shadowMapManagerFromObjects_; }

	/**
	 * @brief この描画物をまとめて描いてよいか調べる
	 * @param object3d 調べる描画物
	 * @param outColorScale まとめてよいとき、元モデルの色に掛けるとこの物の色になる倍率の出力先（任意）
	 * @return まとめてよければ ModelManager が持つ元のモデル。だめなら nullptr（所有権は移らない）
	 * @details 物ごとに1回だけ呼ぶ想定（GameObjectRenderer::Collect）。ビューごとに呼ぶと重い
	 */
	static Model* FindSharedModel(const Object3d* object3d, Vector4* outColorScale = nullptr);

	/** @brief まとめて描くのを使うか（比べるときに切る） */
	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }

	/** @brief 直前の Build でまとめた物の数 */
	uint32_t GetLastInstancedCount() const { return lastInstancedCount_; }
	/** @brief 直前の Build でできたまとまりの数（＝まとめた分の描画命令の数） */
	uint32_t GetLastGroupCount() const { return lastGroupCount_; }

private:
	/**
	 * @brief まとめ分けの本体
	 * @param viewProjection ビューの行列。nullptr なら WVP を作らない（影のパス）
	 */
	void BuildInternal(const std::vector<const GameObjectRenderer::Entry*>& visible, const Matrix4x4* viewProjection);

	struct Group
	{
		// ModelManager が持つ元のモデル。所有しない
		Model* model = nullptr;
		// このまとまりに入った物。配列は使い回す
		std::vector<const GameObjectRenderer::Entry*> entries;
		// このまとまりの物の行列。ビューごとに WVP だけ作り直す。配列は使い回す
		std::vector<InstanceData> instanceData;
		// このまとまり用のバッファ。1回の描画ごとに別のものを使う（下の AcquireRenderer）。作ったものは使い回す
		std::vector<std::unique_ptr<InstancedModelRenderer>> renderers;
		// このフレームで使い始めたバッファの数。フレームが変わると 0 に戻す
		uint32_t usedRenderers = 0;
	};

	/**
	 * @brief このまとまりの、まだこのフレームで使っていないバッファを1つ渡す
	 * @details 行列は描画命令を積む時点で CPU が書き込むが、GPU が読むのはフレームの最後にまとめて実行するとき。
	 *          1つのバッファを使い回すと、同じフレームの別のビュー・別のパス（影のカスケード、G-Buffer、Forward）が
	 *          後から書いた行列で、先に積んだ描画まで描かれてしまう。描画ごとに別のバッファを使って避ける。
	 * @param group 使うまとまり。instanceData に今回の行列が入っている
	 * @return 今回の描画用のバッファ。足りなければ作る
	 */
	InstancedModelRenderer* AcquireRenderer(Group& group);

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	bool enabled_ = true;
	// 描画物から拾ったカスケードシャドウの管理。所有しない（ShadowSystem が持つ）
	ShadowMapManager* shadowMapManagerFromObjects_ = nullptr;
	// 描画パスから直接渡された影の管理。所有しない
	ShadowMapManager* shadowMapManager_ = nullptr;

	// 元モデルごとのまとまり。確保し直さないよう持ち続ける
	std::unordered_map<Model*, Group> groups_;
	// AcquireRenderer が最後に使用数を戻したフレーム
	uint64_t renderersResetFrame_ = ~0ull;
	// 大きいものに作り直されて、使わなくなったバッファ。前のフレームの描画が読み終わるまで壊せないので、終了まで置いておく
	std::vector<std::unique_ptr<InstancedModelRenderer>> retiredRenderers_;
	// このフレームに中身が入ったまとまり。所有は groups_
	std::vector<Group*> activeGroups_;
	// まとめられなかった物。配列は使い回す
	std::vector<const GameObjectRenderer::Entry*> singles_;

	uint32_t lastInstancedCount_ = 0;
	uint32_t lastGroupCount_ = 0;
};
} // namespace KCE
