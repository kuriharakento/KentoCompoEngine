#include "ModelRendererComponent.h"

// app
#include "engine/gameobject/base/GameObject.h"
// system
#include <algorithm>
#include "base/Logger.h"
#include "graphics/3d/Model.h"
#include "graphics/3d/Object3dCommon.h"
#include "manager/graphics/ModelManager.h"
#include "manager/graphics/TextureManager.h"
#include "externals/imgui/imgui.h"
// math
#include "math/MatrixFunc.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
REGISTER_RENDERER_COMPONENT(ModelRendererComponent)

namespace GameObjectComponent
{
	void ForceLinkModelRendererComponent() {}

	ModelRendererComponent::ModelRendererComponent()
	{
		// 空のままだと何も映らないので、本体の既定と同じ cube で始める
		modelName_ = "cube";
		Register("model", &modelName_);
		Register("castShadow", &castShadow_);
		Register("enableLighting", &enableLighting_);
		Register("meshTextures", &meshTextures_);

#ifdef USE_IMGUI
		// モデル名は文字入力ではなく、読み込み済みのモデルから選ぶ
		OverrideDrawer("model", [this]()
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("model");
			ImGui::TableSetColumnIndex(1);
			ImGui::PushItemWidth(-FLT_MIN);
			ImGui::PushID("model");
			if (ImGui::BeginCombo("##value", modelName_.empty() ? "（なし）" : modelName_.c_str()))
			{
				for (const std::string& name : ModelManager::GetInstance()->GetLoadedModelNames())
				{
					if (ImGui::Selectable(name.c_str(), name == modelName_))
					{
						// 実際の差し替えは、UpdateRenderTransform の ApplySettings で行う
						if (name != modelName_)
						{
							// 新しいモデルは、本来のテクスチャから始まる
							meshTextures_.clear();
						}
						modelName_ = name;
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("読み込み済みのモデルから選ぶ。Project ウィンドウで読み込める");
			}
			ImGui::PopID();
			ImGui::PopItemWidth();
		});
			// メッシュごとのテクスチャは、配列の編集ではなく、メッシュごとのプルダウンで選ばせる
			OverrideDrawer("meshTextures", [this]()
			{
				Model* model = object3d_ ? object3d_->GetModel() : nullptr;
				if (!model || model->GetMeshCount() == 0)
				{
					return;
				}
				const auto textures = TextureManager::GetInstance()->GetLoadedTextures();
				ImGui::SeparatorText("メッシュのテクスチャ");
				for (size_t meshIndex = 0; meshIndex < model->GetMeshCount(); ++meshIndex)
				{
					const std::string current = meshIndex < meshTextures_.size() ? meshTextures_[meshIndex] : std::string();
					const std::string label = model->GetMeshCount() == 1 ? std::string("テクスチャ") : "テクスチャ " + std::to_string(meshIndex + 1);
					ImGui::PushItemWidth(-150.0f);
					ImGui::PushID(static_cast<int>(meshIndex));
					if (ImGui::BeginCombo(label.c_str(), current.empty() ? "（モデルのまま）" : current.c_str()))
					{
						if (ImGui::Selectable("（モデルのまま）", current.empty()))
						{
							SetMeshTexture(meshIndex, std::string());
						}
						for (const auto& texture : textures)
						{
							// キューブマップなど、2D でない物は出さない
							if (!texture.previewable) { continue; }
							if (ImGui::Selectable(texture.path.c_str(), texture.path == current))
							{
								SetMeshTexture(meshIndex, texture.path);
							}
							// 選ぶ前に中身が分かるよう、カーソルを置くとプレビューを出す
							if (ImGui::IsItemHovered() && texture.gpuHandle != 0 && texture.width > 0 && texture.height > 0)
							{
								const float scale = (std::min)(192.0f / static_cast<float>(texture.width), 192.0f / static_cast<float>(texture.height));
								ImGui::BeginTooltip();
								ImGui::Image(static_cast<ImTextureID>(texture.gpuHandle), ImVec2(static_cast<float>(texture.width) * scale, static_cast<float>(texture.height) * scale));
								ImGui::TextDisabled("%u x %u", texture.width, texture.height);
								ImGui::EndTooltip();
							}
						}
						ImGui::EndCombo();
					}
					ImGui::PopID();
					ImGui::PopItemWidth();
				}
			});
#endif
	}

	void ModelRendererComponent::Awake()
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

		object3d_ = std::make_unique<Object3d>();
		object3d_->Initialize(common, common->GetDefaultCamera());
		// Deferred は G-Buffer を描かないシーンだと映らない。どのシーンでも映る Forward にする
		object3d_->SetRenderingType(RenderingType::Forward);
		if (LightManager* lightManager = owner->GetLightManager())
		{
			object3d_->SetLightManager(lightManager);
		}
		ApplySettings();
	}

