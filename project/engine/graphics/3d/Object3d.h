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
// スプライト共通部分のポインタ
class Object3dCommon;

/**
 * @brief 3Dオブジェクトクラス
 * @details 3Dモデルを持ち、座標変換、ライティング、カメラ情報を管理して描画するクラス
 */
class Object3d
{
public:	/*========[ メンバ関数 ]========*/
	/**
	 * @brief デストラクタ
	 * @details リソースの解放を行う
	 */
	~Object3d();

	/**
	 * @brief 初期化
	 * @param object3dCommon 3Dオブジェクト共通部へのポインタ
	 * @param camera カメラへのポインタ（省略時はデフォルトカメラを使用）
	 */
	void Initialize(Object3dCommon* object3dCommon,Camera* camera = nullptr);

	/**
	 * @brief 更新
	 * @param camera カメラマネージャーへのポインタ（省略可）
	 */
	void Update(CameraManager* camera = nullptr);

	/**
	 * @brief 描画
	 * @details 座標変換行列、ライティング、カメラ情報を設定して描画する
	 */
	void Draw();

	/**
	 * @brief 行列の更新
	 * @param camera 使用するカメラ（省略時はデフォルトカメラを使用）
	 */
	void UpdateMatrix(Camera* camera = nullptr);

	/**
	 * @brief ワールド行列のみの更新
	 * @details ビュープロジェクション行列は更新せず、ワールド行列のみを更新する
	 */
	void UpdateWorldMatrix();

	/**
	 * @brief 外部ワールド行列を使用した行列の更新
	 * @param worldMatrix 外部から渡されたワールド行列
	 * @param camera 使用するカメラ（省略時はデフォルトカメラを使用）
	 */
	void UpdateMatrixWithWorld(const Matrix4x4& worldMatrix, Camera* camera = nullptr);

public: /*========[ ゲッター ]========*/
	/**
	 * @brief スケールの取得
	 * @return スケール値
	 */
	const Vector3& GetScale() const { return transform_.scale; }

	/**
	 * @brief 回転の取得
	 * @return 回転値（ラジアン）
	 */
	const Vector3& GetRotate() const { return transform_.rotate; }

	/**
	 * @brief 位置の取得
	 * @return 位置
	 */
	const Vector3& GetTranslate() const { return transform_.translate; }

	/**
	 * @brief 色の取得
	 * @return 色（RGBA）
	 */
	Vector4 GetColor() const { return model_->GetColor(); }

	/**
	 * @brief ライティングの有効/無効の取得
	 * @return ライティング有効フラグ
	 */
	bool IsEnableLighting() const { return model_->IsEnableLighting(); }

	/**
	 * @brief ライティングカラーの取得
	 * @return ライトの色
	 */
	Vector4 GetLightingColor() const { return directionalLightData_->color; }

	/**
	 * @brief ライティング強度の取得
	 * @return ライトの強度
	 */
	float GetLightingIntensity() const { return directionalLightData_->intensity; }

	/**
	 * @brief ライティング方向の取得
	 * @return ライトの方向ベクトル
	 */
	Vector3 GetLightingDirection() const { return directionalLightData_->direction; }

	/**
	 * @brief 反射強度の取得
	 * @return 反射強度
	 */
	float GetShininess() const { return model_->GetShininess(); }

public: /*========[ セッター ]========*/
	/**
	 * @brief モデルの設定（ムーブ）
	 * @param model 設定するモデル
	 */
	void SetModel(std::unique_ptr<Model> model) { model_ = std::move(model); }

	/**
	 * @brief モデルの設定（ファイルパスから）
	 * @param filePath モデルのファイルパス
	 */
	void SetModel(const std::string& filePath)
	{
		Model* model = ModelManager::GetInstance()->FindModel(filePath);
		model_ = model ? std::make_unique<Model>(*model) : nullptr;
	}

	/**
	 * @brief モデルの取得
	 * @return モデルへのポインタ
	 */
	Model* GetModel() const { return model_.get(); }

	/**
	 * @brief カメラの設定
	 * @param camera カメラへのポインタ
	 */
	void SetCamera(Camera* camera) { camera_ = camera; }

