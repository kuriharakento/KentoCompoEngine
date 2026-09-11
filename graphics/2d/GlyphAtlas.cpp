#include "graphics/2d/GlyphAtlas.h"

#include <Windows.h>
#include <algorithm>
#include <vector>

#include "base/Logger.h"
#include "base/StringUtility.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "manager/graphics/TextureManager.h"

namespace KCE
{
namespace
{
// アトラスの一辺（ピクセル）。48px なら第1水準まで収まる
constexpr uint32_t kAtlasSize = 4096;
// 隣の文字がにじんで入らないように空ける幅
constexpr int kGlyphPadding = 2;
constexpr uint32_t kBytesPerPixel = 4;
constexpr uint8_t kOpaque = 255;

// Shift_JIS（コードページ 932）の2バイト文字を順に変換して、JIS 第1水準までを集める
constexpr UINT kShiftJisCodePage = 932;
constexpr unsigned kSjisLeadFirst = 0x81;
constexpr unsigned kSjisLeadLast = 0x98;
constexpr unsigned kSjisTrailFirst = 0x40;
constexpr unsigned kSjisTrailLast = 0xFC;
constexpr unsigned kSjisTrailUnused = 0x7F;
// 第1水準の最後は 0x9872
constexpr unsigned kSjisLevel1LastTrail = 0x72;
constexpr wchar_t kAsciiFirst = 0x20;
constexpr wchar_t kAsciiLast = 0x7E;

// 試すフォントの順。どれも Windows に最初から入っている
constexpr const wchar_t* kFontFaces[] = { L"Yu Gothic", L"Meiryo", L"MS Gothic" };

std::vector<wchar_t> CollectCharacters()
{
	std::vector<wchar_t> chars;
	for (wchar_t c = kAsciiFirst; c <= kAsciiLast; ++c)
	{
		chars.push_back(c);
	}
	for (unsigned lead = kSjisLeadFirst; lead <= kSjisLeadLast; ++lead)
	{
		for (unsigned trail = kSjisTrailFirst; trail <= kSjisTrailLast; ++trail)
		{
			if (trail == kSjisTrailUnused)
			{
				continue;
			}
			if (lead == kSjisLeadLast && trail > kSjisLevel1LastTrail)
			{
				break;
			}
			const char bytes[2] = { static_cast<char>(lead), static_cast<char>(trail) };
			wchar_t wc = 0;
			// 未定義のコードは変換に失敗するので、そのまま飛ばせる
			if (MultiByteToWideChar(kShiftJisCodePage, MB_ERR_INVALID_CHARS, bytes, 2, &wc, 1) == 1)
			{
				chars.push_back(wc);
			}
		}
	}
	return chars;
}

/**
 * @brief 日本語が出るフォントを作る
 * @details 名前が一致するか、日本語の文字セットを持つものを採用する。
 *          日本語版 Windows では面名がローカライズされて返ることがあるため。
 */
HFONT CreateJapaneseFont(HDC dc, int pixelSize, std::wstring& outFace)
{
	for (const wchar_t* face : kFontFaces)
	{
		// 高さを負で渡すと、行の高さではなく文字そのものの大きさになる
		HFONT font = CreateFontW(-pixelSize, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
		if (!font)
		{
			continue;
		}
		HGDIOBJ old = SelectObject(dc, font);
		wchar_t actual[LF_FACESIZE] = {};
		GetTextFaceW(dc, LF_FACESIZE, actual);
		const bool japanese = GetTextCharset(dc) == SHIFTJIS_CHARSET;
		SelectObject(dc, old);
		if (_wcsicmp(actual, face) == 0 || japanese)
		{
			outFace = actual;
			return font;
		}
		DeleteObject(font);
	}
	return nullptr;
}

/**
 * @brief 文字を左上から詰めて描き、位置を記録する
 * @return アトラスに入りきらなかった文字の数
 */
size_t RasterizeGlyphs(HDC dc, int lineHeight, std::unordered_map<wchar_t, GlyphInfo>& glyphs)
{
	const std::vector<wchar_t> chars = CollectCharacters();
	glyphs.reserve(chars.size());

	int x = kGlyphPadding;
	int y = kGlyphPadding;
	size_t dropped = 0;
	for (wchar_t c : chars)
	{
		SIZE size{};
		if (!GetTextExtentPoint32W(dc, &c, 1, &size))
		{
			continue;
		}
		if (x + size.cx + kGlyphPadding > static_cast<int>(kAtlasSize))
		{
			x = kGlyphPadding;
			y += lineHeight + kGlyphPadding;
		}
		if (y + lineHeight + kGlyphPadding > static_cast<int>(kAtlasSize))
		{
			++dropped;
			continue;
		}
		TextOutW(dc, x, y, &c, 1);
		glyphs[c] = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(size.cx), static_cast<float>(lineHeight), static_cast<float>(size.cx) };
		x += size.cx + kGlyphPadding;
	}
	return dropped;
}

/**
 * @brief GDI の白黒の絵を「白・透明度 = 濃さ」のテクスチャにして登録する
 */
bool UploadAtlas(const uint8_t* bgra)
{
	DirectX::ScratchImage image;
	if (FAILED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, kAtlasSize, kAtlasSize, 1, 1)))
	{
		return false;
	}
	const DirectX::Image* dst = image.GetImage(0, 0, 0);
	for (uint32_t y = 0; y < kAtlasSize; ++y)
	{
		const uint8_t* srcRow = bgra + static_cast<size_t>(y) * kAtlasSize * kBytesPerPixel;
		uint8_t* dstRow = dst->pixels + static_cast<size_t>(y) * dst->rowPitch;
		for (uint32_t x = 0; x < kAtlasSize; ++x)
		{
			const uint8_t* s = srcRow + x * kBytesPerPixel;
			uint8_t* d = dstRow + x * kBytesPerPixel;
			// グレースケールのアンチエイリアスなので、どのチャンネルも同じ濃さになっている
			const uint8_t coverage = (std::max)({ s[0], s[1], s[2] });
			d[0] = kOpaque;
			d[1] = kOpaque;
			d[2] = kOpaque;
			d[3] = coverage;
		}
	}
	TextureManager::GetInstance()->LoadTextureFromImage(GlyphAtlas::kTextureKey, image);
	return true;
}
} // namespace

