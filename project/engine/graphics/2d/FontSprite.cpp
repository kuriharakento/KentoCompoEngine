#include "FontSprite.h"
#include <fstream>
#include <cassert>
#include <iostream>
#include "externals/nlohmann/json.hpp"
#ifdef USE_IMGUI
#include "ImGui/imgui.h"
#endif // USE_IMGUI

using json = nlohmann::json;

void FontSprite::Initialize(SpriteCommon* spriteCommon, const std::string& fontName)
{
    assert(spriteCommon);
    spriteCommon_ = spriteCommon;

    // フォントファイル置き場（fontName を基にファイル名を構築）
    const std::string dirPath = "./Resources/fonts/";
    // フォント名から生成するアトラスPNGの接尾辞（例: <fontName>_atlas.png）
    const std::string atlasPath = "_atlas.png";
    // フォントメトリクスJSONの接尾辞（例: <fontName>_metrics.json）
    const std::string metricsPath = "_metrics.json";

    // フォント名のみを引数に与える想定：
    // atlasTexturePath_ = "./Resources/fonts/" + fontName + "_atlas.png"
    // jsonPath = "./Resources/fonts/" + fontName + "_metrics.json"
    // （ファイル名／命名規則を変える場合はここを変更してください）
    atlasTexturePath_ = dirPath + fontName + atlasPath;

    // フォントメトリクスJSONのパスを構築して読み込む
    const std::string jsonPath = dirPath + fontName + metricsPath;

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

    // JSON形式: { "A": { "x": 0, "y": 0, "w": 64, "h": 64, "u": 0.0, "v": 0.0, "uw": 0.0625, "vh": 0.0625 }, ... }
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

std::unique_ptr<Sprite> FontSprite::CreateSpriteForChar(char c)
{
    // メトリクス情報が存在しない文字は nullptr を返す
    auto it = charMetrics_.find(c);
    if (it == charMetrics_.end())
    {
        return nullptr;
    }

    // 新規スプライト作成
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon_, atlasTexturePath_);
    sprite->SetAnchorPoint({ 0.5f, 0.5f }); // 中心をアンカーポイントに設定

    const CharInfo& info = it->second;

    // テクスチャ座標を設定
    sprite->SetTextureLeftTop({ static_cast<float>(info.x), static_cast<float>(info.y) });
    sprite->SetTextureSize({ static_cast<float>(info.w), static_cast<float>(info.h) });
    sprite->SetSize({ static_cast<float>(info.w), static_cast<float>(info.h) });

    return sprite;
}

void FontSprite::EnsureSpritesForText(const std::string& text)
{
    // 必要サイズまで拡張（既存は再利用）
    if (indexSprites_.size() < text.size())
    {
        indexSprites_.resize(text.size());
    }

    for (size_t i = 0; i < text.size(); ++i)
    {
        char ch = text[i];

        // スペースは sprite を保持しない（nullptr のまま）
        if (ch == ' ')
        {
            indexSprites_[i].reset();
            continue;
        }

        // 既に index ごとの sprite があれば、そのまま再利用
        if (indexSprites_[i])
        {
            // 必要であればここでメトリクス一致チェックを行い、異なれば差し替える
            continue;
        }

        // まだ無ければ生成して格納
        indexSprites_[i] = CreateSpriteForChar(ch);
    }

    // もし現在の indexSprites_ が text より長ければ余分分を解放しておく（メモリ節約）
    if (indexSprites_.size() > text.size())
    {
        for (size_t i = text.size(); i < indexSprites_.size(); ++i)
        {
            indexSprites_[i].reset();
        }
        indexSprites_.resize(text.size());
    }
}

void FontSprite::SetText(const std::string& text)
{
    text_ = text;
    // 変更されたタイミングでスプライトを確保（または再利用）
    EnsureSpritesForText(text_);
}

void FontSprite::Update()
{
#ifdef USE_IMGUI
    // ImGui で FontSprite のプロパティを操作できるようにする
    // ウィンドウ名に atlas パスの末尾を含めて複数フォントが開けても衝突しないようにする
    std::string windowName = "FontSprite: ";
    windowName += atlasTexturePath_.empty() ? "default" : atlasTexturePath_;
    ImGui::Begin(windowName.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // Text 入力（簡易実装：内部バッファを用いる）
    static char textBuf[512] = "";
    static bool textBufInitialized = false;
    if (!textBufInitialized || text_ != textBuf)
    {
        strncpy_s(textBuf, text_.c_str(), sizeof(textBuf));
        textBuf[sizeof(textBuf) - 1] = '\0';
        textBufInitialized = true;
    }
    if (ImGui::InputText("Text", textBuf, sizeof(textBuf)))
    {
        text_ = std::string(textBuf);
        // ImGui 経由で文字列が変わったらスプライトを Ensure する
        EnsureSpritesForText(text_);
    }

    // 位置 / スケール / スペーシング / 回転
    ImGui::DragFloat2("Position", &position_.x, 1.0f);
    ImGui::DragFloat("Scale", &scale_, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Spacing", &spacing_, 0.1f, -200.0f, 200.0f);
    ImGui::DragFloat("Rotation(rad)", &rotation_, 0.01f, -3.14159f, 3.14159f);

    // 色（RGBA）
    ImGui::ColorEdit4("Color", &color_.x);

    // 表示フラグ
    ImGui::Checkbox("Visible", &isVisible_);

    // アトラスパス表示（読み取り専用）
    ImGui::TextWrapped("Atlas: %s", atlasTexturePath_.c_str());

    // キャッシュクリア（スプライト再生成用）
    if (ImGui::Button("Clear cached character sprites"))
    {
        indexSprites_.clear();
    }

    ImGui::End();
#endif // USE_IMGUI


    // 表示する文字列に含まれる文字のスプライトのみ更新
    for (size_t i = 0; i < text_.size() && i < indexSprites_.size(); ++i)
    {
        char ch = text_[i];
        if (ch == ' ') continue; // スペースはスキップ

        Sprite* sprite = indexSprites_[i].get();
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
    // 一時スプライトで描画（キャッシュを使わない）
    auto tmp = CreateSpriteForChar(character);
    if (!tmp) return;

    Sprite* sprite = tmp.get();

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
    // Ensure してから描画（text_ は変更しない）
    EnsureSpritesForText(text);
    DrawTextInternal(text, position, scale, spacing);
}

void FontSprite::DrawTextInternal(const std::string& text, const Vector2& position, float scale, float spacing)
{
    Vector2 currentPos = position;
    float charWidth = cellSize_ * scale + spacing;

    for (size_t i = 0; i < text.size(); ++i)
    {
        char ch = text[i];

        // スペースは描画せずに位置だけ進める
        if (ch == ' ')
        {
            currentPos.x += charWidth;
            continue;
        }

        // indexSprites_ の該当スプライトを使う（Ensure済みであることが前提）
        if (i >= indexSprites_.size()) continue;
        Sprite* sprite = indexSprites_[i].get();
        if (sprite)
        {
            auto it = charMetrics_.find(ch);
            if (it != charMetrics_.end())
            {
                const CharInfo& info = it->second;
                sprite->SetTextureLeftTop({ static_cast<float>(info.x), static_cast<float>(info.y) });
                sprite->SetTextureSize({ static_cast<float>(info.w), static_cast<float>(info.h) });
                sprite->SetSize({ static_cast<float>(info.w), static_cast<float>(info.h) });
            }

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