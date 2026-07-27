#pragma once
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "IJsonEditable.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "JsonEditorImGuiUtils.h"
#endif
#include "math/Vector3.h"
#include <type_traits>
#include <vector>
#include "base/GraphicsTypes.h"
#include "JsonSerialization.h"
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
	 * @brief メモリ上でデータをJSONにシリアライズ
	 */
	nlohmann::json Serialize() const;

	/**
	 * @brief メモリ上のJSONからデシリアライズ
	 */
	void Deserialize(const nlohmann::json& json);

#ifdef USE_IMGUI
	/**
	 * @brief ImGuiによる編集UIを描画
	 */
	void DrawImGui() override;

	/**
	 * @brief 保存/読込ボタンを含むオプションUIを描画
	 */
	virtual void DrawOptions();
#else
	void DrawImGui() override {}
	virtual void DrawOptions() {}
#endif

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

	/**
	 * @brief デバッグ用メンバ変数を登録する（JSON保存対象外、ImGuiでのみ編集可能）
	 */
	template<typename T>
	void RegisterDebug(const std::string& name, T* value);

private:
	std::unordered_map<std::string, std::function<nlohmann::json()>> getters_;              // 値取得関数マップ
	std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> setters_;   // 値設定関数マップ
#ifdef USE_IMGUI
	std::unordered_map<std::string, std::function<void()>> drawers_;                        // ImGui描画関数マップ
#endif

	std::vector<std::string> registrationOrder_;                                             // 登録順序を保持するリスト
	std::unordered_map<std::string, bool> isComposite_;                                     // 複合型かどうかのフラグマップ
#ifdef USE_IMGUI
	ImGuiTextFilter filter_;                                                                // 検索用フィルター
#endif
	int expandCollapseTrigger_ = 0;                                                         // 一括展開/折りたたみ用トリガー（0:なし、1:展開、2:折りたたみ）

	std::vector<std::shared_ptr<void>> registeredMembers_; // 登録されたメンバ変数のポインタを保持
	const std::string dirPath = "Resources/json/";         // JSONファイルのディレクトリパス
	std::string fileName;                                   // JSONファイル名
};

// メンバ変数登録の自動化マクロ
#define REGISTER_MEMBER(var) Register(#var, &var)
#define REGISTER_MEMBER_DEBUG(var) RegisterDebug(#var, &var)

// 型チェックのためのヘルパー
template<typename> struct is_std_vector : std::false_type {};
template<typename U, typename A> struct is_std_vector<std::vector<U, A>> : std::true_type {};

template<typename T>
void JsonEditableBase::RegisterDebug(const std::string& name, T* value)
{
	// 重複登録を防止（isComposite_はUSE_IMGUI問わず存在する）
	if (isComposite_.count(name)) return;

	// 登録順序を保存
	registrationOrder_.push_back(name);

	// 複合型判定
	bool isComp = false;
	if constexpr (std::is_same_v<T, Transform> ||
		std::is_same_v<T, std::vector<Transform>> ||
		std::is_same_v<T, std::vector<Vector3>> ||
		std::is_same_v<T, std::vector<std::string>>)
	{
		isComp = true;
	}
	isComposite_[name] = isComp;

	// getters_ / setters_ には登録しない（シリアライズ非対象）

#ifdef USE_IMGUI
	// --- ImGui 描画関数登録 ---
	if constexpr (!std::is_same_v<T, Transform> &&
		!std::is_same_v<T, std::vector<Transform>> &&
		!std::is_same_v<T, std::vector<Vector3>> &&
		!std::is_same_v<T, std::vector<std::string>>)
	{
		drawers_[name] = [value, name]() {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s (Debug)", name.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::PushItemWidth(-FLT_MIN);
			ImGui::PushID(name.c_str());
			if constexpr (std::is_same_v<std::decay_t<T>, float>)
			{
				ImGui::DragFloat("##value", value, 0.1f);
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				ImGui::DragInt("##value", value);
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				ImGui::Checkbox("##value", value);
			}
			else if constexpr (std::is_same_v<T, Vector3>)
			{
				ImGui::DragFloat3("##value", &value->x, 0.1f);
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				char buf[256];
				strncpy_s(buf, value->c_str(), sizeof(buf));
				buf[sizeof(buf) - 1] = '\0';
				if (ImGui::InputText("##value", buf, sizeof(buf)))
				{
					*value = buf;
				}
			}
			ImGui::PopID();
			ImGui::PopItemWidth();
			};
	}
	else
	{
		drawers_[name] = [value, name, this]() {
			ImGui::PushID(name.c_str());
			if (expandCollapseTrigger_ == 1)
			{
				ImGui::SetNextItemOpen(true);
			}
			else if (expandCollapseTrigger_ == 2)
			{
				ImGui::SetNextItemOpen(false);
			}

			std::string debugLabel = name + " (Debug)";
			if (ImGui::TreeNodeEx(debugLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				if constexpr (std::is_same_v<T, Transform>)
				{
					if (ImGui::BeginTable("TransformTable", 2, ImGuiTableFlags_SizingStretchProp))
					{
						ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
						ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Translate");
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-FLT_MIN);
						ImGui::DragFloat3("##Translate", &value->translate.x, 0.1f);
						ImGui::PopItemWidth();

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Rotate");
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-FLT_MIN);
						ImGui::DragFloat3("##Rotate", &value->rotate.x, 0.01f);
						ImGui::PopItemWidth();

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Scale");
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-FLT_MIN);
						ImGui::DragFloat3("##Scale", &value->scale.x, 0.1f);
						ImGui::PopItemWidth();

						ImGui::EndTable();
					}
				}
				// 簡易化のため vector系は必要に応じて適宜描画
				ImGui::TreePop();
			}
			ImGui::PopID();
			};
	}
