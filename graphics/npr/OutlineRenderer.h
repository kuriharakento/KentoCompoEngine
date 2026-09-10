#pragma once
#include <string>
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
class SrvManager;

/**
 * @brief アニメ調の輪郭線を描くクラス
 *
 * @details SEQUENCER_PLAN 7.1。背面法（inverted hull）はメッシュごとに
 *          もう一度描く必要があり、スキニングやインスタンス描画のそれぞれに
 *          対応を入れることになる。ここでは G-Buffer の深度と法線の段差から
 *          輪郭を検出する画面空間の方式にした。全オブジェクトに1パスで効き、
 *          描画経路ごとの対応が要らない。
 *
 *          線を引くかどうかは素材ごとの outlineStrength（G-Buffer の発光 RT の
 *          アルファ）で決める。既定は 0 なので、既存の見た目は変わらない。
 *          背景との境界にも線が出るよう、周囲のピクセルの値も見る。
 *
 *          @b 制限: G-Buffer を通らないフォワード描画のオブジェクトには線が出ない。
 */
class OutlineRenderer
{
public:
	/**
	 * @brief 全体の設定
	 */
	struct Settings
	{
		bool enabled = true;
		// 線の太さ（ピクセル）
		float width = 1.5f;
		// 奥行きの差がこの割合を超えたら輪郭とみなす（相対値）
		float depthThreshold = 0.08f;
		// 法線の内積がこれを下回ったら輪郭とみなす（折れ目の線）
		float normalThreshold = 0.6f;
		// 線の色。真っ黒より、少し色味を残した暗色のほうがアニメ調の絵になじむ
		Vector3 color = { 0.12f, 0.09f, 0.14f };
	};

	/**
	 * @brief GPU へ渡す定数。Outline.PS.hlsl の cbuffer OutlineConstants と一致させること
	 */
	struct ConstantsForGPU
	{
		Matrix4x4 invProjection;
		Vector2 screenSize;
		float width;
		float depthThreshold;
		Vector3 color;
		float normalThreshold;
	};
	static_assert(sizeof(ConstantsForGPU) == 96, "OutlineConstants のサイズがシェーダー側と一致しません");

	~OutlineRenderer();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief 輪郭線を描く
	 * @details 呼び出し時点で深度は DEPTH_WRITE 状態の前提。描画中は SRV にし、終わったら戻す。
	 */
	void Draw(Camera* camera, GBuffer* gBuffer, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, FrameConstantAllocator* allocator);

	/** @brief シェーダーを再コンパイルしてパイプラインを作り直す（ホットリロード用） */
	bool ReloadShaders(std::string& outError);

	Settings& GetSettings() { return settings_; }

#ifdef USE_IMGUI
	void RegisterDebugUI();
	void DrawImGui();
#endif

private:
	bool CreateRootSignature(std::string& outError);
	bool CreatePipeline(std::string& outError);

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Settings settings_;
};
} // namespace KCE
