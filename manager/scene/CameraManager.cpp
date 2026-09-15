#include "CameraManager.h"
#include <iostream>

// system
#include "base/Logger.h"
#include "editor/SceneGizmo.h"
#include "editor/SceneViewContext.h"
#include "editor/command/CommandHistory.h"
#include "manager/graphics/LineManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"

#endif

namespace KCE
{
namespace
{
constexpr float kFrustumDepth = 3.0f;
constexpr float kCameraMarkRadius = 0.2f;
constexpr float kCameraDirectionLength = 1.0f;
constexpr Vector4 kCameraDebugColor = { 0.3f, 0.8f, 1.0f, 1.0f };

class CameraGizmoCommand final : public ICommand
{
public:
	CameraGizmoCommand(CameraManager* manager, std::string name, const Transform& before, const Transform& after, uint32_t dragId)
		: manager_(manager), name_(std::move(name)), before_(before), after_(after), dragId_(dragId) {}
	void Execute() override { Apply(after_); }
	void Undo() override { Apply(before_); }
	std::string GetName() const override { return "Move Camera"; }
	bool MergeWith(const ICommand* next) override
	{
		const auto* command = dynamic_cast<const CameraGizmoCommand*>(next);
		if (!command || command->manager_ != manager_ || command->name_ != name_ || command->dragId_ != dragId_) { return false; }
		after_ = command->after_;
		return true;
	}
private:
	void Apply(const Transform& transform)
	{
		if (Camera* camera = manager_ ? manager_->GetCamera(name_) : nullptr)
		{
			camera->SetTranslate(transform.translate);
			camera->SetRotate(transform.rotate);
		}
	}
	// Framework が所有し、履歴より長生きする
	CameraManager* manager_ = nullptr;
	std::string name_;
	Transform before_{};
	Transform after_{};
	uint32_t dragId_ = 0;
};
}

void CameraManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;

#ifdef USE_IMGUI
	// 一覧は Hierarchy、選んだカメラの詳細は Inspector に出す
	DebugUIManager::GetInstance()->RegisterHierarchySection(this, "カメラ", [this]() { this->DrawHierarchyImGui(); });
	DebugUIManager::GetInstance()->RegisterInspector(this, SelectionKind::Camera, [this](const SelectionItem& item) { this->DrawInspectorImGui(item); });
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "シーン", "カメラ", [this]() { this->DrawSettingsImGui(); });
	GizmoTarget target;
	target.getPose = [this](const SelectionItem& item, Matrix4x4& world, uint32_t& operations)
	{
		Camera* camera = GetCamera(item.name);
		if (!camera || (SceneViewContext::HasInstance() && camera == SceneViewContext::GetInstance()->GetCamera())) { return false; }
		world = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, camera->GetRotate(), camera->GetTranslate());
		operations = kGizmoTranslate | kGizmoRotate;
		return true;
	};
	target.apply = [this](const SelectionItem& item, const GizmoResult& after, uint32_t dragId)
	{
		Camera* camera = GetCamera(item.name);
		if (!camera) { return; }
		Transform before{ { 1.0f, 1.0f, 1.0f }, camera->GetRotate(), camera->GetTranslate() };
		Transform moved{ { 1.0f, 1.0f, 1.0f }, after.rotate.ToEuler(), after.translate };
		CommandHistory::GetInstance()->Execute(std::make_unique<CameraGizmoCommand>(this, item.name, before, moved, dragId));
	};
	SceneGizmo::GetInstance()->RegisterTarget(this, SelectionKind::Camera, std::move(target));
#endif
}

CameraManager::~CameraManager()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
	if (SceneGizmo::HasInstance()) { SceneGizmo::GetInstance()->Unregister(this); }
#endif
}

void CameraManager::AddCamera(const std::string& name) {
    // 名前で重複を避けてカメラを追加
    if (cameras_.find(name) == cameras_.end()) {
        // 新しいカメラをunique_ptrで生成してマップに追加
        cameras_[name] = std::make_unique<Camera>();
        KCE::Logger::Log("Add Camera: " + name + "\n");

		// GPU定数バッファを初期化
		if (dxCommon_)
		{
			cameras_[name]->InitializeConstantBuffer(dxCommon_);
		}
    }
}

bool CameraManager::RemoveCamera(const std::string& name)
{
	auto it = cameras_.find(name);
	if (it == cameras_.end() || it->second.get() == activeCamera_ || it->second.get() == renderCameraOverride_)
	{
		return false;
	}
	cameras_.erase(it);
	return true;
}

Camera* CameraManager::GetCamera(const std::string& name) {
    // 名前でカメラを検索
    auto it = cameras_.find(name);
    if (it != cameras_.end()) {
    	// 名前に対応するカメラが見つかった場合、そのポインタを返す
        return it->second.get();
    }
    // 見つからない場合はnullptrを返す
    return nullptr;
}

