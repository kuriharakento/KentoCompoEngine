#pragma once
#include <unordered_map>
#include <string>

// light
#include "light/LightConstants.h"
#include "light/PointLight.h"
#include "light/SpotLight.h"
// system
#include "base/DirectXCommon.h"
// math
#include "math/VectorColorCodes.h"

class LightManager
{
public:
	//コンストラクタ
	LightManager();

	~LightManager();

	//初期化
	void Initialize(DirectXCommon* dxCommon);

	//更新
	void Update();

	//描画
	void Draw();

	//ポイントライトの追加
	void AddPointLight(const std::string& name);

	// スポットライトの追加
	void AddSpotLight(const std::string& name);

	//ライトの削除
	void Clear();

	/**
	 * \brief グラデーション
	 * \param name ライトの名前
	 * \param startColor 開始したい色
	 * \param endColor 終了したい色
	 * \param duration グラデーションにかける時間
	 * \param easingFunction イージング関数
	 */
	void StartGradient(const std::string& name, const Vector4& startColor, const Vector4& endColor, float duration, std::function<float(float)> easingFunction);


public: //セッター
	// ポイントライトのプロパティ設定
	void SetPointLightColor(const std::string& name, const Vector4& color);
	void SetPointLightPosition(const std::string& name, const Vector3& position);
	void SetPointLightIntensity(const std::string& name, float intensity);
	void SetPointLightRadius(const std::string& name, float radius);
	void SetPointLightDecay(const std::string& name, float decay);

	// スポットライトのプロパティ設定
	void SetSpotLightColor(const std::string& name, const Vector4& color);
	void SetSpotLightPosition(const std::string& name, const Vector3& position);
	void SetSpotLightIntensity(const std::string& name, float intensity);
	void SetSpotLightDirection(const std::string& name, const Vector3& direction);
	void SetSpotLightDistance(const std::string& name, float distance);
	void SetSpotLightDecay(const std::string& name, float decay);
	void SetSpotLightCosAngle(const std::string& name, float cosAngle);
	void SetSpotLightCosFalloffStart(const std::string& name, float cosFalloffStart);

public: //ゲッター
	//ポイントライトの数の取得
	const uint32_t& GetPointLightCount() const;
	//スポットライトの数の取得
	const uint32_t& GetSpotLightCount() const;

	//ポイントライトの取得
	const GPUPointLight& GetPointLight(const std::string& name) const;
	//スポットライトの取得
	const GPUSpotLight& GetSpotLight(const std::string& name) const;

	// ポイントライトのプロパティ取得
	const Vector4& GetPointLightColor(const std::string& name) const;
	const Vector3& GetPointLightPosition(const std::string& name) const;
	float GetPointLightIntensity(const std::string& name) const;
	float GetPointLightRadius(const std::string& name) const;
	float GetPointLightDecay(const std::string& name) const;

	// スポットライトのプロパティ取得
	const Vector4& GetSpotLightColor(const std::string& name) const;
	const Vector3& GetSpotLightPosition(const std::string& name) const;
	float GetSpotLightIntensity(const std::string& name) const;
	const Vector3& GetSpotLightDirection(const std::string& name) const;
	float GetSpotLightDistance(const std::string& name) const;
	float GetSpotLightDecay(const std::string& name) const;
	float GetSpotLightCosAngle(const std::string& name) const;
	float GetSpotLightCosFalloffStart(const std::string& name) const;

private:
	//ImGui
	void ImGuiUpdate();

	//定数バッファの作成
	void CreateConstantBuffer();

private:
	//ポイントライト
	std::unordered_map<std::string, CPUPointLight> pointLights_;
	//ポイントライトの名前
	std::vector<std::string> pointLightNames_;

	//スポットライト
	std::unordered_map<std::string, CPUSpotLight> spotLights_;
	//スポットライトの名前
	std::vector<std::string> spotLightNames_;

	//ライトの数
	LightCount lightCount_;

	//DxCommon
	DirectXCommon* dxCommon_ = nullptr;

	//定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> lightCountResource_;

	//書き込むデータ
	GPUPointLight* pointLightData_ = nullptr;
	GPUSpotLight* spotLightData_ = nullptr;
	LightCount* lightCountData_ = nullptr;

	//イージング関数ポインタ
	float (*pEasingFunc_)(float) = nullptr;

	//持続時間
	float duration_ = 1.0f;

	//ポイントライトの開始色
	Vector4 startPointLightColor_ = VectorColorCodes::White;
	//ポイントライトの終了色
	Vector4 endPointLightColor_ = VectorColorCodes::Purple;

	//スポットライトの開始色
	Vector4 startSpotLightColor_ = VectorColorCodes::White;
	//スポットライトの終了色
	Vector4 endSpotLightColor_ = VectorColorCodes::Red;
};
