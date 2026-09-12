#include "graphics/2d/GlyphAtlas.h"

#include <Windows.h>
#include <dwrite_2.h>
#include <wrl.h>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DirectXTex/d3dx12.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "base/StringUtility.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "manager/graphics/TextureManager.h"

#pragma comment(lib, "dwrite.lib")

namespace KCE
{
namespace
{
constexpr uint32_t kAtlasSize = GlyphAtlas::kTextureSize;
// 隣の文字がにじんで入らないように空ける幅
constexpr uint32_t kGlyphPadding = 2;
constexpr uint32_t kBytesPerPixel = 4;
constexpr uint8_t kOpaque = 255;
constexpr DXGI_FORMAT kAtlasFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DWRITE_FONT_WEIGHT kFontWeight = DWRITE_FONT_WEIGHT_SEMI_BOLD;

// 当たるフォントの順。先頭が見た目の基準で、無い文字だけ後ろのフォントで描く
constexpr const wchar_t* kFontFaces[] = { L"Yu Gothic", L"Meiryo", L"MS Gothic", L"Segoe UI Symbol", L"Segoe UI Emoji" };

struct CharRange
{
	wchar_t first;
	wchar_t last;
};
// 起動時に先に描いておく文字。漢字は使われたときに描き足す
constexpr CharRange kPrefillRanges[] = {
	{ 0x0020, 0x007E }, // ASCII
	{ 0x3000, 0x303F }, // 句読点・括弧などの記号
	{ 0x3041, 0x3096 }, // ひらがな
	{ 0x30A0, 0x30FF }, // カタカナ
	{ 0xFF01, 0xFF5E }, // 全角の英数記号
};

uint32_t AlignUp(uint32_t value, uint32_t alignment)
{
	return (value + alignment - 1) / alignment * alignment;
}
} // namespace

struct GlyphAtlas::Impl
{
	// 所有しない
	DirectXCommon* dxCommon = nullptr;

	Microsoft::WRL::ComPtr<IDWriteFactory2> factory;
	std::vector<Microsoft::WRL::ComPtr<IDWriteFontFace>> faces;
	float emSize = 0.0f;

	// 要素のアドレスは再ハッシュでも変わらないので、Acquire はポインタを返してよい
	std::unordered_map<wchar_t, GlyphInfo> glyphs;
	// どのフォントにも無かった文字。毎回探し直さない
	std::unordered_set<wchar_t> missing;

	// CPU 側の濃さ（1ピクセル1バイト）。GPU へは変わった矩形だけ送る
	std::vector<uint8_t> coverage;
	// 棚詰め（左から詰めて、はみ出したら次の段へ）
	uint32_t cursorX = kGlyphPadding;
	uint32_t cursorY = kGlyphPadding;
	uint32_t shelfHeight = 0;
	bool full = false;

	bool dirty = false;
	uint32_t dirtyLeft = 0;
	uint32_t dirtyTop = 0;
	uint32_t dirtyRight = 0;
	uint32_t dirtyBottom = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> texture;
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
	// 転送に使ったバッファ。GPU が使い終わるまで持っておく
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> pendingUploads;

	void MarkDirty(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
	{
		if (!dirty)
		{
			dirtyLeft = left;
			dirtyTop = top;
			dirtyRight = right;
			dirtyBottom = bottom;
			dirty = true;
			return;
		}
		dirtyLeft = (std::min)(dirtyLeft, left);
		dirtyTop = (std::min)(dirtyTop, top);
		dirtyRight = (std::max)(dirtyRight, right);
		dirtyBottom = (std::max)(dirtyBottom, bottom);
	}

	void Transition(D3D12_RESOURCE_STATES next)
	{
		if (state == next)
		{
			return;
		}
		const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), state, next);
		dxCommon->GetCommandList()->ResourceBarrier(1, &barrier);
		state = next;
	}

	/**
	 * @brief 1文字を描いてアトラスへ詰める
	 * @return 描けたら真（空白のように絵の無い文字も真）
	 */
	bool Rasterize(wchar_t c, GlyphInfo& out);
};

