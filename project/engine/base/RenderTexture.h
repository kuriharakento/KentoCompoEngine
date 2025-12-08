#pragma once  

#include <wrl.h>
#include <d3d12.h>  
#include <cstdint>
#include "math/Vector4.h"

class DirectXCommon;
class SrvManager;

/**
 * @brief レンダーテクスチャクラス
 */
class RenderTexture  
{  
public:
    /**
     * @brief 初期化
     * @param dxCommon DirectXCommonへのポインタ
     * @param srvManager SrvManagerへのポインタ
     * @param width テクスチャの幅
     * @param height テクスチャの高さ
     * @param format テクスチャフォーマット
     * @param clearColor クリアカラー
     */
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

    /**
     * @brief レンダリング開始
     */
    void BeginRender();

    /**
     * @brief レンダリング終了
     */
    void EndRender();

    /**
     * @brief ImGui用の描画前処理
     */
	void PreDrawForImGui();

    /**
     * @brief ImGui用の描画後処理
     */
	void PostDrawForImGui();


public:
    /**
     * @brief リソースを取得
     * @return テクスチャリソースへのポインタ
     */
    ID3D12Resource* GetResource() const { return texture_.Get(); }

    /**
     * @brief GPUディスクリプタハンドルを取得
     * @return GPUディスクリプタハンドル
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const;

    /**
     * @brief CPUディスクリプタハンドルを取得
     * @return CPUディスクリプタハンドル
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const;

    /**
     * @brief RTVハンドルを取得
     * @return RTVハンドル
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle_; }

    /**
     * @brief SRVインデックスを取得

     * @return SRVインデックス
     */
    uint32_t GetSRVIndex() const { return srvIndex_; }

private:
    // DirectXCommonへのポインタ
    DirectXCommon* dxCommon_ = nullptr;
    // SrvManagerへのポインタ
    SrvManager* srvManager_ = nullptr;
    // テクスチャリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> texture_;
    // RTV用ディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    // RTVハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    // SRVインデックス
    uint32_t srvIndex_ = 0;
    // テクスチャフォーマット
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    // クリアカラー
    Vector4 clearColor_ = { 0, 0, 0, 1 };
    // テクスチャの幅
    uint32_t width_ = 0;
    // テクスチャの高さ
    uint32_t height_ = 0;
    // 現在のリソース状態
    D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

};
