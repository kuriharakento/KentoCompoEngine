#pragma once
#include "engine/ecs/system/ISystem.h"
#include "math/Vector3.h"

class Object3dCommon;
class SpriteCommon;
class CameraManager;
class LightManager;
class ShadowMapManager;

/**
 * @brief エネミーのスポーンを管理するシステム。
 * 
 * - 指定された時間間隔や、プレイヤーの周囲にエネミーを生成する。
 * - 生成されたエネミーは StatusComponent, EnemyAIComponent を持つ。
 */
class EnemySpawnSystem : public ISystem
{
public:
    void Initialize(
        Object3dCommon* object3dCommon,
        LightManager* lightManager,
        CameraManager* cameraManager);

    void Update(Registry& registry) override;

    // スポーン設定
    void SetSpawnRadius(float inner, float outer) { innerRadius_ = inner; outerRadius_ = outer; }
    void SetInitialSpawnRate(float rate) { spawnRate_ = rate; }

private:
    void SpawnEnemy(Registry& registry);

    Object3dCommon* object3dCommon_ = nullptr;
    LightManager* lightManager_ = nullptr;
    CameraManager* cameraManager_ = nullptr;

    float spawnTimer_ = 0.0f;
    float spawnRate_ = 1.0f; // 1秒間に何体
    float innerRadius_ = 20.0f;
    float outerRadius_ = 40.0f;

    float elapsedTime_ = 0.0f;
};
