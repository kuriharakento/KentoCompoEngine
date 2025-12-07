#include "Object3dCommon.h"

#include <cassert>
// system
#include "base/Logger.h"
#include "manager/graphics/TextureManager.h"

// ルートパラメータ数（カスケードシャドウ用に追加）
constexpr int kRootParameterCount = 16;
// 入力要素数
constexpr int kInputElementCount = 3;
// ディスクリプタレンジ数
constexpr int kDescriptorRangeCount = 1;
// サンプラー数（通常サンプラー + シャドウ比較サンプラー）
constexpr int kStaticSamplerCount = 2;
// 環境マップ用ルートパラメータインデックス
constexpr int kEnvMapRootParamIndex = 8;

void Object3dCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	// 引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// ルートシグネチャの生成
	CreateRootSignature();

	// グラフィックスパイプラインの生成
	CreateGraphicsPipelineState();
}

void Object3dCommon::CommonRenderingSetting()
{
	// ルートシグネチャをセット
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());

	// グラフィックスパイプラインステートをセット
	dxCommon_->GetCommandList()->SetPipelineState(graphicsPipelineState_.Get());

	// プリミティブトポロジーをセット（三角形リスト）
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 環境マップのテクスチャをセット
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
		kEnvMapRootParamIndex,
		srvManager_->GetGPUDescriptorHandle(TextureManager::GetInstance()->GetSRVIndex("./Resources/rostock_laage_airport_4k.dds"))
	);
}

void Object3dCommon::CreateRootSignature()
{
	// 通常テクスチャ用ディスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange[kDescriptorRangeCount] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 環境マップ用ディスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRangeEnvMap[kDescriptorRangeCount] = {};
	descriptorRangeEnvMap[0].BaseShaderRegister = 1;
	descriptorRangeEnvMap[0].NumDescriptors = 1;
	descriptorRangeEnvMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeEnvMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	// RootSignatureの設定
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameterの設定
	D3D12_ROOT_PARAMETER rootParameters[kRootParameterCount] = {};

	// ルートパラメータ0: ピクセルシェーダ用CBV（マテリアル）
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// ルートパラメータ1: 頂点シェーダ用CBV（ワールドビュープロジェクション行列）
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// ルートパラメータ2: ピクセルシェーダ用テクスチャ
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// ルートパラメータ3: ピクセルシェーダ用CBV（ディレクショナルライト）
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	// ルートパラメータ4: ピクセルシェーダ用CBV（カメラ）
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;	

	// ルートパラメータ5: ピクセルシェーダ用SRV（ポイントライト配列）
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Descriptor.ShaderRegister = 3;

	// ルートパラメータ6: ピクセルシェーダ用SRV（スポットライト配列）
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].Descriptor.ShaderRegister = 4;

	// ルートパラメータ7: ピクセルシェーダ用CBV（ライト数）
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[7].Descriptor.ShaderRegister = 5;

	// ルートパラメータ8: ピクセルシェーダ用環境マップテクスチャ
	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[8].DescriptorTable.pDescriptorRanges = descriptorRangeEnvMap;
	rootParameters[8].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeEnvMap);

	// シャドウマップ用ディスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRangeShadowMap[kDescriptorRangeCount] = {};
	descriptorRangeShadowMap[0].BaseShaderRegister = 5; // t5
	descriptorRangeShadowMap[0].NumDescriptors = 1;
	descriptorRangeShadowMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeShadowMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータ9: ピクセルシェーダ用シャドウマップテクスチャ
	rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[9].DescriptorTable.pDescriptorRanges = descriptorRangeShadowMap;
	rootParameters[9].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeShadowMap);

	// ルートパラメータ10: ピクセルシェーダ用CBV（シャドウ行列）
	rootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[10].Descriptor.ShaderRegister = 6;

	// ルートパラメータ11: ピクセルシェーダ用CBV（カスケードシャドウデータ）
	rootParameters[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[11].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[11].Descriptor.ShaderRegister = 7;

	// カスケードシャドウマップ用ディスクリプタレンジの設定（t6-t9）
	D3D12_DESCRIPTOR_RANGE descriptorRangeCascade0[kDescriptorRangeCount] = {};
	descriptorRangeCascade0[0].BaseShaderRegister = 6; // t6
	descriptorRangeCascade0[0].NumDescriptors = 1;
	descriptorRangeCascade0[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeCascade0[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeCascade1[kDescriptorRangeCount] = {};
	descriptorRangeCascade1[0].BaseShaderRegister = 7; // t7
	descriptorRangeCascade1[0].NumDescriptors = 1;
	descriptorRangeCascade1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeCascade1[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeCascade2[kDescriptorRangeCount] = {};
	descriptorRangeCascade2[0].BaseShaderRegister = 8; // t8
	descriptorRangeCascade2[0].NumDescriptors = 1;
	descriptorRangeCascade2[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeCascade2[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeCascade3[kDescriptorRangeCount] = {};
	descriptorRangeCascade3[0].BaseShaderRegister = 9; // t9
	descriptorRangeCascade3[0].NumDescriptors = 1;
	descriptorRangeCascade3[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeCascade3[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータ12-15: カスケードシャドウマップ（t6-t9）
	rootParameters[12].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[12].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[12].DescriptorTable.pDescriptorRanges = descriptorRangeCascade0;
	rootParameters[12].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeCascade0);

	rootParameters[13].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[13].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[13].DescriptorTable.pDescriptorRanges = descriptorRangeCascade1;
	rootParameters[13].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeCascade1);

	rootParameters[14].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[14].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[14].DescriptorTable.pDescriptorRanges = descriptorRangeCascade2;
	rootParameters[14].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeCascade2);

	rootParameters[15].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[15].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[15].DescriptorTable.pDescriptorRanges = descriptorRangeCascade3;
	rootParameters[15].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeCascade3);

	// サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[kStaticSamplerCount] = {};
	// サンプラー0: 通常テクスチャ用
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// サンプラー1: シャドウマップ比較用
	staticSamplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[1].ShaderRegister = 1;
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	HRESULT hr;

	// シリアライズしてバイナリ化
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// ルートシグネチャを生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateGraphicsPipelineState()
{
	// ルートシグネチャの生成
	CreateRootSignature();

	HRESULT hr;

	// InputLayoutの設定
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[kInputElementCount] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
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
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileSharder(L"Resources/shaders/Object3d.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileSharder(L"Resources/shaders/Object3d.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);


	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
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
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}
