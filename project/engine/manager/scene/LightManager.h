#pragma once
#include <unordered_map>
#include <string>

// light
#include "light/LightConstants.h"
#include "light/DirectionalLight.h"
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
	 * @brief デバッグ用ライト可視化描画
	 * @details ポイントライト、スポットライト、ディレクショナルライトの位置と方向を線で描画する
	 */
	void DrawDebugLines();

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

public: // DirectionalLight関連
	/**
	 * @brief ディレクショナルライトを設定
	 * @param light ディレクショナルライトデータ
	 */
	void SetDirectionalLight(const DirectionalLight& light);

	/**
	 * @brief ディレクショナルライトを取得
	 * @return ディレクショナルライトデータへの参照
	 */
	const DirectionalLight& GetDirectionalLight() const { return directionalLight_; }

	/**
	 * @brief ディレクショナルライトのシャドウ用行列を更新
	 * @param targetCenter シャドウを投影する中心位置
	 * @param shadowMapSize シャドウマップがカバーする範囲（ワールド単位）
	 * @param nearPlane 近クリップ面
	 * @param farPlane 遠クリップ面
	 */
	void UpdateDirectionalLightShadowMatrix(const Vector3& targetCenter, float shadowMapSize, float nearPlane, float farPlane);

	/**
	 * @brief ディレクショナルライトのビュー行列を取得
	 * @return ビュー行列
	 */
	const Matrix4x4& GetDirectionalLightViewMatrix() const { return directionalLightView_; }

	/**
	 * @brief ディレクショナルライトのプロジェクション行列を取得
	 * @return プロジェクション行列
	 */
	const Matrix4x4& GetDirectionalLightProjectionMatrix() const { return directionalLightProjection_; }

	/**
	 * @brief ディレクショナルライトのビュー・プロジェクション行列を取得
	 * @return ビュー・プロジェクション行列
	 */
	const Matrix4x4& GetDirectionalLightViewProjectionMatrix() const { return directionalLightViewProjection_; }

	/**
	 * @brief ディレクショナルライトの定数バッファのGPUアドレスを取得
	 * @return GPUバーチャルアドレス
	 */
	D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightGPUAddress() const;

	/**
	 * @brief シャドウ用行列の定数バッファのGPUアドレスを取得
	 * @return GPUバーチャルアドレス
	 */
	D3D12_GPU_VIRTUAL_ADDRESS GetShadowMatrixGPUAddress() const;

	/**
	 * @brief カスケードシャドウ用行列を更新
	 * @param camera カメラポインタ（視錐台分割用）
	 * @param nearPlane カメラの近クリップ面
	 * @param farPlane カメラの遠クリップ面
	 */
	void UpdateCascadeShadowMatrices(class Camera* camera, float nearPlane = 0.1f, float farPlane = 200.0f);

	/**
	 * @brief 指定カスケードのビュー・プロジェクション行列を取得
	 * @param cascadeIndex カスケードインデックス
	 * @return ビュー・プロジェクション行列
	 */
	const Matrix4x4& GetCascadeViewProjection(uint32_t cascadeIndex) const;

	/**
	 * @brief カスケードシャドウ用GPUデータのアドレスを取得
	 * @return GPUバーチャルアドレス
	 */
	D3D12_GPU_VIRTUAL_ADDRESS GetCascadeShadowDataGPUAddress() const;

	/**
	 * @brief 指定カスケードのライトビュープロジェクション行列のGPUアドレスを取得
	 * @param cascadeIndex カスケードインデックス
	 * @return GPUバーチャルアドレス（CascadeShadowDataForGPU内のオフセット）
	 */
	D3D12_GPU_VIRTUAL_ADDRESS GetCascadeLightViewProjectionGPUAddress(uint32_t cascadeIndex) const;

public: // SpotLight シャドウ関連
	/**
	 * @brief スポットライトのシャドウ用行列を更新
	 * @param name ライトの名前
	 * @param nearPlane 近クリップ面
	 * @param farPlane 遠クリップ面
	 */
	void UpdateSpotLightShadowMatrix(const std::string& name, float nearPlane = 0.1f, float farPlane = 100.0f);

	/**
	 * @brief スポットライトのシャドウ用ビュープロジェクション行列を取得
	 * @param name ライトの名前
	 * @return ビュープロジェクション行列
	 */
	const Matrix4x4& GetSpotLightShadowMatrix(const std::string& name) const;

	/**
	 * @brief スポットライトのシャドウを有効化
	 * @param name ライトの名前
	 * @param enabled 有効フラグ
	 */
	void SetSpotLightShadowEnabled(const std::string& name, bool enabled);

	/**
	 * @brief スポットライトのシャドウが有効かどうか
	 * @param name ライトの名前
	 * @return 有効な場合true
	 */
	bool IsSpotLightShadowEnabled(const std::string& name) const;

	/**
	 * @brief スポットライトマップを取得
	 * @return スポットライトのマップ
	 */
	const std::unordered_map<std::string, CPUSpotLight>& GetSpotLights() const { return spotLights_; }

	/**
	 * @brief スポットライトのシャドウ行列GPUアドレスを取得
	 * @param name ライトの名前
	 * @return GPUバーチャルアドレス（シャドウパス用）
	 */
	D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightShadowMatrixGPUAddress(const std::string& name) const;

