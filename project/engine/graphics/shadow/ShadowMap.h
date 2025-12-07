#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "math/MatrixFunc.h"

/**
 * @brief シャドウマップの解像度設定
 */
namespace ShadowMapConfig {
    constexpr UINT kDirectionalLightResolution = 2048;  // ディレクショナルライト用
    constexpr UINT kSpotLightResolution = 1024;         // スポットライト用
    constexpr UINT kPointLightResolution = 512;         // ポイントライト用（キューブマップ各面）
    constexpr float kDefaultNearPlane = 0.1f;
    constexpr float kDefaultFarPlane = 100.0f;
    constexpr float kDefaultShadowBias = 0.005f;
}

/**
 * @brief シャドウマップのライトタイプ
 */
enum class ShadowMapType {
    Directional,    // ディレクショナルライト（正射影）
    Spot,           // スポットライト（透視投影）
    Point           // ポイントライト（キューブマップ）
};

/**
 * @brief シャドウマップ構造体
 * @details 深度バッファとビュー情報を保持
 */
struct ShadowMap {
    // 深度バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer;
    
    // DSVハンドル（深度ステンシルビュー）
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    
    // SRVインデックス（シェーダーリソースビュー）
    uint32_t srvIndex = 0;
    
    // シャドウマップの解像度
    uint32_t resolution = ShadowMapConfig::kSpotLightResolution;
    
    // ライトのビュー行列
    Matrix4x4 lightView = {};
    
    // ライトのプロジェクション行列
    Matrix4x4 lightProjection = {};
    
    // ビュー・プロジェクション行列の積
    Matrix4x4 lightViewProjection = {};
    
    // シャドウマップタイプ
    ShadowMapType type = ShadowMapType::Spot;
    
    // シャドウバイアス
    float shadowBias = ShadowMapConfig::kDefaultShadowBias;
    
    // 有効フラグ
    bool isEnabled = true;
};

/**
 * @brief ポイントライト用キューブマップシャドウ構造体
 * @details 6方向の深度バッファを保持
 */
struct PointLightShadowMap {
    // 6面の深度バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer;
    
    // 各面のDSVハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandles[6] = {};
    
    // キューブマップSRVインデックス
    uint32_t srvIndex = 0;
    
    // 各面のビュー行列（+X, -X, +Y, -Y, +Z, -Z）
    Matrix4x4 lightViews[6] = {};
    
    // プロジェクション行列（全面共通、90度FOV）
    Matrix4x4 lightProjection = {};
    
    // 解像度
    uint32_t resolution = ShadowMapConfig::kPointLightResolution;
    
    // シャドウバイアス
    float shadowBias = ShadowMapConfig::kDefaultShadowBias;
    
    // 有効フラグ
    bool isEnabled = true;
};
