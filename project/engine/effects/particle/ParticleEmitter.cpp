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
#include <time/TimeManager.h>

namespace
{
	// デバッグ球の半径
	constexpr float kDebugSphereRadius = 0.1f;
	// 1フレームの時間（60FPS基準）
	constexpr float kFrameTime = 1.0f / 60.0f;
}


ParticleEmitter::~ParticleEmitter()
{
	// パーティクルグループを解放
	particleGroup_.reset();
	// 振る舞いコンポーネントリストをクリア
	behaviorComponents_.clear();
	// パーティクルマネージャーからエミッターを登録解除
	ParticleManager::GetInstance()->UnregisterEmitter(groupName_);
}

void ParticleEmitter::Initialize(const std::string& groupName, const std::string& textureFilePath)
{
	// グループ名を保存
	groupName_ = groupName;
	// パーティクルグループを生成・初期化
	particleGroup_ = std::make_unique<ParticleGroup>();
	particleGroup_->Initialize(groupName, textureFilePath);
	// パーティクルマネージャーにエミッターを登録
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
	// デバッグモード：発生ポイントを球で描画
	LineManager::GetInstance()->DrawSphere(
		position_,
		kDebugSphereRadius,
		VectorColorCodes::Red
	);
	// デバッグモード：発生範囲をAABBで描画
	LineManager::GetInstance()->DrawAABB(
		AABB(
			position_ + emitRangeMin_,
			position_ + emitRangeMax_),
		VectorColorCodes::Green
	);
#endif

	// パーティクルグループが存在しない場合は描画しない
	if (!particleGroup_) return;
	// パーティクルグループを描画
	particleGroup_->Draw(dxCommon, srvManager);
}

void ParticleEmitter::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("ParticleEmitter Info");

	// --- BlendMode切り替えUI ---
	static const char* blendModeNames[] = {
		"Alpha",
		"Additive",
		"Subtractive",
		"Multiply",
		"Screen",
		"Darken",
		"Lighten",
		"ColorBurn",
		"ColorDodge"
	};
	int blendModeIdx = static_cast<int>(blendMode_);
	if (ImGui::Combo("Blend Mode", &blendModeIdx, blendModeNames, IM_ARRAYSIZE(blendModeNames)))
	{
		blendMode_ = static_cast<BlendMode>(blendModeIdx);
	}

	// --- 現在の生成パーティクル数を表示 ---
	if (particleGroup_)
	{
		size_t particleCount = particleGroup_->GetParticles().size();
		ImGui::Text("Current Particle Count: %zu", particleCount);
	}

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

	// ビルボード
	bool isBillboard = particleGroup_->IsBillboard();
	if (ImGui::Checkbox("Billboard", &isBillboard))
	{
		particleGroup_->SetBillboard(isBillboard);
	}

	// 初期値
	float life = initialLifeTime_;
	if (ImGui::DragFloat("Initial LifeTime", &life, 0.01f, 0.0f, 100.0f))
	{
		SetInitialLifeTime(life);
	}
	Vector3 vel = initialVelocity_;
	if (ImGui::DragFloat3("Initial Velocity", &vel.x))
	{
		SetInitialVelocity(vel);
	}
	Vector4 col = initialColor_;
	if (ImGui::ColorEdit4("Initial Color", &col.x))
	{
		SetInitialColor(col);
	}
	Vector3 scale = initialScale_;
	if (ImGui::DragFloat3("Initial Scale", &scale.x))
	{
		SetInitialScale(scale);
	}
	Vector3 rot = initialRotation_;
	if (ImGui::DragFloat3("Initial Rotation", &rot.x))
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
		if (ImGui::DragFloat3("Random Velocity Min", &minV.x))
		{
			randomVelocityRange_.min_ = minV;
			SetRandomVelocityRange(randomVelocityRange_);
		}
		if (ImGui::DragFloat3("Random Velocity Max", &maxV.x))
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
		if (ImGui::DragFloat3("Random Scale Min", &minS.x))
		{
			randomScaleRange_.min_ = minS;
			SetRandomScaleRange(randomScaleRange_);
		}
		if (ImGui::DragFloat3("Random Scale Max", &maxS.x))
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
		if (ImGui::DragFloat3("Random Rotation Min", &minR.x))
		{
			randomRotationRange_.min_ = minR;
			SetRandomRotationRange(randomRotationRange_);
		}
		if (ImGui::DragFloat3("Random Rotation Max", &maxR.x))
		{
			randomRotationRange_.max_ = maxR;
			SetRandomRotationRange(randomRotationRange_);
		}
	}
#endif
}

void ParticleEmitter::AddComponent(std::shared_ptr<IParticleComponent> component)
{
	// コンポーネントをリストに追加
	behaviorComponents_.push_back(component);
}

void ParticleEmitter::Play()
{
	// 再生開始
	isPlaying_ = true;
	// 追従対象がある場合は位置を同期
	if (target_)
	{
		position_ = *target_;
	}
	// 発生時間をリセット
	emitTime_ = 0.0f;
	// 即座に発生できるようにタイマーをセット
	timeSinceLastEmit_ = emitRate_;
	// 初回の発生を即座に行う
	EmitFirst();
}

