#pragma once

#include "base/GraphicsTypes.h"
#include "light/DirectionalLight.h"

namespace KCE
{
class Object3dCommon;
class Camera;
class LightManager;

/**
 * @brief レンダリングタイプ
 */
enum class RenderingType {
	Deferred, // ディファードレンダリング（G-Bufferパス）
	Forward,  // フォワードレンダリング（Forwardパス）
};

/**
 * @brief 3D描画オブジェクトの共通インターフェース
 * @details Object3dとSkinnedObject3dの共通操作を定義する。
 *          GameObjectはこのインターフェース経由で描画オブジェクトを操作する。
 */
class IRenderable3d
{
public:
	virtual ~IRenderable3d() = default;

	// ======================================
	// ライフサイクル
	// ======================================

	/**
	 * @brief 初期化
	 * @param object3dCommon Object3dCommonへのポインタ
	 * @param camera カメラへのポインタ
	 */
	virtual void Initialize(Object3dCommon* object3dCommon, Camera* camera) = 0;

	/**
	 * @brief 更新
	 * @param deltaTime フレーム時間（秒）
	 * @param camera カメラへのポインタ
	 */
	virtual void Update(float deltaTime, Camera* camera) = 0;

	// ======================================
	// 描画
	// ======================================

	/**
	 * @brief 描画
	 */
	virtual void Draw() = 0;

	/**
	 * @brief シャドウマップ用描画
	 */
	virtual void DrawShadowOnly() = 0;

	/**
	 * @brief G-Buffer用描画
	 */
	virtual void DrawGBuffer() = 0;

	// ======================================
	// Transform操作
	// ======================================

	/**
	 * @brief スケールの設定
	 */
	virtual void SetScale(const Vector3& scale) = 0;

	/**
	 * @brief 回転の設定
	 */
	virtual void SetRotate(const Vector3& rotate) = 0;

	/**
	 * @brief 位置の設定
	 */
	virtual void SetTranslate(const Vector3& translate) = 0;

	/**
	 * @brief スケールの取得
	 */
	virtual const Vector3& GetScale() const = 0;

	/**
	 * @brief 回転の取得
	 */
	virtual const Vector3& GetRotate() const = 0;

	/**
	 * @brief 位置の取得
	 */
	virtual const Vector3& GetTranslate() const = 0;

	/**
	 * @brief ワールド行列の取得
	 */
	virtual Matrix4x4 GetWorldMatrix() const = 0;

	/**
	 * @brief ワールド行列の更新
	 */
	virtual void UpdateWorldMatrix() = 0;

	/**
	 * @brief 外部ワールド行列を使用した行列の更新
	 */
	virtual void UpdateMatrixWithWorld(const Matrix4x4& worldMatrix, Camera* camera) = 0;

	// ======================================
	// ライト/カメラ設定
	// ======================================

	/**
	 * @brief カメラの設定
	 */
	virtual void SetCamera(Camera* camera) = 0;

	/**
	 * @brief ライトマネージャーの設定
	 */
	virtual void SetLightManager(LightManager* lightManager) = 0;

	/**
	 * @brief ディレクショナルライトの設定
	 */
	virtual void SetDirectionalLight(const DirectionalLight& light) = 0;

	// ======================================
	// マテリアル操作
	// ======================================

	/**
	 * @brief 色の設定
	 */
	virtual void SetColor(const Vector4& color) = 0;

	/**
	 * @brief 色の取得
	 */
	virtual Vector4 GetColor() const = 0;

	/**
	 * @brief ライティングの有効/無効の設定
	 */
	virtual void SetEnableLighting(bool enable) = 0;

	/**
	 * @brief ライティングの有効/無効の取得
	 */
	virtual bool IsEnableLighting() const = 0;
	virtual void SetEmissiveSettings(const EmissiveSettings& settings) = 0;
	virtual EmissiveSettings GetEmissiveSettings() const = 0;

	// ======================================
	// レンダリングタイプ操作
	// ======================================

	/**
	 * @brief レンダリングタイプの取得
	 */
	virtual RenderingType GetRenderingType() const = 0;

	/**
	 * @brief レンダリングタイプの設定
	 */
	virtual void SetRenderingType(RenderingType type) = 0;
};
} // namespace KCE