bool GlyphAtlas::Impl::Rasterize(wchar_t c, GlyphInfo& out)
{
	const UINT32 codePoint = static_cast<UINT32>(c);
	IDWriteFontFace* face = nullptr;
	UINT16 glyphIndex = 0;
	for (const auto& candidate : faces)
	{
		UINT16 index = 0;
		// 0 はそのフォントに無い（豆腐の）グリフ
		if (SUCCEEDED(candidate->GetGlyphIndices(&codePoint, 1, &index)) && index != 0)
		{
			face = candidate.Get();
			glyphIndex = index;
			break;
		}
	}
	if (!face)
	{
		return false;
	}

	DWRITE_FONT_METRICS fontMetrics{};
	face->GetMetrics(&fontMetrics);
	DWRITE_GLYPH_METRICS glyphMetrics{};
	if (FAILED(face->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE)))
	{
		return false;
	}
	const float designToPixel = emSize / static_cast<float>(fontMetrics.designUnitsPerEm);
	float advance = static_cast<float>(glyphMetrics.advanceWidth) * designToPixel;

	DWRITE_GLYPH_OFFSET offset{};
	DWRITE_GLYPH_RUN run{};
	run.fontFace = face;
	run.fontEmSize = emSize;
	run.glyphCount = 1;
	run.glyphIndices = &glyphIndex;
	run.glyphAdvances = &advance;
	run.glyphOffsets = &offset;

	// グレースケールのアンチエイリアス。ALIASED_1x1 で1ピクセル1バイトの濃さが取れる
	Microsoft::WRL::ComPtr<IDWriteGlyphRunAnalysis> analysis;
	if (FAILED(factory->CreateGlyphRunAnalysis(&run, nullptr, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL,
		DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, 0.0f, &analysis)))
	{
		return false;
	}
	RECT bounds{};
	if (FAILED(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &bounds)))
	{
		return false;
	}

	out = GlyphInfo{};
	out.advance = advance;
	const uint32_t width = static_cast<uint32_t>((std::max)(0L, bounds.right - bounds.left));
	const uint32_t height = static_cast<uint32_t>((std::max)(0L, bounds.bottom - bounds.top));
	if (width == 0 || height == 0)
	{
		// 空白など。送り幅だけ持つ
		return true;
	}

	if (cursorX + width + kGlyphPadding > kAtlasSize)
	{
		cursorX = kGlyphPadding;
		cursorY += shelfHeight + kGlyphPadding;
		shelfHeight = 0;
	}
	if (cursorY + height + kGlyphPadding > kAtlasSize)
	{
		full = true;
		return false;
	}

	// 新しい文字を描くときだけの小さな確保
	std::vector<uint8_t> bitmap(static_cast<size_t>(width) * height);
	if (FAILED(analysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &bounds, bitmap.data(), static_cast<UINT32>(bitmap.size()))))
	{
		return false;
	}
	for (uint32_t row = 0; row < height; ++row)
	{
		std::copy_n(bitmap.data() + static_cast<size_t>(row) * width, width,
			coverage.data() + static_cast<size_t>(cursorY + row) * kAtlasSize + cursorX);
	}

	out.x = static_cast<float>(cursorX);
	out.y = static_cast<float>(cursorY);
	out.width = static_cast<float>(width);
	out.height = static_cast<float>(height);
	out.offsetX = static_cast<float>(bounds.left);
	out.offsetY = static_cast<float>(bounds.top);
	MarkDirty(cursorX, cursorY, cursorX + width, cursorY + height);

	cursorX += width + kGlyphPadding;
	shelfHeight = (std::max)(shelfHeight, height);
	return true;
}

GlyphAtlas::GlyphAtlas() = default;
GlyphAtlas::~GlyphAtlas() = default;

bool GlyphAtlas::Build(DirectXCommon* dxCommon, uint32_t fontPixelSize)
{
	ready_ = false;
	impl_ = std::make_unique<Impl>();
	Impl& atlas = *impl_;
	atlas.dxCommon = dxCommon;
	atlas.emSize = static_cast<float>(fontPixelSize);
	fontPixelSize_ = atlas.emSize;

	if (!dxCommon || FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(atlas.factory.GetAddressOf()))))
	{
		Logger::Log("GlyphAtlas: DirectWrite を用意できなかった。ゲーム内の文字は出ない\n", Logger::LogLevel::Error);
		return false;
	}

	Microsoft::WRL::ComPtr<IDWriteFontCollection> fonts;
	if (FAILED(atlas.factory->GetSystemFontCollection(&fonts)))
	{
		return false;
	}
	std::wstring primaryName;
	for (const wchar_t* name : kFontFaces)
	{
		UINT32 familyIndex = 0;
		BOOL exists = FALSE;
		if (FAILED(fonts->FindFamilyName(name, &familyIndex, &exists)) || !exists)
		{
			continue;
		}
		Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
		Microsoft::WRL::ComPtr<IDWriteFont> font;
		Microsoft::WRL::ComPtr<IDWriteFontFace> face;
		if (SUCCEEDED(fonts->GetFontFamily(familyIndex, &family))
			&& SUCCEEDED(family->GetFirstMatchingFont(kFontWeight, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font))
			&& SUCCEEDED(font->CreateFontFace(&face)))
		{
			if (atlas.faces.empty())
			{
				primaryName = name;
			}
			atlas.faces.push_back(face);
		}
	}
	if (atlas.faces.empty())
	{
		Logger::Log("GlyphAtlas: 日本語フォントが見つからなかった。ゲーム内の文字は出ない\n", Logger::LogLevel::Error);
		return false;
	}

	// 行の高さとベースラインは先頭のフォントに合わせる
	DWRITE_FONT_METRICS metrics{};
	atlas.faces.front()->GetMetrics(&metrics);
	const float designToPixel = atlas.emSize / static_cast<float>(metrics.designUnitsPerEm);
	ascent_ = static_cast<float>(metrics.ascent) * designToPixel;
	lineHeight_ = static_cast<float>(metrics.ascent + metrics.descent + metrics.lineGap) * designToPixel;

	atlas.coverage.assign(static_cast<size_t>(kAtlasSize) * kAtlasSize, 0);
	DirectX::TexMetadata metadata{};
	metadata.width = kAtlasSize;
	metadata.height = kAtlasSize;
	metadata.depth = 1;
	metadata.arraySize = 1;
	metadata.mipLevels = 1;
	metadata.format = kAtlasFormat;
	metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;
	// 作りたてはコピー先の状態で、中身は 0（透明）
	atlas.texture = dxCommon->CreateTextureResource(metadata);
	atlas.state = D3D12_RESOURCE_STATE_COPY_DEST;
	TextureManager::GetInstance()->RegisterTexture(kTextureKey, atlas.texture, metadata);
	ready_ = true;

	size_t prefilled = 0;
	for (const CharRange& range : kPrefillRanges)
	{
		for (wchar_t c = range.first; c <= range.last; ++c)
		{
			if (Acquire(c))
			{
				++prefilled;
			}
		}
	}
	FlushUploads();
	// 何も描けなかった場合も、読める状態にしておく
	atlas.Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	Logger::Log("GlyphAtlas: DirectWrite（" + StringUtility::ConvertString(primaryName) + "）で " + std::to_string(prefilled) + " 文字を先に描いた。残りは使われたときに描き足す\n");
	return true;
}

