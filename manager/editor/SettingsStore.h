#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace KCE
{
/**
 * @brief Settings の値のうち、ファイルに保存しておく物を受け持つ。
 *
 * - 持ち主は、保存したい項目を RegisterItem で登録する（キー、今の値を JSON にする関数、JSON から値を戻す関数）
 * - 保存先は Resources/json/settings/editor_settings.json。起動時に読み、登録された項目へ復元する
 * - 登録の時点でファイルに値があれば、その場で復元する。持ち主が初期化された後に読み込む必要は無い
 * - 保存は、Settings ウィンドウの「設定を保存」ボタンで行う（勝手には書き換えない）
 * - 保存先に無い項目は、既定値のまま。ファイルに残っている未登録のキーは、保存で消さない
 */
class SettingsStore
{
public:
	static SettingsStore* GetInstance();
	static bool HasInstance();

	/** @brief 保存ファイルを読む。DebugUIManager の初期化の後、各持ち主の初期化より前に呼ぶ */
	void Initialize();
	/** @brief 登録を全部外して片付ける */
	void Finalize();

	/**
	 * @brief 保存する項目を登録する
	 * @param owner 登録元。Unregister で外すときの鍵。所有しない
	 * @param key 保存のキー（"fog" など）。登録元の間で重ならないこと
	 * @param serialize 今の値を JSON にする
	 * @param deserialize JSON から値を戻す。形が違う JSON でも落ちないこと
	 */
	void RegisterItem(void* owner, const std::string& key,
		std::function<nlohmann::json()> serialize, std::function<void(const nlohmann::json&)> deserialize);
	/** @brief owner が登録した項目を全部外す。持ち主が先に壊れるときに呼ぶ */
	void Unregister(void* owner);

	/** @brief 登録された項目を、保存ファイルへ書く */
	bool Save();
	/** @brief 保存ファイルの値を、登録された項目へ戻す */
	bool Reload();
	/** @return 最後に保存（か読み込み）してから、値が変わっていれば真 */
	bool IsDirty() const;
	/** @return 保存の結果などの、画面に出す短い文 */
	const std::string& GetStatusMessage() const { return statusMessage_; }

	~SettingsStore() = default;

private:
	static std::unique_ptr<SettingsStore> instance_;
	friend std::unique_ptr<SettingsStore> std::make_unique<SettingsStore>();

	SettingsStore() = default;
	SettingsStore(const SettingsStore&) = delete;
	SettingsStore& operator=(const SettingsStore&) = delete;

	struct Item
	{
		void* owner = nullptr;
		std::string key;
		std::function<nlohmann::json()> serialize;
		std::function<void(const nlohmann::json&)> deserialize;
	};

	static std::string GetFilePath();
	/** @brief 今の値を、1つの JSON にまとめる（ファイルにあって未登録のキーも残す） */
	nlohmann::ordered_json Collect() const;

	std::vector<Item> items_;
	// ファイルから読んだ内容。登録の時点の復元と、未登録のキーを残すのに使う
	nlohmann::ordered_json loaded_ = nlohmann::ordered_json::object();
	// 最後に保存（か読み込み）した時点の、Collect の結果。IsDirty が比べる
	nlohmann::ordered_json savedSnapshot_ = nlohmann::ordered_json::object();
	std::string statusMessage_;
};
} // namespace KCE
