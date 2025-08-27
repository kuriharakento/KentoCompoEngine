#pragma once
#include "application/stage/StageManager.h"
#include "graphics/2d/Sprite.h"

class Minimap
{
public:
	void Initialize(SpriteCommon* spriteCommon, StageManager* stageManager);
	void Update();
	void Draw();

private:
	Vector2 WorldToMinimap(const Vector3& worldPos)const;

private:
	SpriteCommon* spriteCommon_ = nullptr;
	StageManager* stageManager_ = nullptr;

	std::unique_ptr<Sprite> frame_;
	std::vector<std::unique_ptr<Sprite>> enemyIcons_;
	std::unique_ptr< Sprite> playerIcon_;
	std::vector<std::unique_ptr<Sprite>> areaIcon_;
	std::vector<bool> areaActiveFlags_;
	float mapWidth_ = 200.0f; // マップの幅（ワールド座標系）
	float mapHeight_ = 200.0f; // マップの高さ（ワールド座標系）
};

