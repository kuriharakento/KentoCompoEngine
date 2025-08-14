#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>

// math
#include "base/GraphicsTypes.h"
// graphics
#include "Model.h"
#include "light/DirectionalLight.h"
#include "manager/graphics/ModelManager.h"
// camera
#include "manager/scene/CameraManager.h"

class LightManager;
//スプライト共通部分のポインタ
class Object3dCommon;

class Object3d
{
public:	/*========[ メンバ関数 ]========*/
	~Object3d();
	/**
	 * \brief 初期化
	 */
	void Initialize(Object3dCommon* object3dCommon,Camera* camera = nullptr);

	/**
	 * \brief 更新
	 */
	void Update(CameraManager* camera = nullptr);

	/**
	 * \brief 描画
	 * \param commandList 
	 */
	void Draw();

	/**
	 * \brief 行列の更新
	 */
	void UpdateMatrix(Camera* camera = nullptr);

	/**
	 * \brief 行列の更新
	 * \param worldMatrix 
	 * \param camera 
	 */
	void UpdateMatrixWithWorld(const Matrix4x4& worldMatrix, Camera* camera = nullptr);

public: /*========[ ゲッター ]========*/
	//Transform
	const Vector3& GetScale() const { return transform_.scale; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }

	//色
	Vector4 GetColor() const { return model_->GetColor(); }

	//ライティングの有効無効
	bool IsEnableLighting() const { return model_->IsEnableLighting(); }

	//ライティングのカラー
	Vector4 GetLightingColor() const { return directionalLightData_->color; }

	//ライティングの強さ
	float GetLightingIntensity() const { return directionalLightData_->intensity; }

	//ライティングの向き
	Vector3 GetLightingDirection() const { return directionalLightData_->direction; }

	//反射強度
	float GetShininess() const { return model_->GetShininess(); }

public: /*========[ セッター ]========*/
	//モデルの設定
	void SetModel(Model* model) { model_ = model; }
	void SetModel(const std::string& filePath) { model_ = ModelManager::GetInstance()->FindModel(filePath); }
	Model* GetModel() const { return model_; }

	//カメラの設定
	void SetCamera(Camera* camera) { camera_ = camera; }

	//Transform
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

	Matrix4x4 GetWorldMatrix() const { return transformationMatrixData_ ? transformationMatrixData_->World : MakeIdentity4x4(); }

	//色
	void SetColor(const Vector4& color) const { model_->SetColor(color); }

	//ライティングの有効無効
	void SetEnableLighting(bool enable) const { model_->SetEnableLighting(enable); }

	//ライティングのカラー
	void SetLightingColor(const Vector4& color) const { directionalLightData_->color  = color; }

	//ライティングの強さ
	void SetLightingIntensity(float intensity) const { directionalLightData_->intensity = intensity; }

	//ライティングの向き
	void SetLightingDirection(const Vector3& direction) const { directionalLightData_->direction = direction; }

	//反射強度
	void SetShininess(float shininess) const { model_->SetShininess(shininess); }

	//ライト
	void SetDirectionalLightColor(const Vector4& color) { directionalLightData_->color = color; }
	void SetDirectionalLightDirection(const Vector3& direction) { directionalLightData_->direction = direction; }
	void SetDirectionalLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }
	void SetDirectionalLight(const DirectionalLight& light) { *directionalLightData_ = light; }

	void SetLightManager(LightManager* lightManager) { lightManager_ = lightManager; }

private: /*========[ プライベートメンバ関数(このクラス内でしか使わない関数)  ]========*/

	/**
	 * \brief 座標変換行列の生成
	 */
	void CreateWvpData();

	/**
	 * \brief 平行光源データの生成
	 */
	void CreateDirectionalLightData();

	/**
	 * \brief カメラデータの生成
	 */
	void CreateCameraData();

	/**
	 * \brief 描画設定の初期化
	 */
	void InitializeRenderingSettings();
	

private: /*========[ 描画用変数 ]========*/
	//オブジェクトのコマンド
	Object3dCommon* object3dCommon_ = nullptr;

	//バッファリソース	
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;

	//バッファリソース内のデータを指すポインタ
	TransformationMatrix* transformationMatrixData_ = nullptr;
	DirectionalLight* directionalLightData_ = nullptr;
	CameraForGPU* cameraData_ = nullptr;


private: /*========[ メンバ変数 ]========*/
	//カメラ
	Camera* camera_ = nullptr;

	//モデル
	Model* model_ = nullptr;

	//ライトマネージャー
	LightManager* lightManager_ = nullptr;

	//座標変換行列
	Transform transform_;

};

