#include "Object3d.h"

// system
#include "Object3dCommon.h"
// math
#include "math/MathUtils.h"
// graphics
#include "manager/scene/LightManager.h"
#include "manager/system/SrvManager.h"

// デフォルトのディレクショナルライト強度
constexpr float kDefaultLightIntensity = 0.5f;
// デフォルトのディレクショナルライト方向のY成分
constexpr float kDefaultLightDirectionY = -1.0f;
// デフォルトのスケール値
constexpr float kDefaultScale = 1.0f;
// ワールド行列の位置成分のインデックス
constexpr int kWorldMatrixPosIndex = 3;

///////////////////////////////////////////////////////////////////////
///						>>>基本的な処理<<<							///
///////////////////////////////////////////////////////////////////////

Object3d::~Object3d()
{
	// 座標変換行列のリソースを解放
	if (wvpResource_)
	{
		wvpResource_->Unmap(0, nullptr);
		wvpResource_.Reset();
	}
	// 平行光源のリソースを解放
	if (directionalLightResource_)
	{
		directionalLightResource_->Unmap(0, nullptr);
		directionalLightResource_.Reset();
	}
	// カメラのリソースを解放
	if (cameraResource_)
	{
		cameraResource_->Unmap(0, nullptr);
		cameraResource_.Reset();
	}
}

void Object3d::Initialize(Object3dCommon* object3dCommon,Camera* camera)
{
	// 引数で受け取った物を記録する
	object3dCommon_ = object3dCommon;

	// 引数が指定されていれば引数のカメラを使う。指定されていなければデフォルトのカメラを使う
	camera_ = camera ? camera : object3dCommon->GetDefaultCamera();

	// 描画設定の初期化
	InitializeRenderingSettings();

	// Transformの初期値を設定
	transform_ = {
		{ kDefaultScale, kDefaultScale, kDefaultScale },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f },
	};
}

void Object3d::Update(CameraManager* camera)
{
	// カメラマネージャーが指定されていればアクティブカメラを使用
	camera_ = camera ? camera->GetActiveCamera() : camera_;

	// 座標変換行列の更新
	UpdateMatrix(camera_);
}

void Object3d::Update(float deltaTime, Camera* camera)
{
	// deltaTimeは静的モデルでは未使用
	(void)deltaTime;

	// カメラが指定されていれば使用
	if (camera)
	{
		camera_ = camera;
	}

	// 座標変換行列の更新
	UpdateMatrix(camera_);
}

void Object3d::Draw()
{
	auto* commandList = object3dCommon_->GetDXCommon()->GetCommandList();

	// 座標変換行列CBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// 平行光源CBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	// カメラCBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());

	// ライトマネージャーがあればライトの描画を行う
	LightManager* activeLightManager = lightManager_ ? lightManager_ : object3dCommon_->GetDefaultLightManager();
	if (activeLightManager)
	{
		activeLightManager->Draw();

		// シャドウ行列CBVは常にバインドする（シェーダーが必ずアクセスするため）
		// シャドウマップSRVが設定されている場合のみSRVもバインド
		if (shadowEnabled_ && srvManager_)
		{
			// シャドウマップSRV（ルートパラメータ9）
			srvManager_->SetGraphicsRootDescriptorTable(9, shadowMapSrvIndex_);
		}
		// シャドウ行列CBV（ルートパラメータ10）は常にバインド
		commandList->SetGraphicsRootConstantBufferView(10, activeLightManager->GetShadowMatrixGPUAddress());
		// カスケードシャドウデータ（ルートパラメータ11）もバインド
		commandList->SetGraphicsRootConstantBufferView(11, activeLightManager->GetCascadeShadowDataGPUAddress());
	}

	// 3Dモデルが割り当てられていれば描画する
	if(model_)
	{
		model_->Draw();
	}
}

void Object3d::DrawShadow(D3D12_GPU_VIRTUAL_ADDRESS lightViewProjectionAddress)
{
	// 3Dモデルが割り当てられていなければスキップ
	if (!model_) return;

	auto* commandList = object3dCommon_->GetDXCommon()->GetCommandList();

	// ライトビュープロジェクション行列を設定（ルートパラメータ0）
	commandList->SetGraphicsRootConstantBufferView(0, lightViewProjectionAddress);

	// 描画実行
	DrawShadowOnly();
}

