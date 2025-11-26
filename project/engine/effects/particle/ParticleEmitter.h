#pragma once
#include <memory>
#include <list>
#include <string>
#include "ParticleGroup.h"
#include "component/interface/IParticleComponent.h"
#include "math/AABB.h"
#include "math/BlendMode.h"

/**
 * @brief パーティクルエミッタークラス
 * 
 * パーティクルの発生、更新、描画を管理するクラス。
 * エミット位置、発生レート、発生数などを設定し、パーティクルグループを通じて
 * 複数のパーティクルを効率的に管理・描画する。
 * Component-Based Designパターンを採用し、振る舞いを動的に追加可能。
 */
class ParticleEmitter
{
public:
	/**
	 * @brief デストラクタ
	 */
	~ParticleEmitter();

	/**
	 * @brief エミッターを初期化する
	 * @param groupName パーティクルグループの名前
	 * @param textureFilePath テクスチャファイルのパス
	 */
	void Initialize(const std::string& groupName, const std::string& textureFilePath);

	/**
	 * @brief パーティクルを更新する
	 * @param camera カメラマネージャー（ビルボード計算に使用）
	 */
	void Update(CameraManager* camera);

	/**
	 * @brief パーティクルを描画する
	 * @param dxCommon DirectXCommonインスタンス
	 * @param srvManager SRVマネージャー
	 */
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief ImGuiでのデバッグUI描画
	 */
	void DrawImGui();

	/**
	 * @brief コンポーネントを追加する
	 * @param component 追加するパーティクルコンポーネント
	 */
	void AddComponent(std::shared_ptr<IParticleComponent> component);

	/**
	 * @brief パーティクルの再生を開始する（現在位置から）
	 */
	void Play();

	/**
	 * @brief 指定位置からパーティクルの発生を開始する
	 * @param position 発生位置
	 * @param count 1回の発生で生成するパーティクル数
	 * @param duration 発生継続時間
	 * @param isLoop ループ再生するかどうか
	 */
	void Start(const Vector3& position, uint32_t count, float duration, bool isLoop = false);

	/**
	 * @brief 追従対象からパーティクルの発生を開始する
	 * @param target 追従対象の位置ポインタ
	 * @param count 1回の発生で生成するパーティクル数
	 * @param duration 発生継続時間
	 * @param isLoop ループ再生するかどうか
	 */
	void Start(const Vector3* target, uint32_t count, float duration, bool isLoop = false);

	/**
	 * @brief パーティクルの発生を停止する
	 */
	void StopEmit();

	/**
	 * @brief 再生中かどうかを取得する
	 * @return 再生中ならtrue
	 */
	bool IsPlaying() const { return isPlaying_; }

	/**
	 * @brief ブレンドモードを取得する
	 * @return 現在のブレンドモード
	 */
	BlendMode GetBlendMode() const { return blendMode_; }

	/**
	 * @brief ブレンドモードを設定する
	 * @param mode 設定するブレンドモード
	 */
	void SetBlendMode(BlendMode mode) { blendMode_ = mode; }

	/**
	 * @brief エミッターの位置を設定する
	 * @param position 設定する位置
	 */
	void SetPosition(const Vector3& position) { position_ = position; }

	/**
	 * @brief エミッターの位置を取得する
	 * @return 現在の位置
	 */
	const Vector3& GetPosition() const { return position_; }

	/**
	 * @brief 発生範囲を設定する
	 * @param min 発生範囲の最小値
	 * @param max 発生範囲の最大値
	 */
    void SetEmitRange(const Vector3& min, const Vector3& max);

	/**
	 * @brief 発生レート（間隔）を設定する
	 * @param rate 発生間隔（秒）
	 */
    void SetEmitRate(float rate) { emitRate_ = rate; }

	/**
	 * @brief 1回の発生で生成するパーティクル数を設定する
	 * @param count パーティクル数
	 */
    void SetEmitCount(uint32_t count) { emitCount_ = count; }

	/**
	 * @brief ループ再生を設定する
	 * @param loop ループするかどうか
	 */
    void SetLoop(bool loop) { isLoop_ = loop; }

	/**
	 * @brief ビルボード描画を設定する
	 * @param flag ビルボードを有効にするかどうか
	 */
	void SetBillborad(bool flag) { particleGroup_->SetBillboard(flag); }

