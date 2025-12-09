#include "Skybox.h"

// system
#include "base/Logger.h"
#include "base/Camera.h"
#include "manager/graphics/TextureManager.h"
// math
#include "math/VectorColorCodes.h"
#include "math/MathUtils.h"
// editor
#include "ImGui/imgui.h"

// スカイボックスの頂点数（6面 × 6頂点 = 36頂点）
constexpr int kSkyboxVertexCount = 36;
// キューブの面数
constexpr int kCubeFaceCount = 6;
// 1面あたりの頂点数
constexpr int kVerticesPerFace = 6;
// キューブのハーフサイズ
constexpr float kCubeHalfSize = 1.0f;
// デフォルトのスカイボックススケール
constexpr float kDefaultSkyboxScale = 10.0f;
// ルートパラメータ数
constexpr int kSkyboxRootParameterCount = 3;
// 入力要素数
constexpr int kSkyboxInputElementCount = 3;
// ビュー行列の位置リセット用インデックス
constexpr int kViewMatrixPosIndex = 3;

Skybox::~Skybox()
{
	// 頂点バッファの解放
	if (vertexBuffer_)
	{
		vertexBuffer_.Reset();
		vertices_ = nullptr;
	}
	// マテリアルリソースの解放
	if (materialResource_)
	{
		materialResource_->Unmap(0, nullptr);
		materialResource_.Reset();
		material_ = nullptr;
	}
	// WVP行列リソースの解放
	if (wvpResource_)
	{
		wvpResource_->Unmap(0, nullptr);
		wvpResource_.Reset();
		wvpData_ = nullptr;
	}
}

Skybox::Skybox()
{
}

void Skybox::Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath)
{
	dxCommon_ = dxCommon;

	// モデルデータの初期化
	CreateModeldata(textureFilePath);

	// 頂点データの作成
	CreateVertexData();

	// 座標変換データの作成
	CreateWVPBData();

	// パイプライン作成
	CreatePipelineState();

	// デフォルトのスケールを設定
	transform_.scale = { kDefaultSkyboxScale, kDefaultSkyboxScale, kDefaultSkyboxScale };
}

void Skybox::Update(Camera* camera)
{
	// スカイボックスのワールド行列を計算
	Matrix4x4 worldMatrix = MakeAffineMatrix(
		transform_.scale,
		transform_.rotate,
		transform_.translate
	);

	// ビュー行列を取得し、位置情報をクリア（カメラの移動に追従しない）
	Matrix4x4 viewMatrix = camera->GetViewMatrix();
	viewMatrix.m[kViewMatrixPosIndex][0] = 0.0f;
	viewMatrix.m[kViewMatrixPosIndex][1] = 0.0f;
	viewMatrix.m[kViewMatrixPosIndex][2] = 0.0f;

	// 座標変換行列を計算
	Matrix4x4 projectionMatrix = camera->GetProjectionMatrix();
	Matrix4x4 viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);

	wvpData_->WVP = worldViewProjectionMatrix;
	wvpData_->World = worldMatrix;
	wvpData_->WorldInverseTranspose = MathUtils::Transpose(Inverse(worldMatrix));
}

void Skybox::Draw()
{
	// ルートシグネチャの設定
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

	// パイプラインステートの設定
	dxCommon_->GetCommandList()->SetPipelineState(pipelineState_.Get());

	// トポロジの設定（三角形リスト）
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 頂点バッファの設定
	dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// 座標変換CBufferの設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(0, wvpResource_->GetGPUVirtualAddress());

	// マテリアルCBufferの設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());

	// テクスチャのSRVを設定
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex_));

	// 描画コマンドを発行
	dxCommon_->GetCommandList()->DrawInstanced(UINT(vertexCount_), 1, 0, 0);
}

void Skybox::CreateModeldata(const std::string& textureFilePath)
{
	// テクスチャファイルパスとインデックスを設定
	textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	// マテリアルリソースの作成
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&material_));

	// マテリアルの初期値を設定
	material_->color = VectorColorCodes::White;
	material_->uvTransform = MakeIdentity4x4();
	material_->enableLighting = false;
}

void Skybox::CreateVertexData()
{
	// 頂点データを箱の形に設定 - 各面は2つの三角形（6頂点）で構成
	std::vector<VertexData> vertices(kSkyboxVertexCount);

	const float size = kCubeHalfSize;

	int index = 0;

	// 前面 (z+) - 時計回り（内側から見て）
	vertices[index++] = { {-size, -size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, -size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	vertices[index++] = { {size, -size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	// 後面 (z-) - 時計回り（内側から見て）
	vertices[index++] = { {size, -size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, -size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	vertices[index++] = { {-size, -size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	// 右面 (x+) - 時計回り（内側から見て）
	vertices[index++] = { {size, -size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, -size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	vertices[index++] = { {size, -size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	// 左面 (x-) - 時計回り（内側から見て）
	vertices[index++] = { {-size, -size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, -size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	vertices[index++] = { {-size, -size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	// 上面 (y+) - 時計回り（内側から見て）
	vertices[index++] = { {-size, size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	vertices[index++] = { {size, size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	// 下面 (y-) - 時計回り（内側から見て）
	vertices[index++] = { {-size, -size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, -size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, -size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	vertices[index++] = { {size, -size, -size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {-size, -size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	vertices[index++] = { {size, -size, size, 1.0f}, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };

	// 頂点リソースの作成
	vertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * vertices.size());
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertices_));
	std::memcpy(vertices_, vertices.data(), sizeof(VertexData) * vertices.size());
	vertexCount_ = vertices.size();
	vertexBuffer_->Unmap(0, nullptr);

	// 頂点バッファビューの設定
	vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices.size());
}

void Skybox::CreateWVPBData()
{
	// WVP行列リソースの作成
	wvpResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));

	// 初期値は単位行列
	wvpData_->WVP = MakeIdentity4x4();
	wvpData_->World = MakeIdentity4x4();
}

void Skybox::CreateRootSignature()
{
	// ディスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootSignatureの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameterの設定
	D3D12_ROOT_PARAMETER rootParameters[kSkyboxRootParameterCount] = {};

	// ルートパラメータ0: 頂点シェーダ用座標変換行列
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// ルートパラメータ1: ピクセルシェーダ用マテリアルデータ
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// ルートパラメータ2: ピクセルシェーダ用テクスチャ
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	// シリアライズしてバイナリ化
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// ルートシグネチャを生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void Skybox::CreatePipelineState()
{
	// ルートシグネチャの作成
	CreateRootSignature();

	HRESULT hr;

	// InputLayoutの設定
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[kSkyboxInputElementCount] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);


	// BlendStateの設定（アルファブレンド有効）
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// シェーダーをコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileSharder(L"Resources/shaders/Skybox.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileSharder(L"Resources/shaders/Skybox.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);


	// DepthStencilStateの設定（深度書き込み無効）
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// PSOの設定と生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// レンダーターゲットの設定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// パイプラインステートを生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}