bool GlyphAtlas::Build(uint32_t fontPixelSize)
{
	glyphs_.clear();
	ready_ = false;
	fontPixelSize_ = static_cast<float>(fontPixelSize);

	HDC dc = CreateCompatibleDC(nullptr);
	if (!dc)
	{
		return false;
	}

	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = static_cast<LONG>(kAtlasSize);
	// 負の高さで上から下へ並ぶ（テクスチャと同じ向き）
	info.bmiHeader.biHeight = -static_cast<LONG>(kAtlasSize);
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);

	std::wstring face;
	HFONT font = (bitmap && bits) ? CreateJapaneseFont(dc, static_cast<int>(fontPixelSize), face) : nullptr;
	size_t dropped = 0;
	if (font)
	{
		HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
		HGDIOBJ oldFont = SelectObject(dc, font);
		SetTextColor(dc, RGB(255, 255, 255));
		SetBkMode(dc, TRANSPARENT);

		TEXTMETRICW metrics{};
		GetTextMetricsW(dc, &metrics);
		lineHeight_ = static_cast<float>(metrics.tmHeight);
		dropped = RasterizeGlyphs(dc, metrics.tmHeight, glyphs_);
		// 描画が終わってから DIB を読む
		GdiFlush();
		ready_ = !glyphs_.empty() && UploadAtlas(static_cast<const uint8_t*>(bits));

		SelectObject(dc, oldFont);
		SelectObject(dc, oldBitmap);
		DeleteObject(font);
	}
	if (bitmap)
	{
		DeleteObject(bitmap);
	}
	DeleteDC(dc);

	if (ready_)
	{
		Logger::Log("GlyphAtlas: " + StringUtility::ConvertString(face) + " で " + std::to_string(glyphs_.size()) + " 文字を焼いた（入りきらず " + std::to_string(dropped) + " 文字）\n");
	}
	else
	{
		Logger::Log("GlyphAtlas: 日本語フォントの文字を焼けなかった。ゲーム内の文字は出ない\n", Logger::LogLevel::Error);
	}
	return ready_;
}

const GlyphInfo* GlyphAtlas::Find(wchar_t c) const
{
	const auto it = glyphs_.find(c);
	return it != glyphs_.end() ? &it->second : nullptr;
}
} // namespace KCE
