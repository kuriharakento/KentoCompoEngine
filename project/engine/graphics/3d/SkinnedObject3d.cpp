#include "SkinnedObject3d.h"

#include "Object3dCommon.h"
#include "math/MatrixFunc.h"
#include "math/MathUtils.h"
#include "manager/system/SrvManager.h"
#include "manager/graphics/ModelManager.h"
#include "manager/scene/LightManager.h"

SkinnedObject3d::~SkinnedObject3d()
{
	if (wvpResource_)
	{
		wvpResource_->Unmap(0, nullptr);
		wvpResource_.Reset();
	}
	if (cameraResource_)
	{
		cameraResource_->Unmap(0, nullptr);
		cameraResource_.Reset();
	}
	if (directionalLightResource_)
	{
		directionalLightResource_->Unmap(0, nullptr);
		directionalLightResource_.Reset();
	}
}

void SkinnedObject3d::Initialize(Object3dCommon* object3dCommon)
{
	object3dCommon_ = object3dCommon;

	// スキニング計算用クラスを初期化
	skinningCompute_ = std::make_unique<SkinningCompute>();
	skinningCompute_->Initialize(object3dCommon_->GetDXCommon(), object3dCommon_->GetSrvManager());

	// 描画リソースの作成
	CreateDrawResources();

	// トランスフォームの初期化
	transform_ = Transform();
	worldMatrix_ = MakeIdentity4x4();

	// ライトの初期化
	directionalLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLight_.direction = { 0.0f, -1.0f, 0.0f };
	directionalLight_.intensity = 1.0f;
}

void SkinnedObject3d::Update(float deltaTime)
{
	// アニメーターを更新
	animator_.Update(deltaTime);

	// ワールド行列を更新
	UpdateTransform();

	// アニメーターにワールド行列を設定（ボーンアタッチ用）
	animator_.SetWorldMatrix(worldMatrix_);

	// WVP行列を計算
	if (camera_ && wvpData_)
	{
		Matrix4x4 viewMatrix = camera_->GetViewMatrix();
		Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
		Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);
		Matrix4x4 wvpMatrix = Multiply(worldMatrix_, viewProjectionMatrix);

		wvpData_->WVP = wvpMatrix;
		wvpData_->World = worldMatrix_;
		wvpData_->WorldInverseTranspose = MathUtils::Transpose(Inverse(worldMatrix_));
	}

	// カメラ情報を更新
	if (camera_ && cameraData_)
	{
		cameraData_->worldPos = camera_->GetTranslate();
	}

	// ライト情報を更新（LightManagerからディレクショナルライトを同期）
	if (directionalLightData_)
	{
		if (lightManager_)
		{
			// LightManagerからディレクショナルライトを取得して同期
			directionalLight_ = lightManager_->GetDirectionalLight();
		}
		*directionalLightData_ = directionalLight_;
	}
}

void SkinnedObject3d::DispatchSkinning()
{
	if (!model_ || !skinningCompute_)
	{
		return;
	}

	// ボーン行列を更新
	skinningCompute_->UpdateBoneMatrices(animator_.GetFinalBoneMatrices());

	// リソースを準備
	skinningCompute_->PrepareResources(
		model_->GetTotalVertexCount(),
		model_->GetSkinnedVertexInputBuffer(),
		model_->GetSkinnedVertexOutputBuffer()
	);

	// スキニング計算を実行
	skinningCompute_->Dispatch();
}

void SkinnedObject3d::Draw()
{
	if (!model_)
	{
		return;
	}

	// グラフィックスパイプラインを設定（コンピュートシェーダー後のリセット）
	object3dCommon_->CommonRenderingSetting();

	auto* commandList = object3dCommon_->GetDXCommon()->GetCommandList();

	// WVP行列を設定
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// ディレクショナルライトを設定
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	// カメラ情報を設定
	commandList->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());

	// ライトマネージャーの描画（ルートパラメータ5,6,7等の設定）
	if (lightManager_)
	{
		lightManager_->Draw();
	}

	// モデルを描画
	model_->Draw();
}

