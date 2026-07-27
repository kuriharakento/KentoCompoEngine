#pragma once

namespace KCE
{
class Skybox;
class PostProcessManager;
class PostProcessPass;
class LineManager;
class LightManager;
class CameraManager;
class SpriteCommon;
class Object3dCommon;
class ShadowMapManager;

/**
 * @brief 各シーンで共有するコンテキスト情報。
 * 
 * すべてのメンバポインタは非所有参照（borrowed）。
 * 所有権は Framework 等の Engine 側にあり、SceneManager の生存期間中有効。
 */
struct SceneContext
{
    SpriteCommon* spriteCommon = nullptr;
    Object3dCommon* object3dCommon = nullptr;
	CameraManager* cameraManager = nullptr;
	LightManager* lightManager = nullptr;
	PostProcessManager* postProcessManager = nullptr;
	Skybox* skybox = nullptr;
	ShadowMapManager* shadowMapManager = nullptr;
};
} // namespace KCE
