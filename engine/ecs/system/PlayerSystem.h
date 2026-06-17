#pragma once

#include "ISystem.h"

/**
 * @brief プレイヤーの入力と移動・ステータスを制御するシステム。
 */
class CameraManager;

/**
 * @brief プレイヤーの入力と移動・ステータスを制御するシステム。
 */
class PlayerSystem : public ISystem
{
public:
    void Update(Registry& registry) override;

    /**
     * @brief カメラマネージャーを設定する
     * @param cameraManager カメラマネージャーへのポインタ
     */
    void SetCameraManager(CameraManager* cameraManager) { cameraManager_ = cameraManager; }

private:
    CameraManager* cameraManager_ = nullptr;
};