void ParticleEmitter::Start(const Vector3& position, uint32_t count, float duration, bool isLoop)
{
	// 再生開始
	isPlaying_ = true;
	// 追従対象をクリア
	target_ = nullptr;
	// 発生位置を設定
	position_ = position;
	// 発生数を設定
	emitCount_ = count;
	// 発生時間をリセット
	emitTime_ = 0.0f;
	// 即座に発生できるようにタイマーをセット
	timeSinceLastEmit_ = emitRate_;
	// 継続時間を設定
	duration_ = duration;
	// ループフラグを設定
	isLoop_ = isLoop;
	// 初回の発生を即座に行う
	EmitFirst();
}

void ParticleEmitter::Start(const Vector3* target, uint32_t count, float duration, bool isLoop)
{
	// 追従対象を設定
	target_ = target;
	// 追従対象がある場合は位置を同期
	if (target)
	{
		position_ = *target_;
	}
	// 再生開始
	isPlaying_ = true;
	// 発生数を設定
	emitCount_ = count;
	// 発生時間をリセット
	emitTime_ = 0.0f;
	// 即座に発生できるようにタイマーをセット
	timeSinceLastEmit_ = emitRate_;
	// 継続時間を設定
	duration_ = duration;
	// ループフラグを設定
	isLoop_ = isLoop;
	// 初回の発生を即座に行う
	EmitFirst();
}

void ParticleEmitter::StopEmit()
{
	// 再生停止
	isPlaying_ = false;
	// 発生時間をリセット
	emitTime_ = 0.0f;
	// 最後の発生からの経過時間をリセット
	timeSinceLastEmit_ = 0.0f;
}

void ParticleEmitter::SetEmitRange(const Vector3& min, const Vector3& max)
{
	// 発生範囲の最小値を設定
	emitRangeMin_ = min;
	// 発生範囲の最大値を設定
	emitRangeMax_ = max;
}

void ParticleEmitter::Emit()
{
	// 再生中でなければ何もしない
	if (!isPlaying_) return;

	// 発生経過時間を更新
	emitTime_ += TimeManager::GetInstance().GetGameContext().deltaTime;
	// 固定フレームタイムで発生タイミングを計算
	timeSinceLastEmit_ += kFrameTime;

	// 継続時間を超えた場合の処理
	if (emitTime_ >= duration_)
	{
		if (isLoop_)
		{
			// ループする場合は発生時間をリセット
			emitTime_ = 0.0f;
		}
		else
		{
			// ループしない場合は再生停止
			isPlaying_ = false;
			return;
		}
	}

	// 発生レートに達した場合、パーティクルを生成
	if (timeSinceLastEmit_ >= emitRate_)
	{
		for (uint32_t i = 0; i < emitCount_; ++i)
		{
			// 初期パラメータをランダム化
			RandomizeInitialParameters();
			// 新しいパーティクルを作成
			Particle newParticle;
			// 発生範囲内のランダム位置を計算
			Vector3 randomOffset = MathUtils::RandomVector3(emitRangeMin_, emitRangeMax_);

			// パーティクルの初期値を設定
			newParticle.transform.translate = position_ + randomOffset;
			newParticle.transform.scale = initialScale_;
			newParticle.transform.rotate = initialRotation_;
			newParticle.velocity = initialVelocity_;
			newParticle.color = initialColor_;
			newParticle.lifeTime = initialLifeTime_;
			newParticle.currentTime = 0.0f;

			// パーティクルグループに追加
			particleGroup_->AddParticle(newParticle);
		}
		// 最後の発生からの経過時間をリセット
		timeSinceLastEmit_ = 0.0f;
	}
}

void ParticleEmitter::EmitFirst()
{
	// 再生中でなければ何もしない
	if (!isPlaying_) return;
	// 初回の発生を即座に行う
	for (uint32_t i = 0; i < emitCount_; ++i)
	{
		// 初期パラメータをランダム化
		RandomizeInitialParameters();
		// 新しいパーティクルを作成
		Particle newParticle;
		// 発生範囲内のランダム位置を計算
		Vector3 randomOffset = MathUtils::RandomVector3(emitRangeMin_, emitRangeMax_);
		// パーティクルの初期値を設定
		newParticle.transform.translate = position_ + randomOffset;
		newParticle.transform.scale = initialScale_;
		newParticle.transform.rotate = initialRotation_;
		newParticle.velocity = initialVelocity_;
		newParticle.color = initialColor_;
		newParticle.lifeTime = initialLifeTime_;
		newParticle.currentTime = 0.0f;
		// パーティクルグループに追加
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
	// 速度のランダム化が有効な場合
	if (isRandomVelocity_)
	{
		initialVelocity_ = MathUtils::RandomVector3(randomVelocityRange_.min_, randomVelocityRange_.max_);
	}
	// スケールのランダム化が有効な場合
	if (isRandomScale_)
	{
		initialScale_ = MathUtils::RandomVector3(randomScaleRange_.min_, randomScaleRange_.max_);
	}
	// カラーのランダム化が有効な場合
	if (isRandomColor_)
	{
		initialColor_ = MathUtils::RandomVector4(randomColormin_, randomColormax_);
	}
	// 回転のランダム化が有効な場合
	if (isRandomRotation_)
	{
		initialRotation_ = MathUtils::RandomVector3(randomRotationRange_.min_, randomRotationRange_.max_);
	}
}
