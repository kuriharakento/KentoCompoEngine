#pragma once
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

#include "graphics/text/TextMesh3D.h"

namespace KCE
{
class Camera;
class DirectXCommon;
class FrameConstantAllocator;
class GlyphAtlas;
struct SelectionItem;

/**
 * @brief 3D 空間の文字（TextMesh3D）をまとめて描く
 *
 * - 1文字1枚の板をインスタンス描画する。アルファで重ね、深度は見るが書かない
 * - 半透明の後に描くので、霧・ビーム・光の筋の手前に出る
 * - TextMesh3D は名前で登録する。シーケンサの Text3D トラックはこの名前で探す
 */
class Text3DRenderer
{
public:
	/** @brief 1回の描画で出せる文字数の上限 */
	static constexpr size_t kMaxGlyphs = 2048;

	~Text3DRenderer();

	/**
	 * @brief 初期化
	 * @param dxCommon 所有しない
	 * @param atlas 文字のアトラス。所有しない（このインスタンスより長生きする前提）
	 */
	void Initialize(DirectXCommon* dxCommon, GlyphAtlas* atlas);

	/**
	 * @brief 文字列を名前で登録する
	 * @param name シーケンサから探すときの名前。同じ名前は後から登録した方で上書き
	 * @param mesh 所有しない。破棄する前に Unregister すること
	 */
	void Register(const std::string& name, TextMesh3D* mesh);
	void Unregister(TextMesh3D* mesh);

	/** @return 見つからなければ nullptr（所有しない） */
	TextMesh3D* Find(const std::string& name) const;

	/**
	 * @brief 登録されている文字列を全部描く
	 * @param camera このビューのカメラ
	 * @param sceneColorRtv 描き込む先
	 * @param depthDsv シーンの深度（DEPTH_WRITE 状態）
	 * @param allocator 板のデータとカメラ定数の置き場
	 */
	void Draw(Camera* camera, D3D12_CPU_DESCRIPTOR_HANDLE sceneColorRtv, D3D12_CPU_DESCRIPTOR_HANDLE depthDsv, FrameConstantAllocator* allocator);

	bool ReloadShaders(std::string& outError) { return CreatePipeline(outError); }

#ifdef USE_IMGUI
	void RegisterDebugUI();
	void DrawHierarchyImGui();
	void DrawInspectorImGui(const SelectionItem& item);
#endif

private:
	bool CreateRootSignature(std::string& outError);
	bool CreatePipeline(std::string& outError);

	// 所有しない
	DirectXCommon* dxCommon_ = nullptr;
	GlyphAtlas* atlas_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	// 先に濃い所の深度だけを書くパス。被写界深度が文字の距離を読めるようにする
	Microsoft::WRL::ComPtr<ID3D12PipelineState> depthPipelineState_;

	// 名前 → 文字列。所有しない
	std::unordered_map<std::string, TextMesh3D*> meshes_;
	// 板のデータを集める作業領域。使い回して毎フレーム確保しない
	std::vector<TextMesh3D::Instance> instances_;
};
} // namespace KCE
