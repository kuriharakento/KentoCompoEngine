#include "Minimap.h"

void Minimap::Initialize(SpriteCommon* spriteCommon, StageManager* stageManager)
{
	spriteCommon_ = spriteCommon;
	stageManager_ = stageManager;
	// フレームのスプライトを作成
	frame_ = std::make_unique<Sprite>();
	frame_->Initialize(spriteCommon_, "./Resources/minimap_frame.png");
	frame_->SetPosition(Vector2(1080.0f, 90.0f));	// 右上に配置
	frame_->SetSize({ 300.0f, 300.0f });
	frame_->SetAnchorPoint({ 0.5f, 0.5f });

	// 敵のスプライトを作成
	enemyIcons_.clear();

	// エリアアイコンのスプライトを作成
	areaIcon_.clear();

	// プレイヤーアイコンを作成
	playerIcon_ = std::make_unique<Sprite>();
	playerIcon_->Initialize(spriteCommon_, "./Resources/red.png");
	playerIcon_->SetSize({ 18.0f, 18.0f });
	playerIcon_->SetAnchorPoint({ 0.5f, 0.5f });
	playerIcon_->SetColor(VectorColorCodes::Cyan);
}

void Minimap::Update()
{
#ifdef _DEBUG
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

	// プレイヤーの回転角度を取得
	float playerYaw = stageManager_->GetPlayer()->GetRotation().y; // Yaw（Y軸回転）

	// ミニマップ上のプレイヤーアイコンに反映
	playerIcon_->SetPosition(frame_->GetPosition());
	playerIcon_->SetRotation(playerYaw); // ミニマップ上で回転方向が逆ならマイナスを付ける
	playerIcon_->Update();

	// 敵の取得
	auto enemyManager = stageManager_->GetEnemyManager();
	const auto& enemies = enemyManager->GetEnemies();

	// 敵の数に合わせてアイコンスプライト配列を調整
	if (enemyIcons_.size() < enemies.size())
	{
		for (size_t i = enemyIcons_.size(); i < enemies.size(); ++i)
		{
			auto icon = std::make_unique<Sprite>();
			icon->Initialize(spriteCommon_, "./Resources/red.png");
			icon->SetSize({ 10.0f, 10.0f });
			icon->SetAnchorPoint({ 0.5f, 0.5f });
			enemyIcons_.push_back(std::move(icon));
		}
	}
	else if (enemyIcons_.size() > enemies.size())
	{
		// 余分なアイコンを削除
		enemyIcons_.resize(enemies.size());
	}

	// 各敵座標をミニマップ座標へ
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		Vector3 enemyPos = enemies[i]->GetPosition();
		float enemyYaw = enemies[i]->GetRotation().y;
		Vector2 miniMapPos = WorldToMinimap(enemyPos);
		enemyIcons_[i]->SetPosition(miniMapPos);
		enemyIcons_[i]->SetRotation(enemyYaw);
		enemyIcons_[i]->Update();
	}

	// エリアの取得
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
		// 余分なアイコンを削除
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

	// 敵の数だけ描画
	for (auto& icon : enemyIcons_)
	{
		icon->Draw();
	}

	// アクティブなエリアのみ描画
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

	// プレイヤー中心の相対座標をミニマップ上に変換
	float nx = ((worldPos.x - playerPos.x) / halfWidth) * (frameSize.x * 0.5f);
	float ny = -((worldPos.z - playerPos.z) / halfHeight) * (frameSize.y * 0.5f);

	// 表示位置（まだclampしていない）
	float x = frameCenter.x + nx;
	float y = frameCenter.y + ny;

	frameSize.x /= 2.0f; // 半分のサイズにする
	frameSize.y /= 2.0f;

	// 半径（アイコンサイズ分だけ内側にする場合は調整）
	float iconHalfSize = 5.0f; // アイコンサイズが10x10の場合
	float radius = (frameSize.x < frameSize.y ? frameSize.x : frameSize.y) * 0.5f - iconHalfSize;

	float dx = x - frameCenter.x;
	float dy = y - frameCenter.y;
	float dist = std::sqrt(dx * dx + dy * dy);

	if (dist > radius)
	{
		// 枠外なら円周上に
		float angle = std::atan2(dy, dx);
		x = frameCenter.x + radius * std::cos(angle);
		y = frameCenter.y + radius * std::sin(angle);
	}

	return Vector2(x, y);
}