public: // PointLight シャドウ関連
	/**
	 * @brief ポイントライトのシャドウ用行列を更新（6面）
	 * @param name ライトの名前
	 * @param nearPlane 近クリップ面
	 * @param farPlane 遠クリップ面
	 */
	void UpdatePointLightShadowMatrix(const std::string& name, float nearPlane = 0.1f, float farPlane = 100.0f);

	/**
	 * @brief ポイントライトのシャドウ用ビュープロジェクション行列を取得（指定面）
	 * @param name ライトの名前
	 * @param faceIndex 面のインデックス（0-5）
	 * @return ビュープロジェクション行列
	 */
	const Matrix4x4& GetPointLightShadowMatrix(const std::string& name, uint32_t faceIndex) const;

	/**
	 * @brief ポイントライトのシャドウを有効化
	 * @param name ライトの名前
	 * @param enabled 有効フラグ
	 */
	void SetPointLightShadowEnabled(const std::string& name, bool enabled);

	/**
	 * @brief ポイントライトのシャドウが有効かどうか
	 * @param name ライトの名前
	 * @return 有効な場合true
	 */
	bool IsPointLightShadowEnabled(const std::string& name) const;

	/**
	 * @brief ポイントライトマップを取得
	 * @return ポイントライトのマップ
	 */
	const std::unordered_map<std::string, CPUPointLight>& GetPointLights() const { return pointLights_; }

	/**
	 * @brief ポイントライトのシャドウ行列GPUアドレスを取得
	 * @param name ライトの名前
	 * @param faceIndex 面のインデックス（0-5）
	 * @return GPUバーチャルアドレス（シャドウパス用）
	 */
	D3D12_GPU_VIRTUAL_ADDRESS GetPointLightShadowMatrixGPUAddress(const std::string& name, uint32_t faceIndex) const;

public: // ゲッター
	/**
	 * @brief ポイントライトの数を取得
	 * @return ポイントライトの数
	 */
	uint32_t GetPointLightCount() const;

	/**
	 * @brief スポットライトの数を取得
	 * @return スポットライトの数
	 */
	uint32_t GetSpotLightCount() const;

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
	 * @brief ポイントライトリソースの取得
	 */
	ID3D12Resource* GetPointLightResource() const { return pointLightResource_.Get(); }

	/**
	 * @brief スポットライトリソースの取得
	 */
	ID3D12Resource* GetSpotLightResource() const { return spotLightResource_.Get(); }

	/**
	 * @brief ライト数リソースの取得
	 */
	ID3D12Resource* GetLightCountResource() const { return lightCountResource_.Get(); }

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
	 * @brief ディレクショナルライト用定数バッファの作成
	 */
	void CreateDirectionalLightBuffer();

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

	// ディレクショナルライトデータ
	DirectionalLight directionalLight_;
	// ディレクショナルライト用定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	// ディレクショナルライトのデータポインタ
	DirectionalLight* directionalLightData_ = nullptr;

	// シャドウ用行列
	Matrix4x4 directionalLightView_ = {};
	Matrix4x4 directionalLightProjection_ = {};
	Matrix4x4 directionalLightViewProjection_ = {};

	// シャドウ用GPU構造体（シェーダーと同じレイアウト）
	struct ShadowDataForGPU {
		Matrix4x4 lightViewProjection;
		int32_t enableShadow;
		float padding[3];
	};

	// シャドウ用行列の定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> shadowMatrixResource_;
	ShadowDataForGPU* shadowData_ = nullptr;

	// カスケードシャドウ用GPU構造体
	struct CascadeShadowDataForGPU {
		Matrix4x4 lightViewProjections[4];
		float cascadeSplits[4];
		int32_t enableShadow;
		float padding[3];
	};

	// カスケードシャドウ用行列（CPU側）
	Matrix4x4 cascadeViewProjections_[4] = {};
	float cascadeSplits_[4] = {};

	// カスケードシャドウ用定数バッファ（シェーダー用）
	Microsoft::WRL::ComPtr<ID3D12Resource> cascadeShadowResource_;
	CascadeShadowDataForGPU* cascadeShadowData_ = nullptr;

	// 各カスケードのライトビュープロジェクション行列用個別バッファ（シャドウパス用、256バイトアライメント）
	Microsoft::WRL::ComPtr<ID3D12Resource> cascadeLightVPResources_[4];
	Matrix4x4* cascadeLightVPData_[4] = {};

	// スポットライト用シャドウ行列バッファ（シャドウパス用、最大8つ）
	static constexpr uint32_t kMaxSpotLightShadows = 8;
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightVPResources_[kMaxSpotLightShadows];
	Matrix4x4* spotLightVPData_[kMaxSpotLightShadows] = {};
	std::unordered_map<std::string, uint32_t> spotLightVPIndices_; // ライト名 -> バッファインデックス

	// ポイントライト用シャドウ行列バッファ（シャドウパス用、最大2つ×6面）
	static constexpr uint32_t kMaxPointLightShadows = 2;
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightVPResources_[kMaxPointLightShadows][6];
	Matrix4x4* pointLightVPData_[kMaxPointLightShadows][6] = {};
	std::unordered_map<std::string, uint32_t> pointLightVPIndices_; // ライト名 -> バッファインデックス
};