#endif // USE_IMGUI
}

template<typename T>
void JsonEditableBase::Register(const std::string& name, T* value)
{
	// 重複登録を防止
	if (getters_.count(name)) return;

	// 型名を出力
	KCE::Logger::Log("Register: " + name + " type: " + std::string(typeid(T).name()) + "\n");

	// 登録順序を保存
	registrationOrder_.push_back(name);

	// 複合型（ツリーノードで描画すべきか）判定
	bool isComp = false;
	if constexpr (std::is_same_v<T, Transform> ||
		std::is_same_v<T, std::vector<Transform>> ||
		std::is_same_v<T, std::vector<Vector3>> ||
		std::is_same_v<T, std::vector<std::string>>)
	{
		isComp = true;
	}
	isComposite_[name] = isComp;

	// nlohmann::jsonのto_json/from_jsonに委譲
	getters_[name] = [value]() {
		return nlohmann::json(*value);
		};
	setters_[name] = [value, name](const nlohmann::json& j) {
		try {
			j.get_to(*value);
		}
		catch (const std::exception& e) {
			KCE::Logger::Log("JSON parse error for key '" + name + "': " + std::string(e.what()) + "\n");
		}
		};

#ifdef USE_IMGUI
	// --- ImGui 描画関数登録 ---
	if constexpr (!std::is_same_v<T, Transform> &&
		!std::is_same_v<T, std::vector<Transform>> &&
		!std::is_same_v<T, std::vector<Vector3>> &&
		!std::is_same_v<T, std::vector<std::string>>)
	{
		// 基本データ型：テーブルレイアウト対応の描画処理
		drawers_[name] = [value, name]() {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", name.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::PushItemWidth(-FLT_MIN);
			ImGui::PushID(name.c_str());
			if constexpr (std::is_same_v<std::decay_t<T>, float>)
			{
				ImGui::DragFloat("##value", value, 0.1f);
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				ImGui::DragInt("##value", value);
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				ImGui::Checkbox("##value", value);
			}
			else if constexpr (std::is_same_v<T, Vector3>)
			{
				ImGui::DragFloat3("##value", &value->x, 0.1f);
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				char buf[256];
				strncpy_s(buf, value->c_str(), sizeof(buf));
				buf[sizeof(buf) - 1] = '\0';
				if (ImGui::InputText("##value", buf, sizeof(buf)))
				{
					*value = buf;
				}
			}
			ImGui::PopID();
			ImGui::PopItemWidth();
			};
	}
	else
	{
		// 複合データ型：ツリーノードによる個別階層描画
		drawers_[name] = [value, name, this]() {
			ImGui::PushID(name.c_str());
			if (expandCollapseTrigger_ == 1)
			{
				ImGui::SetNextItemOpen(true);
			}
			else if (expandCollapseTrigger_ == 2)
			{
				ImGui::SetNextItemOpen(false);
			}

			if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				if constexpr (std::is_same_v<T, Transform>)
				{
					if (ImGui::BeginTable("TransformTable", 2, ImGuiTableFlags_SizingStretchProp))
					{
						ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
						ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Translate");
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-FLT_MIN);
						ImGui::DragFloat3("##Translate", &value->translate.x, 0.1f);
						ImGui::PopItemWidth();

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Rotate");
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-FLT_MIN);
						ImGui::DragFloat3("##Rotate", &value->rotate.x, 0.01f);
						ImGui::PopItemWidth();

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("Scale");
						ImGui::TableSetColumnIndex(1);
						ImGui::PushItemWidth(-FLT_MIN);
						ImGui::DragFloat3("##Scale", &value->scale.x, 0.1f);
						ImGui::PopItemWidth();

						ImGui::EndTable();
					}
				}
				else if constexpr (std::is_same_v<T, std::vector<Transform>>)
				{
					for (size_t i = 0; i < value->size(); ++i)
					{
						std::string headerLabel = "Transform[" + std::to_string(i) + "]";
						ImGui::PushID(static_cast<int>(i));
						if (expandCollapseTrigger_ == 1) ImGui::SetNextItemOpen(true);
						else if (expandCollapseTrigger_ == 2) ImGui::SetNextItemOpen(false);
						if (ImGui::TreeNode(headerLabel.c_str()))
						{
							if (ImGui::BeginTable("VectorTransformTable", 2, ImGuiTableFlags_SizingStretchProp))
							{
								ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 100.0f);
								ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Translate");
								ImGui::TableSetColumnIndex(1);
								ImGui::PushItemWidth(-FLT_MIN);
								ImGui::DragFloat3("##Translate", &(*value)[i].translate.x, 0.1f);
								ImGui::PopItemWidth();

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Rotate");
								ImGui::TableSetColumnIndex(1);
								ImGui::PushItemWidth(-FLT_MIN);
								ImGui::DragFloat3("##Rotate", &(*value)[i].rotate.x, 0.01f);
								ImGui::PopItemWidth();

								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								ImGui::Text("Scale");
								ImGui::TableSetColumnIndex(1);
								ImGui::PushItemWidth(-FLT_MIN);
								ImGui::DragFloat3("##Scale", &(*value)[i].scale.x, 0.1f);
								ImGui::PopItemWidth();

								ImGui::EndTable();
							}
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					ImGui::Spacing();
					if (ImGui::Button("Add Transform"))
					{
						value->push_back(Transform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f} });
					}
					ImGui::SameLine();
					if (ImGui::Button("Remove Last") && !value->empty())
					{
						value->pop_back();
					}
				}
				else if constexpr (std::is_same_v<T, std::vector<Vector3>>)
				{
					for (size_t i = 0; i < value->size(); ++i)
					{
						std::string label = "Element[" + std::to_string(i) + "]";
						ImGui::PushID(static_cast<int>(i));
						ImGui::PushItemWidth(-FLT_MIN);
						ImGui::DragFloat3(label.c_str(), &(*value)[i].x, 0.1f);
						ImGui::PopItemWidth();
						ImGui::PopID();
					}
					ImGui::Spacing();
					if (ImGui::Button("Add Vector3"))
					{
						value->push_back(Vector3{ 0.0f, 0.0f, 0.0f });
					}
					ImGui::SameLine();
					if (ImGui::Button("Remove Last") && !value->empty())
					{
						value->pop_back();
					}
				}
				else if constexpr (std::is_same_v<T, std::vector<std::string>>)
				{
					for (size_t i = 0; i < value->size(); ++i)
					{
						std::string label = "Element[" + std::to_string(i) + "]";
						char buf[256];
						strncpy_s(buf, (*value)[i].c_str(), sizeof(buf));
						buf[sizeof(buf) - 1] = '\0';
						ImGui::PushID(static_cast<int>(i));
						if (ImGui::InputText(label.c_str(), buf, sizeof(buf)))
						{
							(*value)[i] = buf;
						}
						ImGui::PopID();
					}
					ImGui::Spacing();
					if (ImGui::Button("Add String"))
					{
						value->push_back("");
					}
					ImGui::SameLine();
					if (ImGui::Button("Remove Last") && !value->empty())
					{
						value->pop_back();
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
			};
	}
#endif // USE_IMGUI
}
