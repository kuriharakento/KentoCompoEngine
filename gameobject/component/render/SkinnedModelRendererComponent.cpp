#include "SkinnedModelRendererComponent.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <vector>

// app
#include "engine/gameobject/base/GameObject.h"
// system
#include "base/PathManager.h"
#include "graphics/3d/Object3dCommon.h"
#include "time/TimeManager.h"
#include "externals/imgui/imgui.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
REGISTER_RENDERER_COMPONENT(SkinnedModelRendererComponent)

namespace GameObjectComponent
{
namespace
{
constexpr const char* kSkinnedModelExtension = ".gltf";

/** @brief Resources/models 直下の <名前>/<名前>.gltf を探して、モデル名の一覧を作る */
std::vector<std::string> ScanSkinnedModelNames()
{
	namespace fs = std::filesystem;
	std::set<std::string> names;
	const fs::path roots[] = {
		PathManager::GetApplicationResourceRoot() / "models",
		fs::path("Resources/models"),
	};
	for (const fs::path& root : roots)
	{
		std::error_code ec;
		if (!fs::is_directory(root, ec))
		{
			continue;
		}
		for (const fs::directory_entry& entry : fs::directory_iterator(root, ec))
		{
			if (!entry.is_directory(ec))
			{
				continue;
			}
			const std::string name = entry.path().filename().string();
			if (fs::exists(entry.path() / (name + kSkinnedModelExtension), ec))
			{
				names.insert(name);
			}
		}
	}
	return { names.begin(), names.end() };
}
} // namespace

void ForceLinkSkinnedModelRendererComponent() {}

SkinnedModelRendererComponent::SkinnedModelRendererComponent()
{
	Register("model", &modelName_);
	Register("animation", &animationName_);
	Register("loop", &loop_);
	Register("speed", &speed_);
	Register("castShadow", &castShadow_);

#ifdef USE_IMGUI
	// モデルは見つけた一覧から、アニメーションは読み込んだモデルの持つクリップから選ぶ
	OverrideDrawer("model", [this]()
	{
		static std::vector<std::string> names;
		static bool scanned = false;
		if (!scanned)
		{
			names = ScanSkinnedModelNames();
			scanned = true;
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("model");
		ImGui::TableSetColumnIndex(1);
		ImGui::PushItemWidth(-FLT_MIN);
		ImGui::PushID("model");
		if (ImGui::BeginCombo("##value", modelName_.empty() ? "（なし）" : modelName_.c_str()))
		{
			if (ImGui::Selectable("（再スキャン）", false))
			{
				scanned = false;
			}
			for (const std::string& name : names)
			{
				if (ImGui::Selectable(name.c_str(), name == modelName_))
				{
					// 実際の差し替えは、UpdateRenderTransform の ApplySettings で行う
					modelName_ = name;
					animationName_.clear();
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Resources/models/<名前>/<名前>.gltf のモデルから選ぶ");
		}
		ImGui::PopID();
		ImGui::PopItemWidth();
	});

	OverrideDrawer("animation", [this]()
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("animation");
		ImGui::TableSetColumnIndex(1);
		ImGui::PushItemWidth(-FLT_MIN);
		ImGui::PushID("animation");
		if (ImGui::BeginCombo("##value", animationName_.empty() ? "（なし）" : animationName_.c_str()))
		{
			if (ImGui::Selectable("（なし）", animationName_.empty()))
			{
				animationName_.clear();
			}
			if (skinned_ && skinned_->GetModel())
			{
				for (const AnimationClip& clip : skinned_->GetModel()->GetAnimations())
				{
					if (ImGui::Selectable(clip.name.c_str(), clip.name == animationName_))
					{
						animationName_ = clip.name;
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
		ImGui::PopItemWidth();
	});
#endif
}

void SkinnedModelRendererComponent::Awake()
{
	GameObject* owner = GetOwner();
	if (!owner)
	{
		return;
	}
	Object3dCommon* common = owner->GetObject3dCommon();
	if (!common)
	{
		return;
	}

	skinned_ = std::make_unique<SkinnedObject3d>();
	skinned_->Initialize(common, common->GetDefaultCamera());
	// Deferred は G-Buffer を描かないシーンだと映らない。どのシーンでも映る Forward にする
	skinned_->SetRenderingType(RenderingType::Forward);
	if (LightManager* lightManager = owner->GetLightManager())
	{
		skinned_->SetLightManager(lightManager);
	}
	ApplySettings();
}

void SkinnedModelRendererComponent::Update()
{
	pendingDelta_ += GetDeltaTime();
	lastUpdateFrame_ = TimeManager::GetInstance().GetFrameCount();
}

void SkinnedModelRendererComponent::SetModel(const std::string& modelName)
{
	modelName_ = modelName;
	ApplySettings();
}

void SkinnedModelRendererComponent::SetAnimation(const std::string& animationName, bool loop)
{
	animationName_ = animationName;
	loop_ = loop;
	ApplySettings();
}

void SkinnedModelRendererComponent::SetSpeed(float speed)
{
	speed_ = speed;
	ApplySettings();
}

void SkinnedModelRendererComponent::SetColor(const Vector4& color)
{
	if (skinned_)
	{
		skinned_->SetColor(color);
	}
}

void SkinnedModelRendererComponent::ApplySettings()
{
	if (!skinned_)
	{
		return;
	}
	// プレハブから読むときは Awake の後に値が入るので、変わったときだけ入れ直す
	const bool modelChanged = appliedModelName_ != modelName_;
	if (modelChanged)
	{
		if (modelName_.empty())
		{
			skinned_->SetModel(std::unique_ptr<SkinnedModel>());
		}
		else
		{
			skinned_->SetModel(modelName_, kSkinnedModelExtension);
		}
		appliedModelName_ = modelName_;
	}
	if (modelChanged || appliedAnimationName_ != animationName_ || appliedLoop_ != loop_)
	{
		if (animationName_.empty())
		{
			skinned_->StopAnimation();
		}
		else
		{
			skinned_->PlayAnimation(animationName_, loop_);
		}
		appliedAnimationName_ = animationName_;
		appliedLoop_ = loop_;
		// 再生の作り直しで速度が戻るので、入れ直す
		appliedSpeed_ = -1.0f;
	}
	if (appliedSpeed_ != speed_)
	{
		skinned_->GetAnimator()->SetPlaybackSpeed(speed_);
		appliedSpeed_ = speed_;
	}
}

void SkinnedModelRendererComponent::UpdateRenderTransform()
{
	if (!skinned_)
	{
		return;
	}
	ApplySettings();

	GameObject* owner = GetOwner();
	if (!owner)
	{
		return;
	}
	skinned_->SetTranslate(owner->GetPosition());
	skinned_->SetRotate(owner->GetRotation());
	skinned_->SetScale(owner->GetScale());

	// アニメーションはフレームに1回だけ、ためた時間で進める
	const float deltaTime = pendingDelta_;
	pendingDelta_ = 0.0f;
	skinned_->Update(deltaTime, nullptr);

	// 持ち主が子なら、持ち主のワールド行列をそのまま使う。根なら Update が自分で組んだ行列を使う
	if (owner->GetParent())
	{
		skinned_->UpdateMatrixWithWorld(owner->GetRenderWorldMatrix(), nullptr);
		skinned_->GetAnimator()->SetWorldMatrix(skinned_->GetWorldMatrix());
	}
}

#ifdef USE_IMGUI
void SkinnedModelRendererComponent::DrawImGui()
{
	// model / animation / loop / speed / castShadow は登録した値から出る
	JsonEditableBase::DrawImGui();

	ImGui::SeparatorText("プレビュー");
	Animator* animator = skinned_ ? skinned_->GetAnimator() : nullptr;
	const AnimationClip* clip = animator ? animator->GetCurrentClip() : nullptr;
	if (!clip)
	{
		ImGui::TextDisabled("モデルとアニメーションを選ぶと、ここで確認できる");
		previewPlaying_ = false;
		return;
	}

	if (ImGui::Button(previewPlaying_ ? "一時停止" : "再生"))
	{
		previewPlaying_ = !previewPlaying_;
		// ループしないクリップが終わっていたら、頭から始め直す
		if (previewPlaying_ && !animator->IsPlaying())
		{
			skinned_->PlayAnimation(animationName_, loop_);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("先頭へ"))
	{
		previewPlaying_ = false;
		animator->Seek(0.0f);
	}

	// 更新が止まっているエディタでは Update が時間を渡さないので、プレビューが代わりに進める
	if (previewPlaying_ && lastUpdateFrame_ != TimeManager::GetInstance().GetFrameCount())
	{
		pendingDelta_ += ImGui::GetIO().DeltaTime;
	}

	float time = animator->GetCurrentTime();
	if (ImGui::SliderFloat("時刻", &time, 0.0f, clip->duration, "%.2f 秒"))
	{
		previewPlaying_ = false;
		animator->Seek(time);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("ドラッグでアニメーションの任意の時刻へ。長さ %.2f 秒", clip->duration);
	}
}
#endif
} // namespace GameObjectComponent
} // namespace KCE