void Object3d::DrawShadowWithMatrix(const Matrix4x4& lightViewProjection)
{
	// 非推奨：この関数は使用しないでください
	// 正しい描画のためには、ライトビュープロジェクション行列のGPUアドレスを渡すDrawShadowを使用するか、
	// 外部で行列を設定してからDrawShadowOnlyを呼び出してください。
    (void)lightViewProjection;
}

void Object3d::DrawShadowOnly()
{
	// 3Dモデルが割り当てられていなければスキップ
	if (!model_) return;

	auto* commandList = object3dCommon_->GetDXCommon()->GetCommandList();

	// ワールド行列を設定（ルートパラメータ1）
	// TransformationMatrix構造体を渡す（シェーダー側でgWVPをスキップしてgWorldを使用）
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// モデルの頂点バッファを設定して描画
	model_->DrawShadow();
}

void Object3d::DrawGBuffer()
{
	// 3Dモデルが割り当てられていなければスキップ
	if (!model_) return;

	auto* commandList = object3dCommon_->GetDXCommon()->GetCommandList();

	// 座標変換行列CBufferの場所を設定（ルートパラメータ0: TransformationMatrix）
	commandList->SetGraphicsRootConstantBufferView(0, wvpResource_->GetGPUVirtualAddress());

	// カメラCBufferの場所を設定（ルートパラメータ1: Camera）
	commandList->SetGraphicsRootConstantBufferView(1, cameraResource_->GetGPUVirtualAddress());

	// モデルをG-Buffer用に描画（ルートパラメータ2=Material、3=Textureはモデル内で設定）
	model_->DrawGBuffer();
}


///////////////////////////////////////////////////////////////////////
///						>>>その他関数の処理<<<						///
///////////////////////////////////////////////////////////////////////

void Object3d::UpdateMatrix(Camera* camera)
{
	// 安全チェック
	if (!transformationMatrixData_) return;

	// 引数が指定されていれば引数のカメラを使う。指定されていなければデフォルトのカメラを使う
	camera_ = camera ? camera : object3dCommon_->GetDefaultCamera();

	// ワールド行列を計算
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	Matrix4x4 worldViewProjectionMatrix;
	Matrix4x4 worldInverseTransposeMatrix = MathUtils::Transpose(Inverse(worldMatrix));

	// カメラが有効な場合はビュープロジェクション行列を適用
	if (camera)
	{
		const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
		worldViewProjectionMatrix = worldMatrix * viewProjectionMatrix;

		// カメラのワールド座標をシェーダーに送る
		if (cameraData_)
		{
			cameraData_->worldPos = { 
				camera->GetWorldMatrix().m[kWorldMatrixPosIndex][0], 
				camera->GetWorldMatrix().m[kWorldMatrixPosIndex][1], 
				camera->GetWorldMatrix().m[kWorldMatrixPosIndex][2] 
			};
		}
	}
	else
	{
		worldViewProjectionMatrix = worldMatrix;
	}

	// モデルがある場合はモデルのローカル行列を乗算
	if (model_)
	{
		const Matrix4x4& local = model_->GetModelData().rootNode.localMatrix;
		transformationMatrixData_->WVP = local * worldViewProjectionMatrix;
		transformationMatrixData_->World = local * worldMatrix;
	}
	else
	{
		transformationMatrixData_->WVP = worldViewProjectionMatrix;
		transformationMatrixData_->World = worldMatrix;
	}

	transformationMatrixData_->WorldInverseTranspose = worldInverseTransposeMatrix;

	// ライト情報を更新（LightManagerからディレクショナルライトを同期）
	if (directionalLightData_ && lightManager_)
	{
		*directionalLightData_ = lightManager_->GetDirectionalLight();
	}
}

