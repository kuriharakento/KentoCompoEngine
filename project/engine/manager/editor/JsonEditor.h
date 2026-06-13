#pragma once
#include <memory>
#include <unordered_map>
#include <string>

// editor
#include "jsonEditor/JsonEditableBase.h"

/**
 * @brief JSONエディタクラス
 * @details タブベースのJSONエディタUIを管理するシングルトンクラス
 *          複数のJsonEditableBaseを登録し、ImGuiでタブ形式で表示する
 */
class JsonEditor
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return JsonEditorのインスタンス
	 */
	static JsonEditor* GetInstance();

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
	 * @brief エディタの登録（生ポインタ版）
	 * @param name エディタの名前（タブに表示される）
	 * @param editor 登録するエディタのポインタ
	 */
	void Register(const std::string& name, JsonEditableBase* editor);

	/**
	 * @brief エディタの登録（shared_ptr版：互換性維持のため）
	 * @param name エディタの名前（タブに表示される）
	 * @param editor 登録するエディタのshared_ptr
	 */
	void Register(const std::string& name, std::shared_ptr<JsonEditableBase> editor);

	/**
	 * @brief エディタの登録解除
	 * @param editor 登録解除するエディタのポインタ
	 */
	void Unregister(JsonEditableBase* editor);

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
	// 登録されたエディタのマップ（名前 -> エディタ、非所有）
	std::unordered_map<std::string, JsonEditableBase*> editors_;

	// 互換性維持のため、shared_ptrで登録されたエディタの所有権を保持するマップ
	std::unordered_map<std::string, std::shared_ptr<JsonEditableBase>> sharedEditors_;

	// 現在選択されているアイテムの名前
	std::string selectedItem_;

	// 汎用JSON編集用のメンバ
	std::string rawJsonFileName_ = "new_data.json";
	std::string rawJsonContentStr_;

private: // シングルトンインスタンス
	static std::unique_ptr<JsonEditor> instance_;
	JsonEditor() = default;       // コンストラクタ
	JsonEditor(const JsonEditor&) = delete;            // コピーコンストラクタ
	JsonEditor& operator=(const JsonEditor&) = delete; // コピー代入禁止

public:
	~JsonEditor() = default;      // デストラクタ
};
