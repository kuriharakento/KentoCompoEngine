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

class JsonEditableBase : public IJsonEditable
{
public:
	JsonEditableBase() = default;
	~JsonEditableBase();
	bool LoadJson(const std::string& path) override;
	bool SaveJson(const std::string& path) const override;
	void DrawImGui() override;
	virtual void DrawOptions();

	void SetValue(const std::string& key, const nlohmann::json& value);

protected:
	template<typename T>
	// NOTE: 必ず変数は登録すること!! しないとエラーが出る。
	void Register(const std::string& name, T* value);

private:
	std::unordered_map<std::string, std::function<nlohmann::json()>> getters_;
	std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> setters_;
	std::unordered_map<std::string, std::function<void()>> drawers_;

	std::vector<std::shared_ptr<void>> registeredMembers_; // 登録されたメンバ変数のポインタを保持
	const std::string dirPath = "Resources/json/";
	std::string fileName;
};

// メンバ変数登録の自動化マクロ
#define REGISTER_MEMBER(var) Register(#var, &var)

// 型チェックのためのヘルパー
template<typename> struct is_std_vector : std::false_type {};
template<typename U, typename A> struct is_std_vector<std::vector<U, A>> : std::true_type {};

template<typename T>
void JsonEditableBase::Register(const std::string& name, T* value)
{
	if (getters_.count(name)) return;

	// 型名を出力
	Logger::Log("Register: " + name + " type: " + std::string(typeid(T).name()) + "\n");

	// シンプルに全体型に対して to_json/from_json を丸投げ
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