void SkinnedObject3d::DrawShadow()
{
	if (!model_)
	{
		return;
	}

	auto* commandList = object3dCommon_->GetDXCommon()->GetCommandList();

	// WVP行列（TransformationMatrix）を設定（ルートパラメータ1）
	// シャドウマップパイプラインではWorld行列を使用する
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// モデルを描画
	model_->DrawShadow();
}

void SkinnedObject3d::DrawGBuffer()
{
	if (!model_)
	{
		return;
	}

	auto* commandList = object3dCommon_->GetDXCommon()->GetCommandList();

	// WVP行列を設定
	commandList->SetGraphicsRootConstantBufferView(0, wvpResource_->GetGPUVirtualAddress());

	// カメラ情報を設定
	commandList->SetGraphicsRootConstantBufferView(1, cameraResource_->GetGPUVirtualAddress());

	// モデルを描画
	model_->DrawGBuffer();
}

void SkinnedObject3d::PlayAnimation(uint32_t animationIndex, bool loop)
{
	if (!model_)
	{
		return;
	}

	const auto& animations = model_->GetAnimations();
	if (animationIndex >= animations.size())
	{
		return;
	}

	animator_.PlayAnimation(&animations[animationIndex], loop);
}

void SkinnedObject3d::PlayAnimation(const std::string& animationName, bool loop)
{
	if (!model_)
	{
		return;
	}

	const auto& animations = model_->GetAnimations();
	for (const auto& anim : animations)
	{
		if (anim.name == animationName)
		{
			animator_.PlayAnimation(&anim, loop);
			return;
		}
	}
}

void SkinnedObject3d::StopAnimation()
{
	animator_.StopAnimation();
}

bool SkinnedObject3d::IsAnimationPlaying() const
{
	return animator_.IsPlaying();
}

Matrix4x4 SkinnedObject3d::GetBoneWorldMatrix(const std::string& boneName) const
{
	return animator_.GetBoneWorldMatrix(boneName);
}

void SkinnedObject3d::SetModel(std::unique_ptr<SkinnedModel> model)
{
	model_ = std::move(model);

	if (model_)
	{
		// アニメーターを初期化
		animator_.Initialize(&model_->GetModelData().skeleton);
	}
}

void SkinnedObject3d::SetModel(const std::string& filePath, const std::string& modelType)
{
	// モデルを作成して初期化
	auto model = std::make_unique<SkinnedModel>();
	model->Initialize(
		ModelManager::GetInstance()->GetModelCommon(),
		"Resources/models",
		filePath,
		modelType
	);

	// モデルを設定
	SetModel(std::move(model));
}

void SkinnedObject3d::SetColor(const Vector4& color)
{
	// モデルへのカラー設定（必要に応じて）
}

void SkinnedObject3d::SetEnableLighting(bool enable)
{
	// モデルへのライティング設定（必要に応じて）
}

void SkinnedObject3d::SetShininess(float shininess)
{
	// モデルへの反射強度設定（必要に応じて）
}

void SkinnedObject3d::UpdateTransform()
{
	worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
}

void SkinnedObject3d::CreateDrawResources()
{
	auto* dxCommon = object3dCommon_->GetDXCommon();

	// WVP行列リソースの作成
	wvpResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	wvpData_->WVP = MakeIdentity4x4();
	wvpData_->World = MakeIdentity4x4();
	wvpData_->WorldInverseTranspose = MakeIdentity4x4();

	// カメラリソースの作成
	cameraResource_ = dxCommon->CreateBufferResource(sizeof(CameraForGPU));
	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
	cameraData_->worldPos = { 0.0f, 0.0f, 0.0f };
	cameraData_->padding = 0.0f;

	// ディレクショナルライトリソースの作成
	directionalLightResource_ = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
	*directionalLightData_ = directionalLight_;
}
