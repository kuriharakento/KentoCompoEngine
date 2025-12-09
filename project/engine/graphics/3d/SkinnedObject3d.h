#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <memory>

// base
#include "base/GraphicsTypes.h"
// graphics
#include "SkinnedModel.h"
#include "SkinningCompute.h"
// animation
#include "animation/Animator.h"
// light
#include "light/DirectionalLight.h"
// camera
#include "base/Camera.h"

class Object3dCommon;
class SrvManager;
class LightManager;

/**
 * @brief スキニング3Dオブジェクトクラス
 * @details スケルタルアニメーション付きの3Dモデルを管理・描画する
 */
class SkinnedObject3d
{
public:
	~SkinnedObject3d();

	/**
	 * @brief 初期化
	 * @param object3dCommon Object3dCommonへのポインタ
	 */
	void Initialize(Object3dCommon* object3dCommon);

	/**
	 * @brief 更新
	 * @param deltaTime フレーム時間（秒）
	 */
	void Update(float deltaTime);

	/**
	 * @brief スキニング計算の実行
	 * @details コンピュートシェーダーによる頂点変形を行う
	 */
	void DispatchSkinning();

	/**
	 * @brief 描画
	 */
	void Draw();

	/**
	 * @brief シャドウマップ用描画
	 */
	void DrawShadow();

	/**
	 * @brief G-Buffer用描画
	 */
	void DrawGBuffer();

public: // アニメーション関連
	/**
	 * @brief アニメーションの再生
	 * @param animationIndex アニメーションインデックス
	 * @param loop ループ再生するか
	 */
	void PlayAnimation(uint32_t animationIndex, bool loop = true);

	/**
	 * @brief アニメーション名で再生
	 * @param animationName アニメーション名
	 * @param loop ループ再生するか
	 */
	void PlayAnimation(const std::string& animationName, bool loop = true);

	/**
	 * @brief アニメーションの停止
	 */
	void StopAnimation();

	/**
	 * @brief アニメーションが再生中か
	 */
	bool IsAnimationPlaying() const;

	/**
	 * @brief 特定ボーンのワールド行列を取得（ボーンアタッチ用）
	 * @param boneName ボーン名
	 * @return ボーンのワールド変換行列
	 */
	Matrix4x4 GetBoneWorldMatrix(const std::string& boneName) const;

	/**
	 * @brief アニメーターへのアクセス
	 */
	Animator* GetAnimator() { return &animator_; }

public: // アクセッサ
	/**
	 * @brief モデルの設定（ムーブ）
	 * @param model スキニングモデル
	 */
	void SetModel(std::unique_ptr<SkinnedModel> model);

	/**
	 * @brief モデルの設定（ファイルパスから）
	 * @param filePath モデルのファイルパス（フォルダ名）
	 * @param modelType モデルファイルの拡張子（デフォルト: ".gltf"）
	 */
	void SetModel(const std::string& filePath, const std::string& modelType = ".gltf");

	/**
	 * @brief モデルの取得
	 */
	SkinnedModel* GetModel() const { return model_.get(); }

	/**
	 * @brief カメラの設定
	 */
	void SetCamera(Camera* camera) { camera_ = camera; }

	/**
	 * @brief ライトマネージャーの設定
	 */
	void SetLightManager(LightManager* lightManager) { lightManager_ = lightManager; }

	/**
	 * @brief スケールの設定
	 */
	void SetScale(const Vector3& scale) { transform_.scale = scale; }

	/**
	 * @brief 回転の設定
	 */
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }

	/**
	 * @brief 位置の設定
	 */
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

	/**
	 * @brief スケールの取得
	 */
	const Vector3& GetScale() const { return transform_.scale; }

	/**
	 * @brief 回転の取得
	 */
	const Vector3& GetRotate() const { return transform_.rotate; }

	/**
	 * @brief 位置の取得
	 */
	const Vector3& GetTranslate() const { return transform_.translate; }

	/**
	 * @brief ワールド行列の取得
	 */
	Matrix4x4 GetWorldMatrix() const { return worldMatrix_; }

	/**
	 * @brief 色の設定
	 */
	void SetColor(const Vector4& color);

	/**
	 * @brief ライティングの有効/無効の設定
	 */
	void SetEnableLighting(bool enable);

	/**
	 * @brief 反射強度の設定
	 */
	void SetShininess(float shininess);

	/**
	 * @brief ディレクショナルライト全体の設定
	 */
	void SetDirectionalLight(const DirectionalLight& light) { directionalLight_ = light; }

	/**
	 * @brief ディレクショナルライトの色を設定
	 */
	void SetDirectionalLightColor(const Vector4& color) { directionalLight_.color = color; }

	/**
	 * @brief ディレクショナルライトの向きを設定
	 */
	void SetDirectionalLightDirection(const Vector3& direction) { directionalLight_.direction = direction; }

	/**
	 * @brief ディレクショナルライトの強度を設定
	 */
	void SetDirectionalLightIntensity(float intensity) { directionalLight_.intensity = intensity; }

private:
	/**
	 * @brief 座標変換行列の更新
	 */
	void UpdateTransform();

	/**
	 * @brief 描画リソースの作成
	 */
	void CreateDrawResources();

private:
	// Object3dCommonへのポインタ
	Object3dCommon* object3dCommon_ = nullptr;

	// スキニングモデル
	std::unique_ptr<SkinnedModel> model_;

	// アニメーター
	Animator animator_;

	// スキニング計算用
	std::unique_ptr<SkinningCompute> skinningCompute_;

	// トランスフォーム
	Transform transform_;

	// ワールド行列
	Matrix4x4 worldMatrix_;

	// カメラ
	Camera* camera_ = nullptr;

	// WVP行列リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	TransformationMatrix* wvpData_ = nullptr;

	// カメラ情報リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;

	// ディレクショナルライト
	DirectionalLight directionalLight_;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	// ライトマネージャー
	LightManager* lightManager_ = nullptr;
};
