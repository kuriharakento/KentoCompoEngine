#pragma once
#include <cstddef>
#include <d3d12.h>
#include <string>
#include <wrl.h>

#include "math/Vector4.h"

namespace KCE
{
class DirectXCommon;
class FrameConstantAllocator;

/**
 * @brief 2D の文字（TextSprite）を、文字列ごとに1回の描画命令で描く
 *
 * - 1文字1枚の板をインスタンス描画する。1文字ずつ Sprite を描くと、文字数ぶん
 *   定数バッファの更新と描画命令が積まれて重い
 * - 板のデータはフレームの置き場に書く（毎フレーム確保しない）
 * - 座標は Sprite と同じ 1920x1080 の仮想画面
 * - 描いた後は Sprite 用の設定が上書きされるので、続けて Sprite を描くなら共通設定をやり直すこと
 */
class TextSpritePipeline
{
public:
	/** @brief TextSprite.VS.hlsl の struct GlyphQuad と一致させること */
	struct Instance
	{
		// 仮想画面での左上 xy と大きさ zw
		Vector4 rect;
		// アトラスの UV（左上 xy、右下 zw）
		Vector4 uvRect;
		Vector4 color;
	};
	static_assert(sizeof(Instance) == 48, "GlyphQuad のサイズがシェーダー側と一致しません");

	~TextSpritePipeline();

	/**
	 * @brief 初期化
	 * @param dxCommon 所有しない
	 * @param allocator 板のデータの置き場。所有しない（Framework が先に死なない前提）
	 */
	bool Initialize(DirectXCommon* dxCommon, FrameConstantAllocator* allocator);

	/**
	 * @brief 板をまとめて描く
	 * @param instances 板のデータ
	 * @param count 枚数
	 */
	void Draw(const Instance* instances, size_t count);

	bool ReloadShaders(std::string& outError) { return CreatePipeline(outError); }

private:
	bool CreateRootSignature(std::string& outError);
	bool CreatePipeline(std::string& outError);

	// 所有しない
	DirectXCommon* dxCommon_ = nullptr;
	FrameConstantAllocator* allocator_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
} // namespace KCE
