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
    
    // スポーン可視化 (デバッグ描画)
    void Draw(Registry& registry, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager) override;

    // 半径アクセサ (ImGui用)
    float GetInnerRadius() const { return innerRadius_; }
    void SetInnerRadius(float r) { innerRadius_ = r; }
    float GetOuterRadius() const { return outerRadius_; }
    void SetOuterRadius(float r) { outerRadius_ = r; }

private:
    void SpawnEnemy(Registry& registry);
    uint32_t GetMaxEnemies(uint32_t level);

    Object3dCommon* object3dCommon_ = nullptr;
    LightManager* lightManager_ = nullptr;
    CameraManager* cameraManager_ = nullptr;

    float tickTimer_ = 0.0f;  // 通常スポーン (1s毎)
    float burstTimer_ = 0.0f; // バースト増援 (10s毎)

    // 設定定数 (レベル別上限)
    const uint32_t kMaxEnemiesLv1 = 6;
    const uint32_t kMaxEnemiesLv2 = 20;
    const uint32_t kMaxEnemiesLv3 = 40;
    const uint32_t kMaxEnemiesLv4 = 60;
    const uint32_t kMaxEnemiesLv5 = 500;

    // 基本設定
    float spawnRate_ = 1.0f; 
    float innerRadius_ = 60.0f;
    float outerRadius_ = 80.0f;

    float elapsedTime_ = 0.0f;
};
