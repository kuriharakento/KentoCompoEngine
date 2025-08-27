#include "Area.h"

#include "application/GameObject/Combatable/character/base/Character.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"
#include "base/Logger.h"

Area::Area(Object3dCommon* objCommon, LightManager* lightManager, EnemyManager* enemyManager,
           const std::vector<Wave>& waves) : waveManager_(enemyManager, waves)
{
	// エリアの判定用ゲームオブジェクトを作成
	areaObject_ = std::make_unique<GameObject>("Area");
	areaObject_->Initialize(objCommon, lightManager);
	// OBBコライダーを追加
	auto collider = std::make_unique<OBBColliderComponent>(areaObject_.get());
	collider->SetOnEnter([this](GameObject* other) {
		// エリアに入った時の処理
		if (!isStarted_ && other->GetTag() == GameObjectTag::Character::Player)
		{
			Start(); // エリアに入ったらスタート
		}
						 });
	collider->SetOnStay([this](GameObject* other) {
		// エリアに留まっている間の処理
						});
	collider->SetOnExit([this](GameObject* other) {
		// エリアから出た時の処理
		// キャラクターはエリアから出さない
		auto character = dynamic_cast<Character*>(other);
		if (character)
		{
			
		}

						});
	areaObject_->AddComponent("OBBCollider", std::move(collider));

	// 初期状態を設定
	isStarted_ = false;
	isCleared_ = false;
	isActive_ = false;
}

void Area::Start()
{
	// すでにスタートしている場合は何もしない
	if (isStarted_) { return; }
	isStarted_ = true;
	isCleared_ = false;
	waveManager_.SetOnAllWavesCleared([this]() {
		isCleared_ = true;
		if (onClearCallback_)
		{
			onClearCallback_();
		}
									  });
	waveManager_.Start();
}

void Area::Update(CameraManager* camera)
{
	if (isStarted_ && !isCleared_)
	{
		waveManager_.Update();
	}
	if (isActive_)
	{
		// エリアオブジェクトの更新
		areaObject_->Update();
		areaObject_->UpdateTransform(camera);
	}
}