	void ModelRendererComponent::SetModel(const std::string& modelName)
	{
		if (modelName != modelName_)
		{
			// 新しいモデルは、本来のテクスチャから始まる
			meshTextures_.clear();
		}
		modelName_ = modelName;
		ApplySettings();
	}

	void ModelRendererComponent::SetMeshTexture(size_t meshIndex, const std::string& texturePath)
	{
		if (meshTextures_.size() <= meshIndex)
		{
			meshTextures_.resize(meshIndex + 1);
		}
		meshTextures_[meshIndex] = texturePath;
		// 全部が本来のテクスチャなら、覚えをやめる（保存に余計な項目を書かない）
		if (std::all_of(meshTextures_.begin(), meshTextures_.end(), [](const std::string& path) { return path.empty(); }))
		{
			meshTextures_.clear();
		}
		ApplySettings();
	}

	void ModelRendererComponent::SetEnableLighting(bool enable)
	{
		enableLighting_ = enable;
		ApplySettings();
	}

	void ModelRendererComponent::SetCastShadow(bool cast)
	{
		castShadow_ = cast;
		ApplySettings();
	}

	void ModelRendererComponent::SetColor(const Vector4& color)
	{
		if (object3d_)
		{
			object3d_->SetColor(color);
		}
	}

	void ModelRendererComponent::ApplySettings()
	{
		if (!object3d_)
		{
			return;
		}
		// プレハブから読むときは Awake の後に値が入るので、変わったときだけ入れ直す
		if (appliedModelName_ != modelName_)
		{
			if (!modelName_.empty())
			{
				object3d_->SetModel(modelName_);
			}
			appliedModelName_ = modelName_;
			// 新しいモデルは本来のテクスチャなので、保存した替えを、全部入れ直す
			appliedMeshTextures_.clear();
			appliedEnableLighting_ = true;
		}
		// メッシュごとのテクスチャ。変わったメッシュだけ入れ直す
		if (Model* model = object3d_->GetModel())
		{
			const size_t count = (std::max)(meshTextures_.size(), appliedMeshTextures_.size());
			appliedMeshTextures_.resize(count);
			for (size_t meshIndex = 0; meshIndex < count; ++meshIndex)
			{
				const std::string wanted = meshIndex < meshTextures_.size() ? meshTextures_[meshIndex] : std::string();
				if (appliedMeshTextures_[meshIndex] == wanted)
				{
					continue;
				}
				// 読み込まれていないテクスチャなどで失敗しても、毎フレームやり直さないよう、反映済みにする
				if (!model->SetMeshTexture(meshIndex, wanted))
				{
					Logger::Log("ModelRendererComponent: テクスチャを替えられません: " + wanted + "\n", Logger::LogLevel::Warning);
				}
				appliedMeshTextures_[meshIndex] = wanted;
			}
		}
		// ライトの影響。モデルがあるときだけ反映する（無いまま済みにすると、モデルが入ったときに反映されない）
		if (appliedEnableLighting_ != enableLighting_ && object3d_->GetModel())
		{
			object3d_->SetEnableLighting(enableLighting_);
			appliedEnableLighting_ = enableLighting_;
		}
		if (appliedCastShadow_ != castShadow_)
		{
			object3d_->SetCastShadow(castShadow_);
			appliedCastShadow_ = castShadow_;
		}
	}

	void ModelRendererComponent::UpdateRenderTransform()
	{
		if (!object3d_)
		{
			return;
		}
		ApplySettings();

		GameObject* owner = GetOwner();
		if (!owner)
		{
			return;
		}
		object3d_->SetTranslate(owner->GetPosition());
		object3d_->SetRotate(owner->GetRotation());
		object3d_->SetScale(owner->GetScale());

		// 持ち主が子なら、持ち主のワールド行列をそのまま使う。根なら本体と同じく自分で組ませる
		if (owner->GetParent())
		{
			object3d_->UpdateMatrixWithWorld(owner->GetRenderWorldMatrix(), nullptr);
		}
		else
		{
			object3d_->Update(0.0f, nullptr);
		}
	}
} // namespace GameObjectComponent
} // namespace KCE
