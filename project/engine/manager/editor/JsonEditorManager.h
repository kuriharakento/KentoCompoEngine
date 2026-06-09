#pragma once
#include <memory>
#include <unordered_map>

// editor
#include "jsonEditor/JsonEditableBase.h"

/**
 * @brief JSONエディタマネージャークラス
 * @details タブベースのJSONエディタUIを管理するシングルトンクラス
 *          複数のJsonEditableBaseを登録し、ImGuiでタブ形式で表示する
 */
class JsonEditorManager
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return JsonEditorManagerのインスタンス
	 */
	static JsonEditorManager* GetInstance();

	/**
	 * @brief 初期化処理
	 */
	void Initialize();

	/**
	 * @brief 終了処理
	 * @details 登録されたエディタをクリアし、インスタンスを解放する
	 */
	void Finalize();

	/**
	 * @brief エディタの登録
	 * @param name エディタの名前（タブに表示される）
	 * @param editor 登録するエディタのshared_ptr
	 */
    void Register(const std::string& name, std::shared_ptr<JsonEditableBase> editor);

	/**
	 * @brief エディタUIの描画
	 * @details 登録されたすべてのエディタをタブ形式で表示する
	 */
	void RenderEditUI();

	/**
	 * @brief すべてのエディタの保存
	 */
    void SaveAll();

private:
	// 登録されたエディタのマップ（名前 -> エディタ）
    std::unordered_map<std::string, std::shared_ptr<JsonEditableBase>> editors_;
	// 現在選択されているアイテムの名前
	std::string selectedItem_;

	// 汎用JSON編集用のメンバ
	std::string rawJsonFileName_ = "new_data.json";
	std::string rawJsonContentStr_;

private: // シングルトンインスタンス
	static std::unique_ptr<JsonEditorManager> instance_;
	JsonEditorManager() = default;       // コンストラクタ
	JsonEditorManager(const JsonEditorManager&) = delete;            // コピーコンストラクタ
	JsonEditorManager& operator=(const JsonEditorManager&) = delete; // コピー代入禁止

public:
	~JsonEditorManager() = default;      // デストラクタ
};
