#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "engine/gameobject/component/base/Component.h"
#include "engine/gameobject/component/base/IRenderableComponent.h"
#include "graphics/3d/SkinnedObject3d.h"
#include "jsonEditor/JsonEditableBase.h"

namespace KCE
{
namespace GameObjectComponent
{
/**
 * @brief 静的ライブラリから、この .obj を確実に取り込ませるための空関数（REGISTER_COMPONENT を走らせる）
 */
void ForceLinkSkinnedModelRendererComponent();

/**
 * @brief ボーンアニメーション付きのモデルを1つ描くコンポーネント
 *
 * - ModelRendererComponent のスキンモデル版。モデルは Resources/models/<名前>/<名前>.gltf
 * - 再生するアニメーションはクリップ名で選ぶ。空なら止めたまま（バインドポーズ）
 * - 時間は持ち主の GameObject の時計で進む（敵だけ止めるとアニメも止まる）
 * - インスペクタにプレビューがある。更新が止まっているエディタでも、再生・一時停止・時刻のスクラブができる
 */
class SkinnedModelRendererComponent : public Component, public IRenderableComponent, public JsonEditableBase
{
public:
	SkinnedModelRendererComponent();

	/** @brief 持ち主が決まったところで SkinnedObject3d を用意する */
	void Awake() override;
	/** @brief アニメーションを進める時間をためる（実際に進めるのは UpdateRenderTransform） */
	void Update() override;

	IRenderable3d* GetRenderable3d() const override { return skinned_.get(); }
	void UpdateRenderTransform() override;
	bool GetCastShadow() const override { return castShadow_; }

	/** @brief 描くモデルを設定する（フォルダ名。例: "walk"） */
	void SetModel(const std::string& modelName);
	const std::string& GetModelName() const { return modelName_; }

	/** @brief 再生するアニメーションをクリップ名で設定する。空なら停止 */
	void SetAnimation(const std::string& animationName, bool loop = true);
	const std::string& GetAnimationName() const { return animationName_; }

	/** @brief 再生速度の倍率 */
	void SetSpeed(float speed);

	/** @return 中の SkinnedObject3d。用意できていなければ nullptr。所有権は移らない */
	SkinnedObject3d* GetSkinnedObject3d() const { return skinned_.get(); }

	void SetCastShadow(bool cast) { castShadow_ = cast; }
	void SetColor(const Vector4& color);

#ifdef USE_IMGUI
	void DrawImGui() override;
#endif

private:
	/** @brief モデル名・アニメーション名などの変更を SkinnedObject3d へ反映する */
	void ApplySettings();

	// このコンポーネントが所有する描画物
	std::unique_ptr<SkinnedObject3d> skinned_;

	// --- プレハブに保存する値 ---
	std::string modelName_;
	std::string animationName_;
	bool loop_ = true;
	float speed_ = 1.0f;
	bool castShadow_ = true;

	// 反映済みの値。プレハブ読み込みやインスペクタの変更を拾うために持つ
	std::string appliedModelName_;
	std::string appliedAnimationName_;
	bool appliedLoop_ = true;
	float appliedSpeed_ = 1.0f;

	// このフレームに進める時間（Update やプレビューがためる）
	float pendingDelta_ = 0.0f;
	// Update が最後に時間を渡したフレーム。プレビューとの二重加算を避ける
	uint64_t lastUpdateFrame_ = UINT64_MAX;
	// インスペクタのプレビューを再生中か（保存しない）
	bool previewPlaying_ = false;
};
} // namespace GameObjectComponent
} // namespace KCE
