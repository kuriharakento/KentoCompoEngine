#pragma once
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "IJsonEditable.h"
#include "imgui/imgui.h"
#include "math/Vector3.h"
#include <type_traits>
#include <vector>
#include "base/GraphicsTypes.h"
#include "JsonSerialization.h"
#include "JsonEditorImGuiUtils.h"
#include "base/Logger.h"

/* NOTE:
 * NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE マクロを使用して、構造体、クラスのシリアライズ・デシリアライズを自動生成する場合
 * 変数名は必ずJsonで定義した名前と一致させること。
 */

/**
 * @brief JSON編集可能な基底クラス
 * 
 * IJsonEditableインターフェースの実装を提供し、リフレクション的な仕組みで
 * メンバ変数の登録・シリアライズ・デシリアライズ・ImGui編集を自動化します。
 * 
 * 使用方法:
 * 1. このクラスを継承
 * 2. コンストラクタでREGISTER_MEMBERマクロを使用してメンバ変数を登録
 * 3. DrawImGui()でSetttingsセクションが自動生成される
 */
class JsonEditableBase : public IJsonEditable
{
public:
	JsonEditableBase() = default;
	~JsonEditableBase();

	/**
	 * @brief JSONファイルからデータを読み込む
	 * @param path JSONファイルのパス
	 * @return 読み込み成功時true
	 */
	bool LoadJson(const std::string& path) override;

	/**
	 * @brief JSONファイルにデータを保存する
	 * @param path JSONファイルのパス
	 * @return 保存成功時true
	 */
	bool SaveJson(const std::string& path) const override;

	/**
	 * @brief ImGuiによる編集UIを描画
	 */
	void DrawImGui() override;

	/**
	 * @brief 保存/読込ボタンを含むオプションUIを描画
	 */
	virtual void DrawOptions();

	/**
	 * @brief 指定キーに対応する値をJSONから設定
	 * @param key プロパティのキー名
	 * @param value 設定するJSON値
	 */
	void SetValue(const std::string& key, const nlohmann::json& value);

	/**
	 * @brief JSONファイル名を設定
	 * @param name ファイル名
	 */
	void SetFileName(const std::string& name) { fileName = name; }

protected:
	/**
	 * @brief メンバ変数を登録する
	 * 
	 * 登録されたメンバ変数は自動的にシリアライズ/デシリアライズ対象となり、
	 * ImGuiでの編集UIも自動生成されます。
	 * 
	 * @tparam T 登録する変数の型
	 * @param name 変数名（JSONのキー名として使用）
	 * @param value 変数へのポインタ
	 */
	template<typename T>
	// NOTE: 必ず変数は登録すること!! しないとエラーが出る。
	void Register(const std::string& name, T* value);

private:
	std::unordered_map<std::string, std::function<nlohmann::json()>> getters_;              // 値取得関数マップ
	std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> setters_;   // 値設定関数マップ
	std::unordered_map<std::string, std::function<void()>> drawers_;                        // ImGui描画関数マップ

	std::vector<std::shared_ptr<void>> registeredMembers_; // 登録されたメンバ変数のポインタを保持
	const std::string dirPath = "Resources/json/";         // JSONファイルのディレクトリパス
	std::string fileName;                                   // JSONファイル名
};

// メンバ変数登録の自動化マクロ
#define REGISTER_MEMBER(var) Register(#var, &var)

// 型チェックのためのヘルパー
template<typename> struct is_std_vector : std::false_type {};
template<typename U, typename A> struct is_std_vector<std::vector<U, A>> : std::true_type {};

template<typename T>
void JsonEditableBase::Register(const std::string& name, T* value)
{
	// 重複登録を防止
	if (getters_.count(name)) return;

	// 型名を出力
	Logger::Log("Register: " + name + " type: " + std::string(typeid(T).name()) + "\n");

	// nlohmann::jsonのto_json/from_jsonに委譲
	getters_[name] = [value]() {
		return nlohmann::json(*value);
		};
	setters_[name] = [value](const nlohmann::json& j) {
		j.get_to(*value);
		};

	// --- ImGui 描画関数登録 ---
	drawers_[name] = [value, name]() {
		ImGui::PushID(name.c_str());
		if (ImGui::CollapsingHeader(name.c_str()))
		{
			// 型ごとの描画関数に委譲
			if constexpr (std::is_same_v<T, float>)
				DrawImGuiForFloat(name, value);
			else if constexpr (std::is_same_v<T, int>)
				DrawImGuiForInt(name, value);
			else if constexpr (std::is_same_v<T, bool>)
				DrawImGuiForBool(name, value);
			else if constexpr (std::is_same_v<T, Vector3>)
				DrawImGuiForVector3(name, value);
			else if constexpr (std::is_same_v<T, Transform>)
				DrawImGuiForTransform(name, value);
			else if constexpr (std::is_same_v<T, std::vector<Transform>>)
				DrawImGuiForTransformVector(name, value);
			else if constexpr (std::is_same_v<T, std::vector<Vector3>>)
				DrawImGuiForVector3Vector(name, value);
			else if constexpr (std::is_same_v<T, std::string>)
				DrawImGuiForString(name, value);
			else if constexpr (std::is_same_v<T, std::vector<std::string>>)
				DrawImGuiForStringVector(name, value);
			else
				Logger::Log("Unsupported type for ImGui drawing: " + std::string(typeid(T).name()) + "\n");
		}
		ImGui::PopID();
		};
}