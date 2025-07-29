#include "ParticleEmitter.h"

// component
#include "component/interface/IParticleGroupComponent.h"
#include "component/interface/IParticleBehaviorComponent.h"

// editor
#include "imgui/imgui.h"
// math
#include "math/VectorColorCodes.h"
#include "math/MathUtils.h"
// system
#include "manager/graphics/LineManager.h"
#include "manager/effect/ParticleManager.h"


ParticleEmitter::~ParticleEmitter()
{
	particleGroup_.reset();
	behaviorComponents_.clear();
	ParticleManager::GetInstance()->UnregisterEmitter(groupName_);
}

void ParticleEmitter::Initialize(const std::string& groupName, const std::string& textureFilePath)
{
	groupName_ = groupName;
	particleGroup_ = std::make_unique<ParticleGroup>();
	particleGroup_->Initialize(groupName, textureFilePath);
	ParticleManager::GetInstance()->RegisterEmitter(groupName_, this);
}

void ParticleEmitter::Update(CameraManager* camera)
{
	// 発生位置の更新
	UpdateEmitPosition();

	// パーティクル生成
	Emit();

	// パーティクル単体に作用するコンポーネントの更新
	for (auto& particle : particleGroup_->GetParticles())
	{
		for (auto& behavior : behaviorComponents_)
		{
			if (auto behaviorComponent = std::dynamic_pointer_cast<IParticleBehaviorComponent>(behavior))
			{
				behaviorComponent->Update(particle);
			}
		}
	}

	// パーティクルグループ全体に作用するコンポーネントの更新
	for (auto& behavior : behaviorComponents_)
	{
		if (auto groupComponent = std::dynamic_pointer_cast<IParticleGroupComponent>(behavior))
		{
			groupComponent->Update(*particleGroup_);
		}
	}

	particleGroup_->Update(camera);
}

void ParticleEmitter::Draw(DirectXCommon* dxCommon, SrvManager* srvManager)
{
#ifdef _DEBUG
	// 発生ポイントを描画
	LineManager::GetInstance()->DrawSphere(
		position_,
		0.1f,
		VectorColorCodes::Red
	);
	LineManager::GetInstance()->DrawAABB(
		AABB(
			position_ + emitRangeMin_,
			position_ + emitRangeMax_),
		VectorColorCodes::Green
	);
#endif

	if (!particleGroup_) return;
	particleGroup_->Draw(dxCommon, srvManager);
}