	/**
	 * @brief テクスチャを設定する
	 * @param textureFilePath テクスチャファイルのパス
	 */
	void SetTexture(const std::string& textureFilePath) { particleGroup_->SetTexture(textureFilePath); }

	/**
	 * @brief パーティクルのモデルタイプを設定する
	 * @param type パーティクルの形状タイプ
	 */
	void SetModelType(ParticleGroup::ParticleType type) { particleGroup_->SetModelType(type); }

	/**
	 * @brief UV平行移動値を取得する
	 * @return 現在のUV平行移動値
	 */
	Vector3 GetUVTranslate() const { return particleGroup_->GetUVTranslate(); }

	/**
	 * @brief UV平行移動値を設定する
	 * @param translate 設定するUV平行移動値
	 */
	void SetUVTranslate(const Vector3& translate) { particleGroup_->SetUVTranslate(translate); }

	/**
	 * @brief UVスケール値を取得する
	 * @return 現在のUVスケール値
	 */
	Vector3 GetUVScale() const { return particleGroup_->GetUVScale(); }

	/**
	 * @brief UVスケール値を設定する
	 * @param scale 設定するUVスケール値
	 */
	void SetUVScale(const Vector3& scale) { particleGroup_->SetUVScale(scale); }

	/**
	 * @brief UV回転値を取得する
	 * @return 現在のUV回転値
	 */
	Vector3 GetUVRotate() const { return particleGroup_->GetUVRotate(); }

	/**
	 * @brief UV回転値を設定する
	 * @param rotate 設定するUV回転値
	 */
	void SetUVRotate(const Vector3& rotate) { particleGroup_->SetUVRotate(rotate); }

	/**
	 * @brief パーティクルグループを取得する
	 * @return パーティクルグループのポインタ
	 */
	ParticleGroup* GetParticleGroup() { return particleGroup_.get(); }

	//===========================[ 初期パラメータのアクセッサ ]===========================//

	/**
	 * @brief 初期寿命を設定する
	 * @param time 寿命（秒）
	 */
	void SetInitialLifeTime(float time) { initialLifeTime_ = time; }

	/**
	 * @brief 初期速度を設定する
	 * @param velocity 速度ベクトル
	 */
	void SetInitialVelocity(const Vector3& velocity) { initialVelocity_ = velocity; }

	/**
	 * @brief 初期カラーを設定する
	 * @param color カラー（RGBA）
	 */
	void SetInitialColor(const Vector4& color) { initialColor_ = color; }

	/**
	 * @brief 初期スケールを設定する
	 * @param scale スケールベクトル
	 */
	void SetInitialScale(const Vector3& scale) { initialScale_ = scale; }

	/**
	 * @brief 初期回転を設定する
	 * @param rotation 回転ベクトル（オイラー角）
	 */
	void SetInitialRotation(const Vector3& rotation) { initialRotation_ = rotation; }

	/**
	 * @brief 初期寿命を取得する
	 * @return 初期寿命（秒）
	 */
	float GetInitialLifeTime() const { return initialLifeTime_; }

	/**
	 * @brief 初期速度を取得する
	 * @return 初期速度ベクトル
	 */
	Vector3 GetInitialVelocity() const { return initialVelocity_; }

	/**
	 * @brief 初期カラーを取得する
	 * @return 初期カラー（RGBA）
	 */
	Vector4 GetInitialColor() const { return initialColor_; }

	/**
	 * @brief 初期スケールを取得する
	 * @return 初期スケールベクトル
	 */
	Vector3 GetInitialScale() const { return initialScale_; }

	/**
	 * @brief 初期回転を取得する
	 * @return 初期回転ベクトル（オイラー角）
	 */
	Vector3 GetInitialRotation() const { return initialRotation_; }

	/**
	 * @brief 速度のランダム化を設定する
	 * @param isRandom ランダム化するかどうか
	 */
	void SetRandomVelocity(bool isRandom) { isRandomVelocity_ = isRandom; }

	/**
	 * @brief スケールのランダム化を設定する
	 * @param isRandom ランダム化するかどうか
	 */
	void SetRandomScale(bool isRandom) { isRandomScale_ = isRandom; }

	/**
	 * @brief カラーのランダム化を設定する
	 * @param isRandom ランダム化するかどうか
	 */
	void SetRandomColor(bool isRandom) { isRandomColor_ = isRandom; }

