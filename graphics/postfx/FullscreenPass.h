#pragma once
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <wrl.h>

namespace KCE
{
class DirectXCommon;

/**
 * @brief 全画面パスの重ね方
 */
enum class FullscreenBlend
{
	Opaque,	  //!< そのまま書く
	Alpha,	  //!< 出力のアルファで重ねる
	Additive, //!< 足し込む
};

/**
 * @brief 全画面パスの作り方
 */
struct FullscreenPassDesc
{
	// ピクセルシェーダー。頂点シェーダーは PostEffect.VS.hlsl を使う
	std::wstring pixelShaderPath;
	DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN;
	FullscreenBlend blend = FullscreenBlend::Opaque;
	// t0 から順に使う SRV の数
	uint32_t srvCount = 1;
};

/**
 * @brief 全画面の三角形を1枚描くパスの、ルートシグネチャとパイプラインをまとめたもの
 *
 * - b0: 定数（ピクセルシェーダー）
 * - t0..: SRV。1つずつ別のテーブルにする（SRV の番号が連続しているとは限らないため）
 * - s0: リニア・クランプ / s1: ポイント・クランプ / s2: 比較（LESS_EQUAL、シャドウマップ用）
 * - 出力先とビューポートは呼ぶ側が設定しておくこと
 */
class FullscreenPass
{
public:
	static constexpr uint32_t kMaxSrvCount = 8;

	/**
	 * @brief ルートシグネチャとパイプラインを作る
	 * @return 作れたら真。失敗したら outError に理由が入る
	 */
	bool Create(DirectXCommon* dxCommon, const FullscreenPassDesc& desc, std::string& outError);

	/** @brief シェーダーを読み直す。失敗したら今のパイプラインを使い続ける */
	bool ReloadShaders(std::string& outError);

	bool IsReady() const { return pipelineState_ != nullptr; }

	/**
	 * @brief 描く
	 * @param constants b0 に渡す定数。0 なら設定しない
	 * @param srvs t0 から順に渡す SRV
	 * @param srvCount srvs の数
	 */
	void Draw(D3D12_GPU_VIRTUAL_ADDRESS constants, const D3D12_GPU_DESCRIPTOR_HANDLE* srvs, uint32_t srvCount);

private:
	bool CreateRootSignature(std::string& outError);
	bool CreatePipeline(std::string& outError);

	// 所有しない
	DirectXCommon* dxCommon_ = nullptr;
	FullscreenPassDesc desc_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
} // namespace KCE
