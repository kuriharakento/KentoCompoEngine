#include "Minimap.h"
#include "application/ecs/components/TransformComponent.h"

void Minimap::Initialize(SpriteCommon* spriteCommon, StageManager* stageManager)
{
	spriteCommon_ = spriteCommon;
	stageManager_ = stageManager;
	
	frame_ = std::make_unique<Sprite>();
	frame_->Initialize(spriteCommon_, "./Resources/minimap_frame.png");
	frame_->SetPosition(Vector2(1080.0f, 90.0f));
	frame_->SetSize({ 300.0f, 300.0f });
	frame_->SetAnchorPoint({ 0.5f, 0.5f });

	enemyIcons_.clear();

	areaIcon_.clear();

	playerIcon_ = std::make_unique<Sprite>();
	playerIcon_->Initialize(spriteCommon_, "./Resources/red.png");
	playerIcon_->SetSize({ 18.0f, 18.0f });
	playerIcon_->SetAnchorPoint({ 0.5f, 0.5f });
	playerIcon_->SetColor(VectorColorCodes::Cyan);
}

void Minimap::Update()
{
#ifdef USE_IMGUI
	ImGui::Begin("Minimap");

	static Vector2 framePos = frame_->GetPosition();
	if (ImGui::DragFloat2("Frame Position", &framePos.x, 1.0f, 0.0f, 1920.0f))
	{
		frame_->SetPosition(framePos);
	}
	static Vector2 frameSize = frame_->GetSize();
	if (ImGui::DragFloat2("Frame Size", &frameSize.x, 1.0f, 50.0f, 400.0f))
	{
		frame_->SetSize(frameSize);
	}

	ImGui::End();

#endif

	frame_->Update();

	float playerYaw = stageManager_->GetPlayer()->GetRotation().y;
	playerIcon_->SetPosition(frame_->GetPosition());
	playerIcon_->SetRotation(playerYaw);
	playerIcon_->Update();

	// 敵アイコンの動的管理 (ECSからTransformComponentを取得して位置を同期)
	auto enemyManager = stageManager_->GetEnemyManager();
	auto registry = enemyManager->GetRegistry();

	// ComponentArrayからTransformを取得 (Viewはshared_ptrを返す)
	auto transformArray = registry ? registry->View<TransformComponent>() : nullptr;
	uint32_t activeEnemyCount = registry ? registry->GetActiveEntityCount() : 0;

	// 敵の数に合わせてアイコンを動的に追加/削除
	if (enemyIcons_.size() < activeEnemyCount)
	{
		for (size_t i = enemyIcons_.size(); i < activeEnemyCount; ++i)
		{
			auto icon = std::make_unique<Sprite>();
			icon->Initialize(spriteCommon_, "./Resources/red.png");
			icon->SetSize({ 10.0f, 10.0f });
			icon->SetAnchorPoint({ 0.5f, 0.5f });
			enemyIcons_.push_back(std::move(icon));
		}
	}
	else if (enemyIcons_.size() > activeEnemyCount)
	{
		enemyIcons_.resize(activeEnemyCount);
	}

	if (transformArray)
	{
		size_t i = 0;
		for (const auto& transform : *transformArray)
		{
			// UIの上限または登録されているアイコン数まで描画
			if (i >= enemyIcons_.size()) break;

			// ECSのTransformから位置情報を取得
			Vector3 enemyPos = { transform.localPosition.x, transform.localPosition.y, transform.localPosition.z };
			// TODO: 回転成分の実用化 (QuaternionからEulerへの変換など)
			float enemyYaw = 0.0f; 

			Vector2 miniMapPos = WorldToMinimap(enemyPos);
			enemyIcons_[i]->SetPosition(miniMapPos);
			enemyIcons_[i]->SetRotation(enemyYaw);
			enemyIcons_[i]->Update();

			++i;
		}
	}

	// エリアアイコンの動的管理
	auto areaManager = stageManager_->GetStage()->GetAreaManager();
	const auto& areas = areaManager->GetAreas();
	
	if (areaIcon_.size() < areas.size())
	{
		for (size_t i = areaIcon_.size(); i < areas.size(); ++i)
		{
			auto icon = std::make_unique<Sprite>();
			icon->Initialize(spriteCommon_, "./Resources/black.png");
			icon->SetSize({ 20.0f, 20.0f });
			icon->SetAnchorPoint({ 0.5f, 0.5f });
			areaIcon_.push_back(std::move(icon));
			areaActiveFlags_.push_back(false);
		}
	}
	else if (areaIcon_.size() > areas.size())
	{
		areaIcon_.resize(areas.size());
	}

	for (size_t i = 0; i < areas.size(); ++i)
	{
		Vector3 areaPos = areas[i]->GetAreaObject()->GetPosition();
		float areaYaw = areas[i]->GetAreaObject()->GetRotation().y;
		Vector2 miniMapPos = WorldToMinimap(areaPos);
		areaIcon_[i]->SetPosition(miniMapPos);
		areaIcon_[i]->SetRotation(areaYaw);
		areaIcon_[i]->Update();
		areaActiveFlags_[i] = areas[i]->IsActive();
	}
}

void Minimap::Draw()
{
	frame_->Draw();

	playerIcon_->Draw();

	for (auto& icon : enemyIcons_)
	{
		icon->Draw();
	}

	for (int i = 0; i < areaIcon_.size(); ++i)
	{
		if (!areaActiveFlags_[i]) { continue; }
		areaIcon_[i]->Draw();
	}
}

Vector2 Minimap::WorldToMinimap(const Vector3& worldPos) const
{
	Vector3 playerPos = stageManager_->GetPlayer()->GetPosition();

	Vector2 frameCenter = frame_->GetPosition();
	Vector2 frameSize = frame_->GetSize();
	float halfWidth = mapWidth_ * 0.5f;
	float halfHeight = mapHeight_ * 0.5f;

	// プレイヤーからの相対座標を計算し、ミニマップスケールに変換
	// X軸はそのまま、Z軸は反転（上が北になるように）
	float nx = ((worldPos.x - playerPos.x) / halfWidth) * (frameSize.x * 0.5f);
	float ny = -((worldPos.z - playerPos.z) / halfHeight) * (frameSize.y * 0.5f);

	float x = frameCenter.x + nx;
	float y = frameCenter.y + ny;

	frameSize.x /= 2.0f;
	frameSize.y /= 2.0f;

	// ミニマップの円形範囲外の場合は円周上にクランプ
	float iconHalfSize = 5.0f;
	float radius = (frameSize.x < frameSize.y ? frameSize.x : frameSize.y) * 0.5f - iconHalfSize;

	float dx = x - frameCenter.x;
	float dy = y - frameCenter.y;
	float dist = std::sqrt(dx * dx + dy * dy);

	if (dist > radius)
	{
		float angle = std::atan2(dy, dx);
		x = frameCenter.x + radius * std::cos(angle);
		y = frameCenter.y + radius * std::sin(angle);
	}

	return Vector2(x, y);
}