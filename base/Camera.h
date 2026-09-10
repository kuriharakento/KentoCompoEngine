#pragma once
#include "base/GraphicsTypes.h"
#include "base/WinApp.h"
#include "math/Quaternion.h"
#include <d3d12.h>
#include <wrl/client.h>

namespace KCE
{
class DirectXCommon;

/**
 * @brief GPU用カメラ定数バッファデータ
 * ParticleConvert.CS.hlslのCamera構造体と一致
 */
struct CameraGPUData
{
	Matrix4x4 view;
	Matrix4x4 projection;
	Vector3 eye;
	float padding;
};

/**
 * @brief カメラクラス
 */
class Camera
{
public:
	/**
	 * @brief コンストラクタ
	 */
	Camera();

	/**
	 * @brief 更新処理
	 */
	void Update();

public:
	/**
	 * @brief ワールド行列を取得
	 * @return ワールド行列
	 */
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

	/**
	 * @brief ビュー行列を取得
	 * @return ビュー行列
	 */
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

	/**
	 * @brief 透視投影行列を取得
	 * @return 透視投影行列
	 */
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

	/**
	 * @brief ビュープロジェクション行列を取得
	 * @return ビュープロジェクション行列
	 */
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

	/**
	 * @brief 座標を取得
	 * @return 座標
	 */
	Vector3 GetTranslate() const { return transform_.translate; }

	/**
	 * @brief 回転を取得
	 * @return 回転
	 */
	Vector3 GetRotate() const { return transform_.rotate; }

public:
	/**
	 * @brief 水平方向視野角を設定
	 * @param fovY 視野角（ラジアン）
	 */
	void SetFovY(float fovY) { fovY_ = fovY; }

	/**
	 * @brief アスペクト比を設定
	 * @param aspectRatio アスペクト比
	 */
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }

	/**
	 * @brief 水平方向視野角を取得
	 * @return 視野角（ラジアン）
	 */
	float GetFovY() const { return fovY_; }

	/**
	 * @brief アスペクト比を取得
	 * @return アスペクト比
	 */
	float GetAspectRatio() const { return aspectRatio_; }

	/**
	 * @brief ニアクリップ距離を設定
	 * @param nearClip ニアクリップ距離
	 */
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }

	/**
	 * @brief ファークリップ距離を設定
	 * @param farClip ファークリップ距離
	 */
	void SetFarClip(float farClip) { farClip_ = farClip; }

	/**
	 * @brief 座標を設定
	 * @param translate 座標
	 */
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

	/**
	 * @brief 回転を設定
	 * @details オイラー角で設定する。以降はオイラー角モードで動作する。
	 * @param rotate 回転
	 */
	void SetRotate(const Vector3& rotate)
	{
		transform_.rotate = rotate;
		useQuaternionRotation_ = false;
	}

	/**
	 * @brief 回転をクォータニオンで設定
	 *
	 * @details シーケンサのカメラトラックはこちらを使う。
	 *          カットをまたぐ大きな回転をオイラー角のまま補間すると
	 *          ジンバルロックや最短経路の取り違えで破綻するため、
	 *          補間はクォータニオンで行い、結果をそのまま行列に落とす。
	 * @param rotation 回転クォータニオン
	 */
	void SetRotateQuaternion(const Quaternion& rotation)
	{
		rotationQuaternion_ = rotation;
		useQuaternionRotation_ = true;
		// オイラー角のゲッタとインスペクタ表示を一致させておく
		transform_.rotate = rotation.ToEuler();
	}

	/**
	 * @brief 回転をクォータニオンで取得
	 * @details オイラー角モードの場合は現在のオイラー角から変換して返す。
	 * @return 回転クォータニオン
	 */
	Quaternion GetRotateQuaternion() const
	{
		return useQuaternionRotation_ ? rotationQuaternion_ : Quaternion::FromEuler(transform_.rotate);
	}

	/**
	 * @brief クォータニオン回転モードかどうか
	 * @return クォータニオンで回転している場合true
	 */
	bool IsUsingQuaternionRotation() const { return useQuaternionRotation_; }

public:
	/**
	 * @brief シェイクを開始
	 * @param intensity シェイクの強度
	 * @param duration シェイクの継続時間（秒）
	 */
	void StartShake(float intensity, float duration);

	/**
	 * @brief 一時的なFOVズームを開始
	 * @param fovOffset 視野角のオフセット（負の値でズームイン）
	 * @param duration ズームの継続時間（秒）
	 */
	void StartZoom(float fovOffset, float duration);

	/**
	 * @brief GPU定数バッファを初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 */
	void InitializeConstantBuffer(DirectXCommon* dxCommon);

	/**
	 * @brief GPU定数バッファのGPU仮想アドレスを取得
	 * @return GPU仮想アドレス（バッファ未初期化時は0）
	 */
	D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferAddress() const;

private:
	// カメラのトランスフォーム
	Transform transform_ = {
		{ 1.0f,1.0f,1.0f },
		{ 0.0f,0.0f,0.0f },
		{ 0.0f,4.0f,-10.0f },
	};

	// クォータニオンによる回転（useQuaternionRotation_ が真のときに使う）
	Quaternion rotationQuaternion_ = Quaternion::Identity();
	// 回転をクォータニオンで扱うかどうか
	bool useQuaternionRotation_ = false;

	// ワールド行列
	Matrix4x4 worldMatrix_;
	// ビュー行列
	Matrix4x4 viewMatrix_;
	// 透視投影行列
	Matrix4x4 projectionMatrix_;
	// ビュープロジェクション行列
	Matrix4x4 viewProjectionMatrix_;

	// 水平方向視野角
	float fovY_ = 0.45f;
	// アスペクト比
	float aspectRatio_ = float(WinApp::kClientWidth) / float(WinApp::kClientHeight);
	// ニアクリップ距離
	float nearClip_ = 0.1f;
	// ファークリップ距離
	float farClip_ = 100.0f;

	// シェイクによるオフセット
	Vector3 shakeOffset_ = { 0.0f, 0.0f, 0.0f };
	// シェイクの持続時間
	float shakeDuration_ = 0.0f;
	// シェイクの残り時間
	float shakeTimer_ = 0.0f;
	// シェイクの強度
	float shakeIntensity_ = 0.0f;

	// 一時的なFOVズーム用オフセット
	float zoomFovOffset_ = 0.0f;
	// ズームの持続時間
	float zoomDuration_ = 0.0f;
	// ズームの残り時間
	float zoomTimer_ = 0.0f;
	// 開始時のズームオフセット
	float startZoomFovOffset_ = 0.0f;

	// GPU定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	CameraGPUData* constantData_ = nullptr;
};
} // namespace KCE
