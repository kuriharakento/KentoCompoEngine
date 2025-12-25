#include "EnemyBase.h"

#include "time/TimerManager.h"
#include <application/GameObject/component/action/EnemyUIComponent.h>
#include <sstream>
#include "engine/math/VectorColorCodes.h"

// 定数定義
const float kHitFlashDuration = 0.05f; // 被弾時の赤色表示時間

// タイマー名を生成するヘルパー
static std::string GenerateHitTimerName(const EnemyBase* enemy)
{
	std::stringstream ss;
	ss << "Enemy_HitFlash_" << enemy;
	return ss.str();
}

EnemyBase::~EnemyBase()
{
	// 自身に関連するタイマーを削除（安全のため）
	TimerManager::GetInstance().RemoveTimer(GenerateHitTimerName(this));
}

void EnemyBase::Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target, const Transform& initialTransform)
{
	Character::Initialize(object3dCommon, lightManager, initialTransform);

	// UIコンポーネント
	auto uiComp = std::make_unique<EnemyUIComponent>(spriteCommon, camera);
	AddComponent("EnemyUIComponent", std::move(uiComp));

	target_ = target; // ターゲットを設定
}

void EnemyBase::Update()
{
	Character::Update();
}

void EnemyBase::TakeDamage(float damage)
{
	// 基底クラスの処理（HP減少など）
	CombatableObject::TakeDamage(damage);

	// ヒット時のフラッシュ処理（TimerManagerを使用）
	std::string timerName = GenerateHitTimerName(this);
	Timer* timer = TimerManager::GetInstance().GetTimer(timerName);

	if (timer)
	{
		// 既にタイマーがある場合はリセットして延長
		timer->Reset();
		timer->Start();
	}
	else
	{
		// 新しいタイマーを作成
		auto newTimer = std::make_unique<Timer>(timerName, kHitFlashDuration);

		// 色変更ヘルパーラムダ
		auto setColorFunc = [this](const Vector4& color) {
			if (object3d_) {
				object3d_->SetColor(color);
			}
			// 子オブジェクトの色も変更
			for (auto& [name, child] : GetChildren()) {
				if (child && child->GetObject3d()) {
					child->GetObject3d()->SetColor(color);
				}
			}
		};
		
		// 開始時に赤くする
		newTimer->SetOnStart([setColorFunc]() {
			setColorFunc(VectorColorCodes::Red);
		});

		// 終了時に白に戻す
		// SetOnTickは不要なので省略（あるいは空設定）

		newTimer->SetOnFinish([setColorFunc]() {
			setColorFunc(VectorColorCodes::White);
		});

		TimerManager::GetInstance().AddTimer(std::move(newTimer));
	}
}