const GlyphInfo* GlyphAtlas::Acquire(wchar_t c)
{
	if (!ready_)
	{
		return nullptr;
	}
	Impl& atlas = *impl_;
	const auto found = atlas.glyphs.find(c);
	if (found != atlas.glyphs.end())
	{
		return &found->second;
	}
	if (atlas.full || atlas.missing.count(c) != 0)
	{
		return nullptr;
	}

	GlyphInfo info;
	if (!atlas.Rasterize(c, info))
	{
		atlas.missing.insert(c);
		if (atlas.full)
		{
			Logger::Log("GlyphAtlas: アトラスが一杯になった。これ以降の新しい文字は '?' で出る\n", Logger::LogLevel::Warning);
		}
		return nullptr;
	}
	return &atlas.glyphs.emplace(c, info).first->second;
}

void GlyphAtlas::FlushUploads()
{
	if (!impl_ || !impl_->dirty || !impl_->texture)
	{
		return;
	}
	Impl& atlas = *impl_;
	const uint32_t width = atlas.dirtyRight - atlas.dirtyLeft;
	const uint32_t height = atlas.dirtyBottom - atlas.dirtyTop;
	const uint32_t rowPitch = AlignUp(width * kBytesPerPixel, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	Microsoft::WRL::ComPtr<ID3D12Resource> upload = atlas.dxCommon->CreateBufferResource(static_cast<size_t>(rowPitch) * height);
	uint8_t* mapped = nullptr;
	if (!upload || FAILED(upload->Map(0, nullptr, reinterpret_cast<void**>(&mapped))))
	{
		return;
	}
	// 白にして、濃さを透明度に入れる（色は Sprite の色で付ける）
	for (uint32_t row = 0; row < height; ++row)
	{
		const uint8_t* source = atlas.coverage.data() + static_cast<size_t>(atlas.dirtyTop + row) * kAtlasSize + atlas.dirtyLeft;
		uint8_t* destination = mapped + static_cast<size_t>(row) * rowPitch;
		for (uint32_t column = 0; column < width; ++column)
		{
			uint8_t* pixel = destination + static_cast<size_t>(column) * kBytesPerPixel;
			pixel[0] = kOpaque;
			pixel[1] = kOpaque;
			pixel[2] = kOpaque;
			pixel[3] = source[column];
		}
	}
	upload->Unmap(0, nullptr);

	atlas.Transition(D3D12_RESOURCE_STATE_COPY_DEST);
	D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
	destinationLocation.pResource = atlas.texture.Get();
	destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	destinationLocation.SubresourceIndex = 0;
	D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
	sourceLocation.pResource = upload.Get();
	sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	sourceLocation.PlacedFootprint.Offset = 0;
	sourceLocation.PlacedFootprint.Footprint = { kAtlasFormat, width, height, 1, rowPitch };
	atlas.dxCommon->GetCommandList()->CopyTextureRegion(&destinationLocation, atlas.dirtyLeft, atlas.dirtyTop, 0, &sourceLocation, nullptr);
	atlas.Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	atlas.pendingUploads.push_back(upload);
	atlas.dirty = false;
}

void GlyphAtlas::BeginFrame()
{
	if (impl_)
	{
		impl_->pendingUploads.clear();
	}
}
} // namespace KCE
