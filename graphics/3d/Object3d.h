#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>

// math
#include "base/GraphicsTypes.h"
// graphics
#include "Model.h"
#include "IRenderable3d.h"
#include "light/DirectionalLight.h"
#include "manager/graphics/ModelManager.h"
// camera
#include "manager/scene/CameraManager.h"

namespace KCE
{
class LightManager;
class SrvManager;
class ShadowMapManager;
// スプライト共通部分のポインタ
class Object3dCommon;

/**
 * @brief 3Dオブジェクトクラス
 * @details 3Dモデルを持ち、座標変換、ライティング、カメラ情報を管理して描画するクラス
 *          IRenderable3dインターフェースを実装し、GameObjectから透過的に使用可能
 */
class Object3d : public IRenderable3d
{
public:	/*========[ メンバ関数 ]========*/
	/**
	 * @brief デストラクタ
	 * @details リソースの解放を行う
	 */
	~Object3d() override;

	/**
	 * @brief 初期化（インターフェース実装）
	 * @param object3dCommon 3Dオブジェクト共通部へのポインタ
	 * @param camera カメラへのポインタ（省略時はデフォルトカメラを使用）
	 */
	void Initialize(Object3dCommon* object3dCommon, Camera* camera = nullptr) override;

	/**
	 * @brief 更新（インターフェース実装）
	 * @param deltaTime フレーム時間（秒）- 現在は未使用
	 * @param camera カメラへのポインタ
	 */
	void Update(float deltaTime, Camera* camera) override;

	/**
	 * @brief 更新（既存互換）
	 * @param camera カメラマネージャーへのポインタ（省略可）
	 */
	void Update(CameraManager* camera = nullptr);

	/**
	 * @brief 描画（インターフェース実装）
	 * @details 座標変換行列、ライティング、カメラ情報を設定して描画する
	 */
	void Draw() override;

	/**
	 * @brief シャドウマップ用描画
	 * @details ライトの視点からオブジェクトを描画し、深度情報をシャドウマップに記録する
	 * @param lightViewProjectionAddress ライトビュープロジェクション行列のGPUアドレス
	 */
	void DrawShadow(D3D12_GPU_VIRTUAL_ADDRESS lightViewProjectionAddress);

	/**
	 * @brief シャドウマップ用描画（インターフェース実装）
	 * @details 外部で行列が設定されている前提で、オブジェクトの描画のみを行う
	 */
	void DrawShadowOnly() override;

	/**
	 * @brief シャドウマップ用描画（非推奨）
	 * @param lightViewProjection ライトビュープロジェクション行列
	 */
	void DrawShadowWithMatrix(const Matrix4x4& lightViewProjection);

	/**
	 * @brief G-Buffer用描画（インターフェース実装）
	 * @details ジオメトリパスでG-Bufferに描画する
	 */
	void DrawGBuffer() override;

	/**
	 * @brief 行列の更新

	 * @param camera 使用するカメラ（省略時はデフォルトカメラを使用）
	 */
	void UpdateMatrix(Camera* camera = nullptr);

	/**
	 * @brief ワールド行列のみの更新（インターフェース実装）
	 * @details ビュープロジェクション行列は更新せず、ワールド行列のみを更新する
	 */
	void UpdateWorldMatrix() override;

	/**
	 * @brief 外部ワールド行列を使用した行列の更新（インターフェース実装）
	 * @param worldMatrix 外部から渡されたワールド行列
	 * @param camera 使用するカメラ（省略時はデフォルトカメラを使用）
	 */
	void UpdateMatrixWithWorld(const Matrix4x4& worldMatrix, Camera* camera = nullptr) override;


public: /*========[ ゲッター ]========*/
	/**
	 * @brief スケールの取得
	 * @return スケール値
	 */
	const Vector3& GetScale() const override { return transform_.scale; }

	/**
	 * @brief 回転の取得
	 * @return 回転値（ラジアン）
	 */
	const Vector3& GetRotate() const override { return transform_.rotate; }

	/**
	 * @brief 位置の取得
	 * @return 位置
	 */
	const Vector3& GetTranslate() const override { return transform_.translate; }

	/**
	 * @brief 色の取得
	 * @return 色（RGBA）
	 */
	Vector4 GetColor() const override { return model_ ? model_->GetColor() : Vector4(1,1,1,1); }