	/**
	 * @brief 回転のランダム化を設定する
	 * @param isRandom ランダム化するかどうか
	 */
	void SetRandomRotation(bool isRandom) { isRandomRotation_ = isRandom; }

	/**
	 * @brief ランダム速度の範囲を設定する
	 * @param range 速度の範囲（AABB形式）
	 */
	void SetRandomVelocityRange(const AABB& range) { randomVelocityRange_ = range; }

	/**
	 * @brief ランダムスケールの範囲を設定する
	 * @param range スケールの範囲（AABB形式）
	 */
	void SetRandomScaleRange(const AABB& range) { randomScaleRange_ = range; }

	/**
	 * @brief ランダムカラーの範囲を設定する
	 * @param min カラーの最小値
	 * @param max カラーの最大値
	 */
	void SetRandomColorRange(const Vector4& min, const Vector4& max)
	{
		randomColormin_ = min;
		randomColormax_ = max;
	}

	/**
	 * @brief ランダム回転の範囲を設定する
	 * @param range 回転の範囲（AABB形式）
	 */
	void SetRandomRotationRange(const AABB& range) { randomRotationRange_ = range; }

private:
	/**
	 * @brief パーティクルを発生させる
	 */
	void Emit();

	/**
	 * @brief 初回の発生を即座に行う
	 */
	void EmitFirst();

	/**
	 * @brief 追従対象の位置に合わせてエミット位置を更新する
	 */
	void UpdateEmitPosition();

	/**
	 * @brief 初期パラメータをランダム化する
	 */
	void RandomizeInitialParameters();

private:
	// パーティクルグループ名
	std::string groupName_ = "";
	// パーティクルグループ
	std::unique_ptr<ParticleGroup> particleGroup_ = nullptr;
	// 振る舞いコンポーネントのリスト
	std::list<std::shared_ptr<IParticleComponent>> behaviorComponents_;

	// ブレンドモード
	BlendMode blendMode_ = BlendMode::Alpha;
	// エミッターの位置
	Vector3 position_ = {};
	// 追従対象の位置ポインタ
	const Vector3* target_ = nullptr;
	// 発生範囲の最小値
	Vector3 emitRangeMin_ = {};
	// 発生範囲の最大値
	Vector3 emitRangeMax_ = {};

	// 発生レート（間隔）
	float emitRate_ = 2.0f;
	// 最後の発生からの経過時間
	float timeSinceLastEmit_ = 0.0f;
	// 1回の発生で生成するパーティクル数
	uint32_t emitCount_ = 3;
	// ループ再生フラグ
	bool isLoop_ = false;
	// 再生中フラグ
	bool isPlaying_ = false;
	// 発生経過時間
	float emitTime_ = 0.0f;
	// 発生継続時間
	float duration_ = 0.0f;

	//===========================[ 初期化用プロパティ ]===========================//

	// 初期寿命
	float initialLifeTime_ = 2.0f;
	// 初期速度
	Vector3 initialVelocity_ = { 0.0f, 0.0f, 0.0f };
	// 初期カラー
	Vector4 initialColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 初期スケール
	Vector3 initialScale_ = { 1.0f, 1.0f, 1.0f };
	// 初期回転
	Vector3 initialRotation_ = { 0.0f, 0.0f, 0.0f };
	// 初期速度をランダムにするかどうか
	bool isRandomVelocity_ = false;
	// 初期スケールをランダムにするかどうか
	bool isRandomScale_ = false;
	// 初期色をランダムにするかどうか
	bool isRandomColor_ = false;
	// 初期回転をランダムにするかどうか
	bool isRandomRotation_ = false;
	// ランダム速度の範囲
	AABB randomVelocityRange_ = { Vector3{ -1.0f, -1.0f, -1.0f }, Vector3{ 1.0f,1.0f,1.0f } };
	// ランダムスケールの範囲
	AABB randomScaleRange_ = { Vector3{ 0.5f, 0.5f, 0.5f }, Vector3{ 1.5f, 1.5f, 1.5f } };
	// ランダム回転の範囲
	AABB randomRotationRange_ = { Vector3{ -1.0f, -1.0f, -1.0f }, Vector3{ 1.0f, 1.0f, 1.0f } };
	// ランダム色の最小値
	Vector4 randomColormin_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	// ランダム色の最大値
	Vector4 randomColormax_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};