void Object3d::UpdateWorldMatrix()
{
	// ワールド行列のみを更新する
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

	// 法線変換用の逆転置行列を計算
	Matrix4x4 worldInverseTransposeMatrix = MathUtils::Transpose(Inverse(worldMatrix));

	// transformationMatrixData_ がなければ何もしない
	if (!transformationMatrixData_) return;

	// モデルのローカル行列があれば乗算して World を構築
	if (model_)
	{
		const Matrix4x4& local = model_->GetModelData().rootNode.localMatrix;
		transformationMatrixData_->World = local * worldMatrix;
	}
	
}

void Object3d::UpdateMatrixWithWorld(const Matrix4x4& worldMatrix, Camera* camera)
{
	if (!transformationMatrixData_) return;

	// 引数が指定されていれば引数のカメラを使う
	camera_ = camera ? camera : object3dCommon_->GetDefaultCamera();

	Matrix4x4 worldViewProjectionMatrix;
	Matrix4x4 worldInverseTransposeMatrix = MathUtils::Transpose(Inverse(worldMatrix));

	// カメラが有効な場合はビュープロジェクション行列を適用
	if (camera_)
	{
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = worldMatrix * viewProjectionMatrix;

		// カメラのワールド座標をシェーダーに送る
		if (cameraData_)
		{
			cameraData_->worldPos = { 
				camera_->GetWorldMatrix().m[kWorldMatrixPosIndex][0], 
				camera_->GetWorldMatrix().m[kWorldMatrixPosIndex][1], 
				camera_->GetWorldMatrix().m[kWorldMatrixPosIndex][2] 
			};
		}
	}
	else
	{
		worldViewProjectionMatrix = worldMatrix;
	}

	// モデルがある場合はモデルのローカル行列を乗算
	if (model_)
	{
		const Matrix4x4& local = model_->GetModelData().rootNode.localMatrix;
		transformationMatrixData_->WVP = local * worldViewProjectionMatrix;
		transformationMatrixData_->World = local * worldMatrix;
	}
	else
	{
		transformationMatrixData_->WVP = worldViewProjectionMatrix;
		transformationMatrixData_->World = worldMatrix;
	}

	transformationMatrixData_->WorldInverseTranspose = worldInverseTransposeMatrix;
}

void Object3d::CreateWvpData()
{
	// 座標変換行列リソースを作成
	wvpResource_ = object3dCommon_->GetDXCommon()->CreateBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースにデータを書き込むためのアドレスを取得
	wvpResource_->Map(0,
		nullptr,
		reinterpret_cast<void**>(&transformationMatrixData_)
	);

	// 単位行列で初期化
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();
}

void Object3d::CreateDirectionalLightData()
{
	// 平行光源リソースを作成
	directionalLightResource_ = object3dCommon_->GetDXCommon()->CreateBufferResource(sizeof(DirectionalLight));

	// 平行光源リソースにデータを書き込むためのアドレスを取得
	directionalLightResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&directionalLightData_)
	);

	// デフォルト値を設定
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = Vector3::Normalize({ 0.0f, kDefaultLightDirectionY, 0.0f });
	directionalLightData_->intensity = kDefaultLightIntensity;
}

void Object3d::CreateCameraData()
{
	// カメラリソースを作成
	cameraResource_ = object3dCommon_->GetDXCommon()->CreateBufferResource(sizeof(CameraForGPU));

	// カメラリソースにデータを書き込むためのアドレスを取得
	cameraResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&cameraData_)
	);
  
	// デフォルト値を設定
	cameraData_->worldPos = {};
}


void Object3d::InitializeRenderingSettings()
{
	// 座標変換行列の生成
	CreateWvpData();

	// 平行光源データの生成
	CreateDirectionalLightData();

	// カメラデータの生成
	CreateCameraData();
}

void Object3d::SetShadowMap(SrvManager* srvManager, uint32_t shadowMapSrvIndex, D3D12_GPU_VIRTUAL_ADDRESS shadowMatrixGPUAddress)
{
	srvManager_ = srvManager;
	shadowMapSrvIndex_ = shadowMapSrvIndex;
	shadowMatrixGPUAddress_ = shadowMatrixGPUAddress;
	shadowEnabled_ = true;
}
