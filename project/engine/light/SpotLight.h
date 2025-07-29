#pragma once
#include <cassert>
#include <d3d12.h>
#include <wrl.h>

#include "DirectXTex/d3dx12.h"
#include "math/MatrixFunc.h"

/**
 * スポットライト
 */
struct GPUSpotLight
{
	Vector4 color;					// ライトの色
	Vector3 position;				// ライトの位置
	float intensity;				// ライトの強さ
	Vector3 direction;				// ライトの向き
	float distance;					// ライトの届く最大距離
	float decay;					// ライトの減衰率
	float cosAngle;					// ライトの余弦
	float cosFalloffStart;			// フォールオフ開始角度の余弦
};

struct CPUSpotLight {
    GPUSpotLight gpuData;

    // 補間用のメンバを追加
    Vector4 startColor;
    Vector4 endColor;
    float duration;       // 補間にかける時間
    float elapsedTime;    // 経過時間
    bool isReversing;     // 補間の方向
    bool isGradientActive; // グラデーションが有効かどうか
    std::function<float(float)> easingFunction; // イージング関数

    // シャドウマップ用のメンバを追加
    Microsoft::WRL::ComPtr<ID3D12Resource> shadowMap;            // シャドウマップのリソース
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;        // 深度ステンシルビュー用ディスクリプタヒープ
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;                       // 深度ステンシルビューのハンドル
    D3D12_VIEWPORT viewport;                                     // ビューポート
    D3D12_RECT scissorRect;                                      // シザー矩形

	//ビュー行列
	Matrix4x4 viewMatrix;
	// プロジェクション行列
	Matrix4x4 projectionMatrix;

    // リソースの初期化関数
    void InitializeShadowMap(ID3D12Device* device) {
        // シャドウマップのテクスチャ設定
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = 1024;   // 解像度は必要に応じて変更
        texDesc.Height = 1024;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_D32_FLOAT;
        texDesc.SampleDesc.Count = 1;
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        // クリア値の設定
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth = 1.0f;

        // ヒーププロパティを明示的に変数として作成
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

        // シャドウマップリソースの作成
        HRESULT hr = device->CreateCommittedResource(
			&heapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&shadowMap)
        );
        assert(SUCCEEDED(hr));

        // DSV用ディスクリプタヒープの作成
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = 1;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dsvHeap));
        assert(SUCCEEDED(hr));

        // DSVハンドルの取得
        dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        device->CreateDepthStencilView(shadowMap.Get(), nullptr, dsvHandle);

        // ビューポートとシザー矩形の設定
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(texDesc.Width);
        viewport.Height = static_cast<float>(texDesc.Height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        scissorRect.left = 0;
        scissorRect.top = 0;
        scissorRect.right = static_cast<LONG>(texDesc.Width);
        scissorRect.bottom = static_cast<LONG>(texDesc.Height);
    }
};