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

/**
 * @brief ライトマネージャークラス
 * @details ポイントライトとスポットライトの管理を行う
 *          ライトの追加、削除、プロパティ設定、グラデーション機能を提供
 *          GPUへの定数バッファ転送も行う
 */
class LightManager
{
public:
	/**
	 * @brief コンストラクタ
	 */
	LightManager();

	/**
	 * @brief デストラクタ
	 */
	~LightManager();

	/**
	 * @brief 初期化処理
	 * @param dxCommon DirectXCommonへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

	/**
	 * @brief 更新処理
	 * @details グラデーション処理とGPUデータの更新を行う
	 */
	void Update();

	/**
	 * @brief 描画処理
	 * @details 定数バッファをGPUにバインドする
	 */
	void Draw();

	/**
	 * @brief ポイントライトの追加
	 * @param name ライトの名前
	 */
	void AddPointLight(const std::string& name);

	/**
	 * @brief スポットライトの追加
	 * @param name ライトの名前
	 */
	void AddSpotLight(const std::string& name);

	/**
	 * @brief 全ライトの削除
	 */
	void Clear();

	/**
	 * @brief グラデーションの開始
	 * @param name ライトの名前
	 * @param startColor 開始色
	 * @param endColor 終了色
	 * @param duration グラデーションにかける時間（秒）
	 * @param easingFunction イージング関数
	 */
	void StartGradient(const std::string& name, const Vector4& startColor, const Vector4& endColor, float duration, std::function<float(float)> easingFunction);


public: // セッター
	/**
	 * @brief ポイントライトの色を設定
	 * @param name ライトの名前
	 * @param color 設定する色
	 */
	void SetPointLightColor(const std::string& name, const Vector4& color);

	/**
	 * @brief ポイントライトの位置を設定
	 * @param name ライトの名前
	 * @param position 設定する位置
	 */
	void SetPointLightPosition(const std::string& name, const Vector3& position);

	/**
	 * @brief ポイントライトの強度を設定
	 * @param name ライトの名前
	 * @param intensity 設定する強度
	 */
	void SetPointLightIntensity(const std::string& name, float intensity);

	/**
	 * @brief ポイントライトの半径を設定
	 * @param name ライトの名前
	 * @param radius 設定する半径
	 */
	void SetPointLightRadius(const std::string& name, float radius);

	/**
	 * @brief ポイントライトの減衰率を設定
	 * @param name ライトの名前
	 * @param decay 設定する減衰率
	 */
	void SetPointLightDecay(const std::string& name, float decay);

	/**
	 * @brief スポットライトの色を設定
	 * @param name ライトの名前
	 * @param color 設定する色
	 */
	void SetSpotLightColor(const std::string& name, const Vector4& color);

	/**
	 * @brief スポットライトの位置を設定
	 * @param name ライトの名前
	 * @param position 設定する位置
	 */
	void SetSpotLightPosition(const std::string& name, const Vector3& position);

	/**
	 * @brief スポットライトの強度を設定
	 * @param name ライトの名前
	 * @param intensity 設定する強度
	 */
	void SetSpotLightIntensity(const std::string& name, float intensity);

	/**
	 * @brief スポットライトの方向を設定
	 * @param name ライトの名前
	 * @param direction 設定する方向
	 */
	void SetSpotLightDirection(const std::string& name, const Vector3& direction);

	/**
	 * @brief スポットライトの距離を設定
	 * @param name ライトの名前
	 * @param distance 設定する距離
	 */
	void SetSpotLightDistance(const std::string& name, float distance);

	/**
	 * @brief スポットライトの減衰率を設定
	 * @param name ライトの名前
	 * @param decay 設定する減衰率
	 */
	void SetSpotLightDecay(const std::string& name, float decay);

	/**
	 * @brief スポットライトのコーン角度（cos値）を設定
	 * @param name ライトの名前
	 * @param cosAngle 設定するコーン角度のcos値
	 */
	void SetSpotLightCosAngle(const std::string& name, float cosAngle);

	/**
	 * @brief スポットライトのフォールオフ開始位置（cos値）を設定
	 * @param name ライトの名前
	 * @param cosFalloffStart 設定するフォールオフ開始位置のcos値
	 */
	void SetSpotLightCosFalloffStart(const std::string& name, float cosFalloffStart);

public: // ゲッター
	/**
	 * @brief ポイントライトの数を取得
	 * @return ポイントライトの数
	 */
	const uint32_t& GetPointLightCount() const;

	/**
	 * @brief スポットライトの数を取得
	 * @return スポットライトの数
	 */
	const uint32_t& GetSpotLightCount() const;

