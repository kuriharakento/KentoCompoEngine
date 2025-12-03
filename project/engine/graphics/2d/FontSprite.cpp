#include "FontSprite.h"
#include <fstream>
#include <cassert>
#include "externals/nlohmann/json.hpp"

using json = nlohmann::json;

void FontSprite::Initialize(SpriteCommon* spriteCommon, const std::string& atlasTexturePath, const std::string& jsonPath)
{
    assert(spriteCommon);
    spriteCommon_ = spriteCommon;
    atlasTexturePath_ = atlasTexturePath;

    // JSONからフォントメトリクスを読み込む
    LoadFontMetrics(jsonPath);
}

void FontSprite::LoadFontMetrics(const std::string& jsonPath)
{
    std::ifstream file(jsonPath);
    if (!file.is_open())
    {
        assert(false && "Failed to open font metrics JSON");
        return;
    }

    json jsonData;
    file >> jsonData;

    // JSON形式: { "A": { "x": 0, "y": 0, "w": 64, "h": 64, "u": 0. 0, "v": 0.0, "uw": 0.0625, "vh": 0. 0625 }, ... }
    for (auto& [key, value] : jsonData.items())
    {
        if (key.length() == 1)
        {
            char ch = key[0];
            CharInfo info;
            info.x = value["x"];
            info.y = value["y"];
            info.w = value["w"];
            info.h = value["h"];
            info.u = value["u"];
            info.v = value["v"];
            info.uw = value["uw"];
            info.vh = value["vh"];

            charMetrics_[ch] = info;

            // セルサイズを最初の文字から取得
            if (cellSize_ == 64.0f)
            {
                cellSize_ = static_cast<float>(info.w);
            }
        }
    }
}

Sprite* FontSprite::GetOrCreateCharSprite(char character)
{
    // 既に生成済みならそれを返す
    auto it = charSprites_.find(character);
    if (it != charSprites_.end())
    {
        return it->second.get();
    }

    // メトリクス情報が存在しない文字は無視
    if (charMetrics_.find(character) == charMetrics_.end())
    {
        return nullptr;
    }

    // 新規スプライト作成
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon_, atlasTexturePath_);

    const CharInfo& info = charMetrics_[character];

    // テクスチャ座標を設定
    sprite->SetTextureLeftTop({ static_cast<float>(info.x), static_cast<float>(info.y) });
    sprite->SetTextureSize({ static_cast<float>(info.w), static_cast<float>(info.h) });
    sprite->SetSize({ static_cast<float>(info.w), static_cast<float>(info.h) });

    Sprite* spritePtr = sprite.get();
    charSprites_[character] = std::move(sprite);

    return spritePtr;
}

void FontSprite::Update()
{
    // 表示する文字列に含まれる文字のスプライトのみ更新
    for (char ch : text_)
    {
        if (ch == ' ') continue; // スペースはスキップ

        Sprite* sprite = GetOrCreateCharSprite(ch);
        if (sprite)
        {
            sprite->Update();
        }
    }
}

void FontSprite::Draw()
{
    // 非表示なら描画しない
    if (!isVisible_) return;

    // 内部実装を呼び出し
    DrawTextInternal(text_, position_, scale_, spacing_);
}

void FontSprite::DrawChar(char character, const Vector2& position, float scale)
{
    Sprite* sprite = GetOrCreateCharSprite(character);
    if (!sprite) return;

    // 設定を適用
    sprite->SetPosition(position);
    sprite->SetSize({ cellSize_ * scale, cellSize_ * scale });
    sprite->SetColor(color_);
    sprite->SetRotation(rotation_);

    sprite->Update();
    sprite->Draw();
}

void FontSprite::DrawText(const std::string& text, const Vector2& position, float scale, float spacing)
{
    // 即座に描画（引数で指定された値を使用）
    DrawTextInternal(text, position, scale, spacing);
}

void FontSprite::DrawTextInternal(const std::string& text, const Vector2& position, float scale, float spacing)
{
    Vector2 currentPos = position;
    float charWidth = cellSize_ * scale + spacing;

    for (char ch : text)
    {
        // スペースは描画せずに位置だけ進める
        if (ch == ' ')
        {
            currentPos.x += charWidth;
            continue;
        }

        Sprite* sprite = GetOrCreateCharSprite(ch);
        if (sprite)
        {
            // 設定を適用
            sprite->SetPosition(currentPos);
            sprite->SetSize({ cellSize_ * scale, cellSize_ * scale });
            sprite->SetColor(color_);
            sprite->SetRotation(rotation_);

            sprite->Update();
            sprite->Draw();
        }

        currentPos.x += charWidth;
    }
}