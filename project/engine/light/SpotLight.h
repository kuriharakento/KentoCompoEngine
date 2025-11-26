#pragma once
#include <cassert>
#include <d3d12.h>
#include <wrl.h>

#include "DirectXTex/d3dx12.h"
#include "math/MatrixFunc.h"

// シャドウマップの解像度
constexpr UINT kShadowMapResolution = 1024;
// シャドウマップのクリア時深度値
constexpr float kShadowMapClearDepth = 1.0f;

/**
 * @brief GPU用スポットライト構造体
 * 
 * シェーダーに送信するスポットライトのデータ。
 * 特定の位置から円錐状に光を放射する光源を表現します。
 * 懐中電灯やステージライトなどの表現に適しています。
 */
struct GPUSpotLight
{
	Vector4 color;					// ライトの色（RGBAで指定）
	Vector3 position;				// ライトの位置（ワールド座標）
	float intensity;				// ライトの強さ（明るさの倍率）
	Vector3 direction;				// ライトの向き（正規化されたベクトル）
	float distance;					// ライトの届く最大距離
	float decay;					// ライトの減衰率（高いほど早く減衰）
	float cosAngle;					// ライト円錐角度の余弦（内側境界）
	float cosFalloffStart;			// フォールオフ開始角度の余弦（外側境界）
};

/**
 * @brief CPU用スポットライト構造体
 * 
 * GPU側のデータに加え、アニメーション処理とシャドウマップ用のメンバを持つ
 * CPU側で管理するスポットライト。
 * シャドウマップを使用してリアルな影の描画が可能。
 */
struct CPUSpotLight {
    GPUSpotLight gpuData;   // GPU送信用データ

    // 色補間用メンバ
    Vector4 startColor;      // グラデーション開始色
    Vector4 endColor;        // グラデーション終了色
    float duration;          // 補間にかける時間（秒）
    float elapsedTime;       // 経過時間（秒）
    bool isReversing;        // 補間の方向（往復用）
    bool isGradientActive;   // グラデーションが有効かどうか
    std::function<float(float)> easingFunction; // イージング関数

    // シャドウマップ用メンバ
    Microsoft::WRL::ComPtr<ID3D12Resource> shadowMap;            // シャドウマップの深度テクスチャ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;        // DSV用ディスクリプタヒープ
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;                       // 深度ステンシルビューのハンドル
    D3D12_VIEWPORT viewport;                                     // シャドウマップ用ビューポート
    D3D12_RECT scissorRect;                                      // シャドウマップ用シザー矩形

	Matrix4x4 viewMatrix;        // ライト視点のビュー行列
	Matrix4x4 projectionMatrix;  // ライト視点のプロジェクション行列

    /**
     * @brief シャドウマップリソースを初期化
     * 
     * 深度テクスチャ、DSVヒープ、ビューポート、シザー矩形を作成します。
     * @param device D3D12デバイス
     */
    void InitializeShadowMap(ID3D12Device* device) {
        // シャドウマップのテクスチャ設定
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = kShadowMapResolution;
        texDesc.Height = kShadowMapResolution;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_D32_FLOAT;
        texDesc.SampleDesc.Count = 1;
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        // クリア値の設定
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth = kShadowMapClearDepth;

        // ヒーププロパティの作成
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

        // DSVハンドルの取得とビューの作成
        dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        device->CreateDepthStencilView(shadowMap.Get(), nullptr, dsvHandle);

        // ビューポートの設定
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>(texDesc.Width);
        viewport.Height = static_cast<float>(texDesc.Height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        // シザー矩形の設定
        scissorRect.left = 0;
        scissorRect.top = 0;
        scissorRect.right = static_cast<LONG>(texDesc.Width);
        scissorRect.bottom = static_cast<LONG>(texDesc.Height);
    }
};