	/**
	 * @brief ポイントライトを取得
	 * @param name ライトの名前
	 * @return ポイントライトのGPUデータ
	 */
	const GPUPointLight& GetPointLight(const std::string& name) const;

	/**
	 * @brief スポットライトを取得
	 * @param name ライトの名前
	 * @return スポットライトのGPUデータ
	 */
	const GPUSpotLight& GetSpotLight(const std::string& name) const;

	/**
	 * @brief ポイントライトの色を取得
	 * @param name ライトの名前
	 * @return ライトの色
	 */
	const Vector4& GetPointLightColor(const std::string& name) const;

	/**
	 * @brief ポイントライトの位置を取得
	 * @param name ライトの名前
	 * @return ライトの位置
	 */
	const Vector3& GetPointLightPosition(const std::string& name) const;

	/**
	 * @brief ポイントライトの強度を取得
	 * @param name ライトの名前
	 * @return ライトの強度
	 */
	float GetPointLightIntensity(const std::string& name) const;

	/**
	 * @brief ポイントライトの半径を取得
	 * @param name ライトの名前
	 * @return ライトの半径
	 */
	float GetPointLightRadius(const std::string& name) const;

	/**
	 * @brief ポイントライトの減衰率を取得
	 * @param name ライトの名前
	 * @return ライトの減衰率
	 */
	float GetPointLightDecay(const std::string& name) const;

	/**
	 * @brief スポットライトの色を取得
	 * @param name ライトの名前
	 * @return ライトの色
	 */
	const Vector4& GetSpotLightColor(const std::string& name) const;

	/**
	 * @brief スポットライトの位置を取得
	 * @param name ライトの名前
	 * @return ライトの位置
	 */
	const Vector3& GetSpotLightPosition(const std::string& name) const;

	/**
	 * @brief スポットライトの強度を取得
	 * @param name ライトの名前
	 * @return ライトの強度
	 */
	float GetSpotLightIntensity(const std::string& name) const;

	/**
	 * @brief スポットライトの方向を取得
	 * @param name ライトの名前
	 * @return ライトの方向
	 */
	const Vector3& GetSpotLightDirection(const std::string& name) const;

	/**
	 * @brief スポットライトの距離を取得
	 * @param name ライトの名前
	 * @return ライトの距離
	 */
	float GetSpotLightDistance(const std::string& name) const;

	/**
	 * @brief スポットライトの減衰率を取得
	 * @param name ライトの名前
	 * @return ライトの減衰率
	 */
	float GetSpotLightDecay(const std::string& name) const;

	/**
	 * @brief スポットライトのコーン角度（cos値）を取得
	 * @param name ライトの名前
	 * @return コーン角度のcos値
	 */
	float GetSpotLightCosAngle(const std::string& name) const;

	/**
	 * @brief スポットライトのフォールオフ開始位置（cos値）を取得
	 * @param name ライトの名前
	 * @return フォールオフ開始位置のcos値
	 */
	float GetSpotLightCosFalloffStart(const std::string& name) const;

private:
	/**
	 * @brief ImGuiによる更新処理
	 */
	void ImGuiUpdate();

	/**
	 * @brief 定数バッファの作成
	 */
	void CreateConstantBuffer();

private:
	// ポイントライトのマップ（名前 -> ライトデータ）
	std::unordered_map<std::string, CPUPointLight> pointLights_;
	// ポイントライトの名前リスト
	std::vector<std::string> pointLightNames_;

	// スポットライトのマップ（名前 -> ライトデータ）
	std::unordered_map<std::string, CPUSpotLight> spotLights_;
	// スポットライトの名前リスト
	std::vector<std::string> spotLightNames_;

	// ライトの数
	LightCount lightCount_;

	// DirectXCommonへのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;  // ポイントライト用
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;   // スポットライト用
	Microsoft::WRL::ComPtr<ID3D12Resource> lightCountResource_;  // ライト数用

	// GPUに書き込むデータへのポインタ
	GPUPointLight* pointLightData_ = nullptr;   // ポイントライトデータ
	GPUSpotLight* spotLightData_ = nullptr;     // スポットライトデータ
	LightCount* lightCountData_ = nullptr;      // ライト数データ

	// イージング関数ポインタ
	float (*pEasingFunc_)(float) = nullptr;

	// グラデーションの持続時間
	float duration_ = 1.0f;

	// ポイントライトのグラデーション用色
	Vector4 startPointLightColor_ = VectorColorCodes::White;  // 開始色
	Vector4 endPointLightColor_ = VectorColorCodes::Purple;   // 終了色

	// スポットライトのグラデーション用色
	Vector4 startSpotLightColor_ = VectorColorCodes::White;   // 開始色
	Vector4 endSpotLightColor_ = VectorColorCodes::Red;       // 終了色
};
