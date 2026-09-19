#include "ModelRendererComponent.h"

// app
#include "engine/gameobject/base/GameObject.h"
// system
#include "graphics/3d/Object3dCommon.h"
// math
#include "math/MatrixFunc.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
REGISTER_COMPONENT(ModelRendererComponent)

namespace GameObjectComponent
{
	ModelRendererComponent::ModelRendererComponent()
	{
		Register("model", &modelName_);
		Register("castShadow", &castShadow_);
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
		if (LightManager* lightManager = owner->GetLightManager())
		{
			object3d_->SetLightManager(lightManager);
		}
		ApplySettings();
	}

	void ModelRendererComponent::SetModel(const std::string& modelName)
	{
		modelName_ = modelName;
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