void ParticleEmitter::DrawImGui()
{
#ifdef _DEBUG
	ImGui::SeparatorText("ParticleEmitter Info");

	// 再生・停止ボタン
	if (ImGui::Button(isPlaying_ ? "Stop" : "Play"))
	{
		if (isPlaying_) StopEmit();
		else Play();
	}
	ImGui::SameLine();
	ImGui::Text("isPlaying: %s", isPlaying_ ? "true" : "false");

	// 位置
	Vector3 pos = position_;
	if (ImGui::DragFloat3("Position", &pos.x, 0.01f))
	{
		SetPosition(pos);
	}

	// エミット範囲
	Vector3 emitMin = emitRangeMin_;
	Vector3 emitMax = emitRangeMax_;
	if (ImGui::DragFloat3("Emit Range Min", &emitMin.x, 0.01f))
	{
		emitRangeMin_ = emitMin;
	}
	if (ImGui::DragFloat3("Emit Range Max", &emitMax.x, 0.01f))
	{
		emitRangeMax_ = emitMax;
	}

	// レート・カウント・ループ・継続時間
	float emitRate = emitRate_;
	if (ImGui::DragFloat("Emit Rate", &emitRate, 0.01f, 0.0f, 100.0f))
	{
		SetEmitRate(emitRate);
	}
	int emitCount = static_cast<int>(emitCount_);
	if (ImGui::DragInt("Emit Count", &emitCount, 1, 1, 1000))
	{
		SetEmitCount(static_cast<uint32_t>(emitCount));
	}
	bool isLoop = isLoop_;
	if (ImGui::Checkbox("Loop", &isLoop))
	{
		SetLoop(isLoop);
	}
	ImGui::DragFloat("Duration", &duration_, 0.01f, 0.0f, 100.0f);

	// 初期値
	float life = initialLifeTime_;
	if (ImGui::DragFloat("Initial LifeTime", &life, 0.01f, 0.0f, 100.0f))
	{
		SetInitialLifeTime(life);
	}
	Vector3 vel = initialVelocity_;
	if (ImGui::InputFloat3("Initial Velocity", &vel.x))
	{
		SetInitialVelocity(vel);
	}
	Vector4 col = initialColor_;
	if (ImGui::ColorEdit4("Initial Color", &col.x))
	{
		SetInitialColor(col);
	}
	Vector3 scale = initialScale_;
	if (ImGui::InputFloat3("Initial Scale", &scale.x))
	{
		SetInitialScale(scale);
	}
	Vector3 rot = initialRotation_;
	if (ImGui::InputFloat3("Initial Rotation", &rot.x))
	{
		SetInitialRotation(rot);
	}

	// ランダム設定
	ImGui::SeparatorText("Randomize");
	bool randomVel = isRandomVelocity_;
	if (ImGui::Checkbox("Random Velocity", &randomVel))
	{
		SetRandomVelocity(randomVel);
	}
	if (randomVel)
	{
		Vector3 minV = randomVelocityRange_.min_;
		Vector3 maxV = randomVelocityRange_.max_;
		if (ImGui::InputFloat3("Random Velocity Min", &minV.x))
		{
			randomVelocityRange_.min_ = minV;
			SetRandomVelocityRange(randomVelocityRange_);
		}
		if (ImGui::InputFloat3("Random Velocity Max", &maxV.x))
		{
			randomVelocityRange_.max_ = maxV;
			SetRandomVelocityRange(randomVelocityRange_);
		}
	}
	bool randomScale = isRandomScale_;
	if (ImGui::Checkbox("Random Scale", &randomScale))
	{
		SetRandomScale(randomScale);
	}
	if (randomScale)
	{
		Vector3 minS = randomScaleRange_.min_;
		Vector3 maxS = randomScaleRange_.max_;
		if (ImGui::InputFloat3("Random Scale Min", &minS.x))
		{
			randomScaleRange_.min_ = minS;
			SetRandomScaleRange(randomScaleRange_);
		}
		if (ImGui::InputFloat3("Random Scale Max", &maxS.x))
		{
			randomScaleRange_.max_ = maxS;
			SetRandomScaleRange(randomScaleRange_);
		}
	}
	bool randomCol = isRandomColor_;
	if (ImGui::Checkbox("Random Color", &randomCol))
	{
		SetRandomColor(randomCol);
	}
	if (randomCol)
	{
		Vector4 minC = randomColormin_;
		Vector4 maxC = randomColormax_;
		if (ImGui::ColorEdit4("Random Color Min", &minC.x))
		{
			randomColormin_ = minC;
		}
		if (ImGui::ColorEdit4("Random Color Max", &maxC.x))
		{
			randomColormax_ = maxC;
		}
	}
	bool randomRot = isRandomRotation_;
	if (ImGui::Checkbox("Random Rotation", &randomRot))
	{
		SetRandomRotation(randomRot);
	}
	if (randomRot)
	{
		Vector3 minR = randomRotationRange_.min_;
		Vector3 maxR = randomRotationRange_.max_;
		if (ImGui::InputFloat3("Random Rotation Min", &minR.x))
		{
			randomRotationRange_.min_ = minR;
			SetRandomRotationRange(randomRotationRange_);
		}
		if (ImGui::InputFloat3("Random Rotation Max", &maxR.x))
		{
			randomRotationRange_.max_ = maxR;
			SetRandomRotationRange(randomRotationRange_);
		}
	}
#endif
}

void ParticleEmitter::AddComponent(std::shared_ptr<IParticleComponent> component)
{
	behaviorComponents_.push_back(component);
}

void ParticleEmitter::Play()
{
	isPlaying_ = true;
	if (target_)
	{
		position_ = *target_;
	}
	emitTime_ = 0.0f;
	timeSinceLastEmit_ = emitRate_;
	// 初回の発生を即座に行う
	EmitFirst();
}

