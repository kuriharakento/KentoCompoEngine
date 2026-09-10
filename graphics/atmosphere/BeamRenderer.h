#pragma once
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>

#include "math/MatrixFunc.h"
#include "math/Vector2.h"
#include "math/Vector3.h"

namespace KCE
{
class Camera;
class DirectXCommon;
class FrameConstantAllocator;
class GBuffer;
class LightManager;
class SrvManager;

/**
 * @brief スポットライトの光の筋（ビーム）をコーンメッシュの加算合成で描くクラス
 *
 * @details SEQUENCER_PLAN 7.3 の方式 (B)。「何十本ものビームが客席を舐める」絵は、
 *          レイマーチ (A) ではコストの都合で本数が出せないため、まずこちらで絵を作る。
 *
 *          スポットライトごとに円錐を置き、次の3つで光の筋に見せる。
 *          - 光源から離れるほど暗くする（光の広がり）
 *          - 円錐の縁ほど暗くする（筋の中心が明るく見える）
 *          - 壁や床に近いほど暗くする（深度フェード）。これが無いと、
 *            円錐が床に刺さった線がくっきり見えて一目で嘘だと分かる
 *
 *          深度はシェーダー内で自分で比較するため、深度バッファはバインドしない。
 */
class BeamRenderer
{
public:
	/**
	 * @brief 全体の設定
	 */
	struct Settings
	{
		bool enabled = false;
		// ビーム全体の明るさ。空気の濃さ（霧）に相当する
		float intensity = 0.15f;
		// 光源から離れるほど暗くなる速さ
		float lengthFalloff = 1.5f;
		// 円錐の縁の暗さ。大きいほど筋が細く見える
		float edgePower = 2.0f;
		// 壁や床の手前でこの距離をかけて消える（ワールド単位）
		float fadeDistance = 1.0f;
		// ライトの届く距離に対するビームの長さの比率
		float lengthScale = 1.0f;
	};

	/**
	 * @brief GPU へ渡す定数（ビーム1本ぶん）。Beam.VS/PS.hlsl の cbuffer BeamConstants と一致させること
	 */
	struct ConstantsForGPU
	{
		Matrix4x4 viewProjection;
		Matrix4x4 view;
		Matrix4x4 invProjection;
		Vector3 apex;
		float length;
		Vector3 axisX;
		float radius;
		Vector3 axisY;
		float intensity;
		Vector3 direction;
		float fadeDistance;
		Vector3 color;
		float edgePower;
		Vector2 screenSize;
		float lengthFalloff;
		float padding;
	};
	static_assert(sizeof(ConstantsForGPU) == 288, "BeamConstants のサイズがシェーダー側と一致しません");

	~BeamRenderer();

	/**
	 * @brief 初期化する
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SrvManagerへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief ビームを有効にしたスポットライトすべてについて描く
	 *
	 * @details 呼び出し時点で、深度バッファは DEPTH_WRITE 状態でバインドされている前提。
	 *          描画中は SRV として読み、終わったら DEPTH_WRITE へ戻す。
	 *
	 * @param camera このビューのカメラ
	 * @param gBuffer 深度を読むG-Buffer
	 * @param sceneColorRtv 描き込む先のRTV
	 * @param lightManager スポットライトの取得元
	 * @param allocator フレーム単位の定数割り当て器
	 */
	void Draw(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv,
		LightManager* lightManager, FrameConstantAllocator* allocator);

	/**
	 * @brief 指定したスポットライトのビームを有効にするか設定する
	 * @param lightName スポットライトの名前
	 * @param enabled 有効にするなら真
	 */
	void SetBeamEnabled(const std::string& lightName, bool enabled) { beamEnabled_[lightName] = enabled; }

	/**
	 * @brief 指定したスポットライトのビームが有効か
	 */
	bool IsBeamEnabled(const std::string& lightName) const;

	/**
	 * @brief スポットライトごとのビームの明るさの倍率を設定する
	 * @details 全体の明るさ（Settings::intensity）に掛かる。シーケンサの Light トラックから動かす。
	 * @param lightName スポットライトの名前
	 * @param scale 倍率（既定 1）
	 */
	void SetBeamScale(const std::string& lightName, float scale) { beamScale_[lightName] = scale; }

	/**
	 * @brief スポットライトごとのビームの明るさの倍率
	 * @return 倍率。設定されていなければ 1
	 */
	float GetBeamScale(const std::string& lightName) const;

	/**
	 * @brief シェーダーを再コンパイルしてパイプラインを作り直す（ホットリロード用）
	 */
	bool ReloadShaders(std::string& outError);

	Settings& GetSettings() { return settings_; }

#ifdef USE_IMGUI
	void RegisterDebugUI(LightManager* lightManager);
	void DrawImGui();
#endif

private:
	bool CreateRootSignature(std::string& outError);
	bool CreatePipeline(std::string& outError);
	/** @brief 単位円錐のメッシュを作る（頂点が原点、+Z 方向に長さ1、底面の半径1） */
	void CreateConeMesh();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	UINT vertexCount_ = 0;

	Settings settings_;
	// スポットライト名 → ビームを描くか
	std::unordered_map<std::string, bool> beamEnabled_;
	// スポットライト名 → ビームの明るさの倍率
	std::unordered_map<std::string, float> beamScale_;

#ifdef USE_IMGUI
	// デバッグUIでライトの一覧を出すための参照
	LightManager* debugLightManager_ = nullptr;
#endif
};
} // namespace KCE