	/**
	 * @brief スケールの設定
	 * @param scale 新しいスケール値
	 */
	void SetScale(const Vector3& scale) { transform_.scale = scale; }

	/**
	 * @brief 回転の設定
	 * @param rotate 新しい回転値（ラジアン）
	 */
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }

	/**
	 * @brief 位置の設定
	 * @param translate 新しい位置
	 */
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

	/**
	 * @brief ワールド行列の取得
	 * @return ワールド行列
	 */
	Matrix4x4 GetWorldMatrix() const { return transformationMatrixData_ ? transformationMatrixData_->World : MakeIdentity4x4(); }

	/**
	 * @brief 色の設定
	 * @param color 新しい色
	 */
	void SetColor(const Vector4& color) const { model_->SetColor(color); }

	/**
	 * @brief ライティングの有効/無効の設定
	 * @param enable ライティング有効フラグ
	 */
	void SetEnableLighting(bool enable) const { model_->SetEnableLighting(enable); }

	/**
	 * @brief ライティングカラーの設定
	 * @param color 新しいライトの色
	 */
	void SetLightingColor(const Vector4& color) const { directionalLightData_->color  = color; }

	/**
	 * @brief ライティング強度の設定
	 * @param intensity 新しいライトの強度
	 */
	void SetLightingIntensity(float intensity) const { directionalLightData_->intensity = intensity; }

	/**
	 * @brief ライティング方向の設定
	 * @param direction 新しいライトの方向
	 */
	void SetLightingDirection(const Vector3& direction) const { directionalLightData_->direction = direction; }

	/**
	 * @brief 反射強度の設定
	 * @param shininess 新しい反射強度
	 */
	void SetShininess(float shininess) const { model_->SetShininess(shininess); }

	/**
	 * @brief ディレクショナルライトカラーの設定
	 * @param color 新しいライトの色
	 */
	void SetDirectionalLightColor(const Vector4& color) { directionalLightData_->color = color; }

	/**
	 * @brief ディレクショナルライト方向の設定
	 * @param direction 新しいライトの方向
	 */
	void SetDirectionalLightDirection(const Vector3& direction) { directionalLightData_->direction = direction; }

	/**
	 * @brief ディレクショナルライト強度の設定
	 * @param intensity 新しいライトの強度
	 */
	void SetDirectionalLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }

	/**
	 * @brief ディレクショナルライトの一括設定
	 * @param light ディレクショナルライトデータ
	 */
	void SetDirectionalLight(const DirectionalLight& light) { *directionalLightData_ = light; }

	/**
	 * @brief ライトマネージャーの設定
	 * @param lightManager ライトマネージャーへのポインタ
	 */
	void SetLightManager(LightManager* lightManager) { lightManager_ = lightManager; }

private: /*========[ プライベートメンバ関数  ]========*/

	/**
	 * @brief 座標変換行列の生成
	 * @details WVP行列用のバッファを作成する
	 */
	void CreateWvpData();

	/**
	 * @brief 平行光源データの生成
	 * @details ディレクショナルライト用のバッファを作成する
	 */
	void CreateDirectionalLightData();

	/**
	 * @brief カメラデータの生成
	 * @details カメラ位置用のバッファを作成する
	 */
	void CreateCameraData();

	/**
	 * @brief 描画設定の初期化
	 * @details 各種データを生成する
	 */
	void InitializeRenderingSettings();
	

private: /*========[ 描画用変数 ]========*/
	// オブジェクト共通部へのポインタ
	Object3dCommon* object3dCommon_ = nullptr;

	// 座標変換行列バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	// ディレクショナルライトバッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	// カメラバッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;

	// 座標変換行列データへのポインタ
	TransformationMatrix* transformationMatrixData_ = nullptr;
	// ディレクショナルライトデータへのポインタ
	DirectionalLight* directionalLightData_ = nullptr;
	// カメラデータへのポインタ
	CameraForGPU* cameraData_ = nullptr;


private: /*========[ メンバ変数 ]========*/
	// カメラへのポインタ
	Camera* camera_ = nullptr;

	// モデル
	std::unique_ptr<Model> model_ = nullptr;

	// ライトマネージャーへのポインタ
	LightManager* lightManager_ = nullptr;

	// Transform情報
	Transform transform_;
};


