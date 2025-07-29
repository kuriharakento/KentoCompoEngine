#pragma once
#include <memory>
#include <list>
#include <string>
#include "ParticleGroup.h"
#include "component/interface/IParticleComponent.h"
#include "math/AABB.h"

class ParticleEmitter
{
public:
	~ParticleEmitter();
	void Initialize(const std::string& groupName, const std::string& textureFilePath);
	void Update(CameraManager* camera);
	void Draw(DirectXCommon* dxCommon, SrvManager* srvManager);
	void DrawImGui();
	void AddComponent(std::shared_ptr<IParticleComponent> component);

	void Play();
	void Start(const Vector3& position, uint32_t count, float duration, bool isLoop = false);
	void Start(const Vector3* target, uint32_t count, float duration, bool isLoop = false);
	void StopEmit();

	void SetPosition(const Vector3& position) { position_ = position; }
	const Vector3& GetPosition() const { return position_; }
    void SetEmitRange(const Vector3& min, const Vector3& max);
    void SetEmitRate(float rate) { emitRate_ = rate; }
    void SetEmitCount(uint32_t count) { emitCount_ = count; }
    void SetLoop(bool loop) { isLoop_ = loop; }
	void SetBillborad(bool flag) { particleGroup_->SetBillboard(flag); }
	void SetTexture(const std::string& textureFilePath) { particleGroup_->SetTexture(textureFilePath); }
	void SetModelType(ParticleGroup::ParticleType type) { particleGroup_->SetModelType(type); }
	Vector3 GetUVTranslate() const { return particleGroup_->GetUVTranslate(); }
	void SetUVTranslate(const Vector3& translate) { particleGroup_->SetUVTranslate(translate); }
	Vector3 GetUVScale() const { return particleGroup_->GetUVScale(); }
	void SetUVScale(const Vector3& scale) { particleGroup_->SetUVScale(scale); }
	Vector3 GetUVRotate() const { return particleGroup_->GetUVRotate(); }
	void SetUVRotate(const Vector3& rotate) { particleGroup_->SetUVRotate(rotate); }
	ParticleGroup* GetParticleGroup() { return particleGroup_.get(); }

	// 初期パラメータのアクセッサ
	void SetInitialLifeTime(float time) { initialLifeTime_ = time; }
	void SetInitialVelocity(const Vector3& velocity) { initialVelocity_ = velocity; }
	void SetInitialColor(const Vector4& color) { initialColor_ = color; }
	void SetInitialScale(const Vector3& scale) { initialScale_ = scale; }
	void SetInitialRotation(const Vector3& rotation) { initialRotation_ = rotation; }
	float GetInitialLifeTime() const { return initialLifeTime_; }
	Vector3 GetInitialVelocity() const { return initialVelocity_; }
	Vector4 GetInitialColor() const { return initialColor_; }
	Vector3 GetInitialScale() const { return initialScale_; }
	Vector3 GetInitialRotation() const { return initialRotation_; }
	void SetRandomVelocity(bool isRandom) { isRandomVelocity_ = isRandom; }
	void SetRandomScale(bool isRandom) { isRandomScale_ = isRandom; }
	void SetRandomColor(bool isRandom) { isRandomColor_ = isRandom; }
	void SetRandomRotation(bool isRandom) { isRandomRotation_ = isRandom; }
	void SetRandomVelocityRange(const AABB& range) { randomVelocityRange_ = range; }
	void SetRandomScaleRange(const AABB& range) { randomScaleRange_ = range; }
	void SetRandomColorRange(const Vector4& min, const Vector4& max)
	{
		randomColormin_ = min;
		randomColormax_ = max;
	}
	void SetRandomRotationRange(const AABB& range) { randomRotationRange_ = range; }

private:
	// パーティクルの生成
	void Emit();
	//　初回の発生を即座に行う
	void EmitFirst();
	// 追従対象の位置に合わせてエミット位置を更新
	void UpdateEmitPosition();
	// 初期パラメータをランダム化
	void RandomizeInitialParameters();

private:
	std::string groupName_ = "";
	std::unique_ptr<ParticleGroup> particleGroup_ = nullptr;
	std::list<std::shared_ptr<IParticleComponent>> behaviorComponents_;

	Vector3 position_ = {};
	const Vector3* target_ = nullptr;
	Vector3 emitRangeMin_ = {};
	Vector3 emitRangeMax_ = {};

	float emitRate_ = 2.0f;
	float timeSinceLastEmit_ = 0.0f;
	uint32_t emitCount_ = 3;
	bool isLoop_ = false;
	bool isPlaying_ = false;
	float emitTime_ = 0.0f;
	float duration_ = 0.0f;

	// --- 初期化用プロパティ ---
	float initialLifeTime_ = 2.0f;
	Vector3 initialVelocity_ = { 0.0f, 0.0f, 0.0f };
	Vector4 initialColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector3 initialScale_ = { 1.0f, 1.0f, 1.0f };
	Vector3 initialRotation_ = { 0.0f, 0.0f, 0.0f }; // 初期回転
	bool isRandomVelocity_ = false; // 初期速度をランダムにするかどうか
	bool isRandomScale_ = false; // 初期スケールをランダムにするかどうか
	bool isRandomColor_ = false; // 初期色をランダムにするかどうか
	bool isRandomRotation_ = false; // 初期回転をランダムにするかどうか
	AABB randomVelocityRange_ = { Vector3{ -1.0f, -1.0f, -1.0f }, Vector3{ 1.0f,1.0f,1.0f } };
	AABB randomScaleRange_ = { Vector3{ 0.5f, 0.5f, 0.5f }, Vector3{ 1.5f, 1.5f, 1.5f } };
	AABB randomRotationRange_ = { Vector3{ -1.0f, -1.0f, -1.0f }, Vector3{ 1.0f, 1.0f, 1.0f } }; // ランダム回転の範囲
	Vector4 randomColormin_ = { 0.0f, 0.0f, 0.0f, 1.0f }; // ランダム色の最小値
	Vector4 randomColormax_ = { 1.0f, 1.0f, 1.0f, 1.0f }; // ランダム色の最大値
};
