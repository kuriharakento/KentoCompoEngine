#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "imgui/imgui.h"
#include "base/GraphicsTypes.h"

/**
 * @brief float型変数のImGui編集UIを描画します。
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
void DrawImGuiForFloat(const std::string& name, float* value);
/**
 * @brief int型変数のImGui編集UIを描画します。
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
void DrawImGuiForInt(const std::string& name, int* value);
/**
 * @brief bool型変数のImGuiチェックボックスUIを描画します。
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
void DrawImGuiForBool(const std::string& name, bool* value);
/**
 * @brief Vector3型（3次元ベクトル）のImGui編集UIを描画します。
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
void DrawImGuiForVector3(const std::string& name, Vector3* value);
/**
 * @brief Transform型（平行移動・回転・スケール）のImGui編集UIを描画します。
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
void DrawImGuiForTransform(const std::string& name, Transform* value);
/**
 * @brief std::vector<Vector3>型のImGui編集UIを描画します。
 *        各要素ごとにVector3編集UIを表示し、要素の追加・削除も可能です。
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
void DrawImGuiForVector3Vector(const std::string& name, std::vector<Vector3>* value);
/**
 * @brief std::vector<Transform>型のImGui編集UIを描画します。
 *        各要素ごとにTransform編集UIを表示し、要素の追加・削除も可能です。
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
void DrawImGuiForTransformVector(const std::string& name, std::vector<Transform>* value);
/**
 * @brief std::string型変数のImGui編集UIを描画します。
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
void DrawImGuiForString(const std::string& name, std::string* value);
/**
 * @brief std::vector<std::string>型のImGui編集UIを描画します。
 *        各要素ごとに文字列編集UIを表示し、要素の追加・削除も可能です。
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
void DrawImGuiForStringVector(const std::string& name, std::vector<std::string>* value);
/**
 * @brief 任意型Tの値をJSONとしてImGui上で直接編集できるUIを描画します。（テンプレート）
 *        値はnlohmann::jsonでシリアライズ＆デシリアライズされます。
 *        型Tはnlohmann::jsonのget/set対応型である必要があります。
 *
 * @tparam T 編集する型（nlohmann::jsonで変換可能な型）
 * @param name ラベル文字列
 * @param value 編集対象の値へのポインタ
 */
template<typename T>
void DrawImGuiForRawJson(const std::string& name, T* value)
{
	std::string s = nlohmann::json(*value).dump(2);
	std::vector<char> buf(s.begin(), s.end());
	buf.push_back('\0');
	if (ImGui::InputTextMultiline(
		name.c_str(), buf.data(), buf.size(),
		ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8),
		ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_AlwaysOverwrite))
	{
		try
		{
			auto jj = nlohmann::json::parse(buf.data());
			*value = jj.get<T>();
		}
		catch (...) {}
	}
}