	/**
	 * @brief ライティングの有効/無効の取得
	 * @return ライティング有効フラグ
	 */
	bool IsEnableLighting() const override { return model_ ? model_->IsEnableLighting() : true; }

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
	void SetCamera(Camera* camera) override { camera_ = camera; }

	/**
	 * @brief スケールの設定
	 * @param scale 新しいスケール値
	 */
	void SetScale(const Vector3& scale) override { transform_.scale = scale; }

	/**
	 * @brief 回転の設定
	 * @param rotate 新しい回転値（ラジアン）
	 */
	void SetRotate(const Vector3& rotate) override { transform_.rotate = rotate; }

	/**
	 * @brief 位置の設定
	 * @param translate 新しい位置
	 */
	void SetTranslate(const Vector3& translate) override { transform_.translate = translate; }

	/**
	 * @brief ワールド行列の取得
	 * @return ワールド行列
	 */
	Matrix4x4 GetWorldMatrix() const override { return transformationMatrixData_ ? transformationMatrixData_->World : MakeIdentity4x4(); }

	/**
	 * @brief 色の設定
	 * @param color 新しい色
	 */
	void SetColor(const Vector4& color) override { if (model_) model_->SetColor(color); }

	/**
	 * @brief ライティングの有効/無効の設定
	 * @param enable ライティング有効フラグ
	 */
	void SetEnableLighting(bool enable) override { if (model_) model_->SetEnableLighting(enable); }

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
	void SetEmissiveSettings(const EmissiveSettings& settings) override { if (model_) model_->SetEmissiveSettings(settings); }
	EmissiveSettings GetEmissiveSettings() const override { return model_ ? model_->GetEmissiveSettings() : EmissiveSettings{}; }

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
	void SetDirectionalLight(const DirectionalLight& light) override { if (directionalLightData_) *directionalLightData_ = light; }

	/**
	 * @brief ライトマネージャーの設定
	 * @param lightManager ライトマネージャーへのポインタ
	 */
	void SetLightManager(LightManager* lightManager) override { lightManager_ = lightManager; }

	/**
	 * @brief シャドウマップの設定
	 * @param srvManager SrvManagerへのポインタ
	 * @param shadowMapSrvIndex シャドウマップのSRVインデックス
	 * @param shadowMatrixGPUAddress シャドウ行列のGPUアドレス
	 */
	void SetShadowMap(SrvManager* srvManager, uint32_t shadowMapSrvIndex, D3D12_GPU_VIRTUAL_ADDRESS shadowMatrixGPUAddress);

	/**
	 * @brief シャドウマップマネージャーの設定
	 * @param shadowMapManager シャドウマップマネージャーへのポインタ
	 */
	void SetShadowMapManager(ShadowMapManager* shadowMapManager) { shadowMapManager_ = shadowMapManager; }

	/**
	 * @brief シャドウの無効化
	 * @details シャドウマップを使用しない設定にする
	 */
	void DisableShadow() { shadowEnabled_ = false; }

	/**
	 * @brief レンダリングタイプの設定
	 * @param type レンダリングタイプ
	 */
	void SetRenderingType(RenderingType type) override { renderingType_ = type; }

	/**
	 * @brief レンダリングタイプの取得
	 * @return レンダリングタイプ
	 */
	RenderingType GetRenderingType() const override { return renderingType_; }

	/**
	 * @brief シャドウを落とすかどうかの設定
	 * @param cast trueで影を落とす、falseで落とさない
	 */
	void SetCastShadow(bool cast) { castShadow_ = cast; }

	/**
	 * @brief シャドウを落とすかどうかの取得
	 * @return trueで影を落とす
	 */
	bool GetCastShadow() const { return castShadow_; }

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

	// シャドウマップ関連
	SrvManager* srvManager_ = nullptr;
	uint32_t shadowMapSrvIndex_ = 0;
	D3D12_GPU_VIRTUAL_ADDRESS shadowMatrixGPUAddress_ = 0;
	bool shadowEnabled_ = false;
	// 影用シャドウマップマネージャー。所有しない（ShadowSystemが所有、破棄されない前提）
	ShadowMapManager* shadowMapManager_ = nullptr;

	// 影を落とすかどうか（RegisterObjectシステム用）
	bool castShadow_ = true;

	// Transform情報
	Transform transform_;

	// レンダリングタイプ
	RenderingType renderingType_ = RenderingType::Deferred;
};
} // namespace KCE