void CameraManager::SetActiveCamera(const std::string& name) {
    // 名前からカメラを取得
    Camera* camera = GetCamera(name);
    if (camera) {
		// アクティブカメラを設定
        activeCamera_ = camera;
		activeCameraName_ = name;
    } else {
        // カメラが見つからない場合は何もしない
    }
}

void CameraManager::Update() {
    // アクティブカメラが設定されていない場合は何もしない
    if(!activeCamera_)
    {
        return;
    }

    // アクティブカメラを更新
    activeCamera_->Update();
}

void CameraManager::DrawDebugLines()
{
#ifdef _DEBUG
	if (!drawDebugLines_) { return; }
	LineManager* lines = LineManager::GetInstance();
	const SelectionItem& selected = SelectionContext::GetInstance()->GetPrimary();
	Camera* viewCamera = SceneViewContext::HasInstance() ? SceneViewContext::GetInstance()->GetCamera() : nullptr;
	for (const auto& [name, cameraOwner] : cameras_)
	{
		Camera* camera = cameraOwner.get();
		if (camera == viewCamera) { continue; }
		camera->Update();
		const Matrix4x4& world = camera->GetWorldMatrix();
		const Vector3 origin = camera->GetTranslate();
		const Vector3 right{ world.m[0][0], world.m[0][1], world.m[0][2] };
		const Vector3 up{ world.m[1][0], world.m[1][1], world.m[1][2] };
		const Vector3 forward{ world.m[2][0], world.m[2][1], world.m[2][2] };
		lines->DrawLine(origin - right * kCameraMarkRadius, origin + right * kCameraMarkRadius, kCameraDebugColor);
		lines->DrawLine(origin - up * kCameraMarkRadius, origin + up * kCameraMarkRadius, kCameraDebugColor);
		const bool isSelected = selected.kind == SelectionKind::Camera && selected.name == name;
		if (!isSelected)
		{
			lines->DrawLine(origin, origin + forward * kCameraDirectionLength, kCameraDebugColor);
			continue;
		}
		const float halfHeight = std::tan(camera->GetFovY() * 0.5f) * kFrustumDepth;
		const float halfWidth = halfHeight * camera->GetAspectRatio();
		const Vector3 center = origin + forward * kFrustumDepth;
		const Vector3 corners[4] = { center + up * halfHeight - right * halfWidth, center + up * halfHeight + right * halfWidth,
			center - up * halfHeight + right * halfWidth, center - up * halfHeight - right * halfWidth };
		for (int i = 0; i < 4; ++i)
		{
			lines->DrawLine(origin, corners[i], kCameraDebugColor);
			lines->DrawLine(corners[i], corners[(i + 1) % 4], kCameraDebugColor);
		}
	}
#endif
}

#ifdef USE_IMGUI
void CameraManager::DrawHierarchyImGui()
{
	// 選ばれているかは今の選択と直接比べる（毎フレーム選択を作らない）
	const SelectionItem& primary = SelectionContext::GetInstance()->GetPrimary();
	for (const auto& [name, camera] : cameras_)
	{
		(void)camera;
		ImGui::PushID(name.c_str());
		const bool isSelected = primary.kind == SelectionKind::Camera && primary.name == name;
		if (ImGui::Selectable(name.c_str(), isSelected))
		{
			SelectionItem item;
			item.kind = SelectionKind::Camera;
			item.name = name;
			SelectionContext::GetInstance()->Select(item);
		}
		if (name == activeCameraName_)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(Active)");
		}
		ImGui::PopID();
	}

	// カメラの追加ボタン
	if (ImGui::Button("カメラを追加"))
	{
		AddCamera("camera" + std::to_string(cameras_.size()));
	}
}

void CameraManager::DrawInspectorImGui(const SelectionItem& item)
{
	// 選んだ後で消されていることがあるので、名前から引き直す
	Camera* camera = GetCamera(item.name);
	if (!camera)
	{
		ImGui::TextDisabled("このカメラは見つかりません");
		return;
	}

	ImGui::SeparatorText(item.name.c_str());
	if (item.name == activeCameraName_)
	{
		ImGui::TextUnformatted("Active Camera");
	}
	else if (ImGui::Button("このカメラを使う"))
	{
		SetActiveCamera(item.name);
	}

	// 触ったときだけ書き戻す。毎フレーム書くと、シーケンサーがクォータニオンで回したカメラをオイラー角に戻してしまう
	Vector3 cameraPosition = camera->GetTranslate();
	if (ImGui::DragFloat3("位置", &cameraPosition.x, 0.1f))
	{
		camera->SetTranslate(cameraPosition);
	}
	Vector3 cameraRotate = camera->GetRotate();
	if (ImGui::DragFloat3("回転", &cameraRotate.x, 0.01f, -3.14f, 3.14f))
	{
		camera->SetRotate(cameraRotate);
	}
}

void CameraManager::DrawSettingsImGui()
{
	ImGui::Checkbox("デバッグラインを表示", &drawDebugLines_);
}
#endif
} // namespace KCE
