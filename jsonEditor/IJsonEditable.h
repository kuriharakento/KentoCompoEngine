#pragma once
#include <string>

namespace KCE
{
/**
 * @brief JSON編集可能なオブジェクトのインターフェース
 * 
 * JSONファイルの読み書きとImGuiを使用した編集UIを提供するためのインターフェースです。
 * このインターフェースを継承することで、オブジェクトのプロパティを
 * JSONとしてシリアライズ/デシリアライズし、GUI上で編集可能にします。
 */
class IJsonEditable
{
protected:
    /**
     * @brief ImGuiによる編集UIを描画
     */
    virtual void DrawImGui() = 0;

    /**
     * @brief JSONファイルからデータを読み込む
     * @param path JSONファイルのパス
     * @return 読み込み成功時true
     */
    virtual bool LoadJson(const std::string& path) = 0;

    /**
     * @brief JSONファイルにデータを保存する
     * @param path JSONファイルのパス
     * @return 保存成功時true
     */
    virtual bool SaveJson(const std::string& path) const = 0;

    virtual ~IJsonEditable() = default;
};
} // namespace KCE