void ParticleEmitter::Start(const Vector3& position, uint32_t count, float duration, bool isLoop)
{
	isPlaying_ = true;
	target_ = nullptr;
	position_ = position;
	emitCount_ = count;
	emitTime_ = 0.0f;
	timeSinceLastEmit_ = emitRate_;
	duration_ = duration;
	isLoop_ = isLoop;
	//初回の発生を即座に行う
	EmitFirst();
}

void ParticleEmitter::Start(const Vector3* target, uint32_t count, float duration, bool isLoop)
{
	target_ = target;
	if (target)
	{
		position_ = *target_;
	}
	isPlaying_ = true;
	emitCount_ = count;
	emitTime_ = 0.0f;
	timeSinceLastEmit_ = emitRate_;
	duration_ = duration;
	isLoop_ = isLoop;
	// 初回の発生を即座に行う
	EmitFirst();
}

void ParticleEmitter::StopEmit()
{
	isPlaying_ = false;
	emitTime_ = 0.0f;
	timeSinceLastEmit_ = 0.0f;
}

void ParticleEmitter::SetEmitRange(const Vector3& min, const Vector3& max)
{
	emitRangeMin_ = min;
	emitRangeMax_ = max;
}

void ParticleEmitter::Emit()
{
	if (!isPlaying_) return;

	emitTime_ += 1.0f / 60.0f;
	timeSinceLastEmit_ += 1.0f / 60.0f;

	if (emitTime_ >= duration_)
	{
		if (isLoop_)
		{
			emitTime_ = 0.0f;
		}
		else
		{
			isPlaying_ = false;
			return;
		}
	}

	if (timeSinceLastEmit_ >= emitRate_)
	{
		for (uint32_t i = 0; i < emitCount_; ++i)
		{
			RandomizeInitialParameters();
			Particle newParticle;
			Vector3 randomOffset = MathUtils::RandomVector3(emitRangeMin_, emitRangeMax_);

			newParticle.transform.translate = position_ + randomOffset;
			newParticle.transform.scale = initialScale_;
			newParticle.transform.rotate = initialRotation_;
			newParticle.velocity = initialVelocity_;
			newParticle.color = initialColor_;
			newParticle.lifeTime = initialLifeTime_;
			newParticle.currentTime = 0.0f;

			particleGroup_->AddParticle(newParticle);
		}
		timeSinceLastEmit_ = 0.0f;
	}
}

void ParticleEmitter::EmitFirst()
{
	if (!isPlaying_) return;
	// 初回の発生を即座に行う
	for (uint32_t i = 0; i < emitCount_; ++i)
	{
		RandomizeInitialParameters();
		Particle newParticle;
		Vector3 randomOffset = MathUtils::RandomVector3(emitRangeMin_, emitRangeMax_);
		newParticle.transform.translate = position_ + randomOffset;
		newParticle.transform.scale = initialScale_;
		newParticle.transform.rotate = initialRotation_;
		newParticle.velocity = initialVelocity_;
		newParticle.color = initialColor_;
		newParticle.lifeTime = initialLifeTime_;
		newParticle.currentTime = 0.0f;
		particleGroup_->AddParticle(newParticle);
	}
}

void ParticleEmitter::UpdateEmitPosition()
{
	// 追従対象が設定されている場合、エミッターの位置を更新
	if (target_)
	{
		position_ = *target_;
	}
}

void ParticleEmitter::RandomizeInitialParameters()
{
	if (isRandomVelocity_)
	{
		initialVelocity_ = MathUtils::RandomVector3(randomVelocityRange_.min_, randomVelocityRange_.max_);
	}
	if (isRandomScale_)
	{
		initialScale_ = MathUtils::RandomVector3(randomScaleRange_.min_, randomScaleRange_.max_);
	}
	if (isRandomColor_)
	{
		initialColor_ = MathUtils::RandomVector4(randomColormin_, randomColormax_);
	}
	if (isRandomRotation_)
	{
		initialRotation_ = MathUtils::RandomVector3(randomRotationRange_.min_, randomRotationRange_.max_);
	}
}
