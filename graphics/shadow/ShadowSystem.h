#pragma once
#include <vector>
#include <string>

#include "manager/graphics/ShadowMapManager.h"
#include "graphics/shadow/ShadowMapPipeline.h"
#include "manager/scene/LightManager.h"
#include "manager/system/SrvManager.h"
#include "graphics/3d/Object3d.h"

namespace KCE
{
/**
 * @brief シャドウシステムクラス
 * @details ゲームシーンでのシャドウマップ描画を簡略化するヘルパークラス
 *          DirectionalLight、SpotLight、PointLightのシャドウを統合管理
 */
class ShadowSystem
{
public:
	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SrvManagerへのポインタ
	 * @param lightManager LightManagerへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, LightManager* lightManager);

	/**
	 * @brief ディレクショナルライトのシャドウマップを作成
	 * @param resolution シャドウマップ解像度（デフォルト2048）
	 */
	void CreateDirectionalLightShadow(uint32_t resolution = 2048);

	/**
	 * @brief スポットライトのシャドウマップを作成
	 * @param name ライトの名前
	 * @param resolution シャドウマップ解像度（デフォルト1024）
	 */
	void CreateSpotLightShadow(const std::string& name, uint32_t resolution = 1024);

	/**
	 * @brief ポイントライトのシャドウマップを作成
	 * @param name ライトの名前
	 * @param resolution シャドウマップ解像度（デフォルト512）
	 */
	void CreatePointLightShadow(const std::string& name, uint32_t resolution = 512);

	/**
	 * @brief シャドウマップの更新（毎フレーム呼び出し）
	 * @param targetCenter ディレクショナルライトシャドウの中心位置
	 * @param shadowMapSize ディレクショナルライトシャドウのカバー範囲
	 */
	void Update(const Vector3& targetCenter, float shadowMapSize = 50.0f);

	/**
	 * @brief シャドウパスの描画
	 * @param objects 描画するオブジェクトの配列
	 */
	void RenderShadowPass(const std::vector<Object3d*>& objects);

	/**
	 * @brief オブジェクトにシャドウマップを適用
	 * @param object 対象のオブジェクト
	 */
	void ApplyShadowToObject(Object3d* object);

	/**
	 * @brief 終了処理
	 */
	void Finalize();

public: // ゲッター
	ShadowMapManager& GetShadowMapManager() { return shadowMapManager_; }
	ShadowMapPipeline& GetShadowMapPipeline() { return shadowMapPipeline_; }
	bool IsInitialized() const { return isInitialized_; }

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	LightManager* lightManager_ = nullptr;

	ShadowMapManager shadowMapManager_;
	ShadowMapPipeline shadowMapPipeline_;

	bool isInitialized_ = false;
};
} // namespace KCE
