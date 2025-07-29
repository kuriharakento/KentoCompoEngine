#pragma once
#include "base/GraphicsTypes.h"
#include "base/WinApp.h"

class Camera
{
public:
	Camera();
	void Update();

public: //ゲッター

	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	Vector3 GetTranslate() const { return transform_.translate; }
	Vector3 GetRotate() const { return transform_.rotate; }

public: //セッター
	//水平方向視野角の設定
	void SetFovY(float fovY) { fovY_ = fovY; }
	//アスペクト比の設定
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	//ニアクリップ距離の設定
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	//ファークリップ距離の設定
	void SetFarClip(float farClip) { farClip_ = farClip; }
	//座標の設定
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	//回転の設定
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }

public:
	// シェイク開始メソッド
	void StartShake(float intensity, float duration);

private:
	//トランスフォーム
	Transform transform_ = {
		{ 1.0f,1.0f,1.0f },
		{ 0.0f,0.0f,0.0f },
		{ 0.0f,4.0f,-10.0f },
	};;

	//ワールド行列
	Matrix4x4 worldMatrix_;
	//ビュー行列
	Matrix4x4 viewMatrix_;
	//透視投影行列
	Matrix4x4 projectionMatrix_;
	//合成行列
	Matrix4x4 viewProjectionMatrix_;

	//水平方向視野角
	float fovY_ = 0.45f;
	//アスペクト比
	float aspectRatio_ = float(WinApp::kClientWidth) / float(WinApp::kClientHeight);
	//ニアクリップ距離
	float nearClip_ = 0.1f;
	//ファークリップ距離
	float farClip_ = 100.0f;

	// シェイク関連のメンバー変数
	Vector3 shakeOffset_ = { 0.0f, 0.0f, 0.0f }; // シェイクによるオフセット
	float shakeDuration_ = 0.0f; // シェイクの持続時間
	float shakeTimer_ = 0.0f; // 残り時間
	float shakeIntensity_ = 0.0f; // シェイクの強度

};

