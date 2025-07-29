#pragma once
#include <memory>
#include <unordered_map>

// editor
#include "jsonEditor/JsonEditableBase.h"

class JsonEditorManager
{
public:
	static JsonEditorManager* GetInstance();
	void Initialize();
	void Finalize();
    void Register(const std::string& name, std::shared_ptr<JsonEditableBase> editor);
	void RenderEditUI();
    void SaveAll();

private:
    std::unordered_map<std::string, std::shared_ptr<JsonEditableBase>> editors_;
	std::string selectedItem_;

private: //シングルトンインスタンス
	static JsonEditorManager* instance_;
	JsonEditorManager() = default;
	~JsonEditorManager() = default;
	JsonEditorManager(const JsonEditorManager&) = delete;
	JsonEditorManager& operator=(const JsonEditorManager&) = delete;
};
