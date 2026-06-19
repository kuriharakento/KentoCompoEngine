#pragma once

class Registry;
class Camera;
class LightManager;
class ShadowMapManager;

/**
 * @brief すべてのECS Systemの基底インターフェース
 * 
 * SystemManagerによって一括管理・実行されるシステムはこのクラスを継承する。
 */
class ISystem
{
public:
	virtual ~ISystem() = default;

	/**
	 * @brief システムの初期化処理
	 */
	virtual void Initialize() {}

	/**
	 * @brief 毎フレームの更新処理
	 * @param registry ECSのRegistryへの参照
	 */
	virtual void Update(Registry& registry) {}

	/**
	 * @brief 描画処理 (3D/2D)
	 * @param registry ECSのRegistryへの参照
	 * @param camera 現在のカメラ
	 * @param lightManager ライトマネージャ
	 * @param shadowMapManager シャドウマップマネージャ
	 */
	virtual void Draw(Registry& registry, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager) {}

	/**
	 * @brief シャドウマップ作成用の描画処理
	 * @param registry ECSのRegistryへの参照
	 */
	virtual void DrawShadow(Registry& registry) {}

	/**
	 * @brief システムの終了処理
	 */
	virtual void Finalize() {}
};
