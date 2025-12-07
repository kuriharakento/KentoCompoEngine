#pragma once
#include <unordered_map>
#include <string>
#include <wrl.h>
#include <d3d12.h>

#include "graphics/shadow/ShadowMap.h"
#include "base/DirectXCommon.h"

class SrvManager;

/**
 * @brief シャドウマップマネージャークラス
 * @details 各ライトタイプのシャドウマップの生成・管理を行う
 *          DSV、SRVの生成とシャドウパスの制御を担当
 */
class ShadowMapManager {
public:
    /**
     * @brief コンストラクタ
     */
    ShadowMapManager() = default;

    /**
     * @brief デストラクタ
     */
    ~ShadowMapManager();

    /**
     * @brief 初期化
     * @param dxCommon DirectXCommonへのポインタ
     * @param srvManager SrvManagerへのポインタ
     */
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    /**
     * @brief ディレクショナルライト用シャドウマップの作成
     * @param resolution シャドウマップの解像度
     */
    void CreateDirectionalLightShadowMap(uint32_t resolution = ShadowMapConfig::kDirectionalLightResolution);

    /**
     * @brief カスケードシャドウマップの作成
     * @param resolution 各カスケードのシャドウマップ解像度
     */
    void CreateCascadeShadowMaps(uint32_t resolution = ShadowMapConfig::kCascadeResolution);

    /**
     * @brief スポットライト用シャドウマップの作成
     * @param name ライトの名前
     * @param resolution シャドウマップの解像度
     */
    void CreateSpotLightShadowMap(const std::string& name, uint32_t resolution = ShadowMapConfig::kSpotLightResolution);

    /**
     * @brief ポイントライト用シャドウマップの作成（キューブマップ）
     * @param name ライトの名前
     * @param resolution シャドウマップの解像度
     */
    void CreatePointLightShadowMap(const std::string& name, uint32_t resolution = ShadowMapConfig::kPointLightResolution);

    /**
     * @brief シャドウパスの開始（ディレクショナルライト）
     */
    void BeginDirectionalLightShadowPass();

    /**
     * @brief カスケードシャドウパスの開始
     * @param cascadeIndex カスケードインデックス（0-3）
     */
    void BeginCascadeShadowPass(uint32_t cascadeIndex);

    /**
     * @brief シャドウパスの開始（スポットライト）
     * @param name ライトの名前
     */
    void BeginSpotLightShadowPass(const std::string& name);

    /**
     * @brief シャドウパスの開始（ポイントライト、指定した面）
     * @param name ライトの名前
     * @param faceIndex 面のインデックス（0-5）
     */
    void BeginPointLightShadowPass(const std::string& name, uint32_t faceIndex);

    /**
     * @brief シャドウパスの終了
     */
    void EndShadowPass();

    /**
     * @brief 全シャドウマップの削除
     */
    void Clear();

public: // ゲッター
    /**
     * @brief ディレクショナルライトシャドウマップの取得
     * @return ディレクショナルライトシャドウマップへの参照
     */
    const ShadowMap& GetDirectionalLightShadowMap() const { return directionalLightShadowMap_; }
    ShadowMap& GetDirectionalLightShadowMap() { return directionalLightShadowMap_; }

    /**
     * @brief スポットライトシャドウマップの取得
     * @param name ライトの名前
     * @return スポットライトシャドウマップへの参照
     */
    const ShadowMap& GetSpotLightShadowMap(const std::string& name) const;
    ShadowMap& GetSpotLightShadowMap(const std::string& name);

    /**
     * @brief ポイントライトシャドウマップの取得
     * @param name ライトの名前
     * @return ポイントライトシャドウマップへの参照
     */
    const PointLightShadowMap& GetPointLightShadowMap(const std::string& name) const;
    PointLightShadowMap& GetPointLightShadowMap(const std::string& name);

    /**
     * @brief ディレクショナルライトシャドウマップが有効か
     * @return 有効な場合true
     */
    bool HasDirectionalLightShadowMap() const { return directionalLightShadowMap_.isEnabled; }

    /**
     * @brief カスケードシャドウマップが有効か
     * @return 有効な場合true
     */
    bool HasCascadeShadowMaps() const { return cascadeShadowMap_.isEnabled; }

    /**
     * @brief カスケードシャドウマップの取得
     * @return カスケードシャドウマップへの参照
     */
    const CascadeShadowMap& GetCascadeShadowMap() const { return cascadeShadowMap_; }
    CascadeShadowMap& GetCascadeShadowMap() { return cascadeShadowMap_; }

    /**
     * @brief 現在描画中のカスケードインデックスを取得
     * @return カスケードインデックス（0-3）
     */
    uint32_t GetCurrentCascadeIndex() const { return currentCascadeIndex_; }

    /**
     * @brief スポットライトシャドウマップが存在するか
     * @param name ライトの名前
     * @return 存在する場合true
     */
    bool HasSpotLightShadowMap(const std::string& name) const;

    /**
     * @brief ポイントライトシャドウマップが存在するか
     * @param name ライトの名前
     * @return 存在する場合true
     */
    bool HasPointLightShadowMap(const std::string& name) const;

private:
    /**
     * @brief 深度バッファリソースの作成
     * @param width 幅
     * @param height 高さ
     * @param arraySize 配列サイズ（キューブマップの場合6）
     * @return 深度バッファリソース
     */
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthBuffer(uint32_t width, uint32_t height, uint32_t arraySize = 1);

    /**
     * @brief DSVの作成
     * @param resource 深度バッファリソース
     * @param arraySlice 配列スライス（キューブマップの面インデックス）
     * @return DSVハンドル
     */
    D3D12_CPU_DESCRIPTOR_HANDLE CreateDSV(ID3D12Resource* resource, uint32_t arraySlice = 0);

    /**
     * @brief SRVの作成（2Dテクスチャ用）
     * @param resource 深度バッファリソース
     * @return SRVインデックス
     */
    uint32_t CreateSRV(ID3D12Resource* resource);

    /**
     * @brief SRVの作成（キューブマップ用）
     * @param resource 深度バッファリソース
     * @return SRVインデックス
     */
    uint32_t CreateCubeSRV(ID3D12Resource* resource);

private:
    // DirectXCommonへのポインタ
    DirectXCommon* dxCommon_ = nullptr;

    // SrvManagerへのポインタ
    SrvManager* srvManager_ = nullptr;

    // DSV用ディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

    // DSVディスクリプタサイズ
    uint32_t dsvDescriptorSize_ = 0;

    // 次に使用するDSVインデックス
    uint32_t dsvIndex_ = 0;

    // ディレクショナルライト用シャドウマップ
    ShadowMap directionalLightShadowMap_;

    // カスケードシャドウマップ
    CascadeShadowMap cascadeShadowMap_;

    // スポットライト用シャドウマップ（名前 -> シャドウマップ）
    std::unordered_map<std::string, ShadowMap> spotLightShadowMaps_;

    // ポイントライト用シャドウマップ（名前 -> シャドウマップ）
    std::unordered_map<std::string, PointLightShadowMap> pointLightShadowMaps_;

    // 現在描画中のシャドウマップリソース（パス終了時にバリア切り替えに使用）
    ID3D12Resource* currentShadowMapResource_ = nullptr;

    // 現在描画中のカスケードインデックス
    uint32_t currentCascadeIndex_ = 0;

    // 最大DSV数
    static constexpr uint32_t kMaxDSVCount = 64;
};
