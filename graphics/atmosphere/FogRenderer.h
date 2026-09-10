#pragma once
#include <string>
#include <wrl.h>
#include <d3d12.h>

#include "math/MatrixFunc.h"
#include "math/Vector3.h"

namespace KCE
{
class Camera;
class DirectXCommon;
class FrameConstantAllocator;
class GBuffer;
class SrvManager;

/**
 * @brief 大気フォグ（距離と高さで濃くなる霧）を描くクラス
 *
 * @details SEQUENCER_PLAN 7.2。ボリュメトリックなビームは「空気中の粒子に光が当たって見える」
 *          ものなので、先に空気そのものを描いておかないと、ビームだけが浮いて嘘くさくなる。
 *
 *          シーンの深度から各ピクセルのワールド座標を復元し、カメラからの距離と
 *          高さで霧の濃さを決めて、霧の色を半透明で重ねる全画面パス。
 *          不透明物と Skybox を描いた後、半透明の前に置く。
 */
class FogRenderer
{
public:
	/**
	 * @brief フォグの設定
	 */
	struct Settings
	{
		bool enabled = false;
		// 霧の色（HDR可）
		Vector3 color = { 0.55f, 0.6f, 0.75f };
		// 距離あたりの濃さ。大きいほど近くで霞む
		float density = 0.02f;
		// この高さより上で薄くなり始める
		float baseHeight = 0.0f;
		// 高さによる減衰の強さ。0 で高さに関係なく一様
		float heightFalloff = 0.1f;
		// 霧の最大の不透明度。1 にすると遠景が完全に霧の色で塗り潰される
		float maxOpacity = 0.9f;
		// 空（何も描かれていないピクセル）にかける霧の量
		float skyAmount = 0.3f;
	};

	/**
	 * @brief GPU へ渡す定数。Fog.PS.hlsl の cbuffer FogConstants と一致させること
	 */
	struct ConstantsForGPU
	{
		Matrix4x4 invViewProjection;
		Vector3 cameraPosition;
		float density;
		Vector3 color;
		float heightFalloff;
		float baseHeight;
		float maxOpacity;
		float skyAmount;
		float padding;
	};
	static_assert(sizeof(ConstantsForGPU) == 112, "FogConstants のサイズがシェーダー側と一致しません");

	~FogRenderer();

	/**
	 * @brief 初期化する
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SrvManagerへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief フォグを描く
	 *
	 * @details 呼び出し時点で、深度バッファは DEPTH_WRITE 状態でバインドされている前提。
	 *          描画中は SRV として読み、終わったら DEPTH_WRITE へ戻す。
	 *
	 * @param camera このビューのカメラ
	 * @param gBuffer 深度を読むG-Buffer
	 * @param sceneColorRtv 描き込む先のRTV
	 * @param allocator フレーム単位の定数割り当て器
	 */
	void Draw(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, FrameConstantAllocator* allocator);

	/**
	 * @brief シェーダーを再コンパイルしてパイプラインを作り直す（ホットリロード用）
	 * @details 失敗した場合は既存のパイプラインを使い続ける。
	 */
	bool ReloadShaders(std::string& outError);

	Settings& GetSettings() { return settings_; }

#ifdef USE_IMGUI
	void RegisterDebugUI();
	void DrawImGui();
#endif

private:
	/** @brief ルートシグネチャを作る */
	bool CreateRootSignature(std::string& outError);
	/** @brief パイプラインを作る。成功したら差し替える */
	bool CreatePipeline(std::string& outError);

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Settings settings_;
};
} // namespace KCE
