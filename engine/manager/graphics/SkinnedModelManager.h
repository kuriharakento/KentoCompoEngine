#pragma once
#include <map>
#include <string>
#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "base/GraphicsTypes.h"

class ModelCommon;
class DirectXCommon;

/**
 * @brief スキニングモデルの共有リソース構造体
 */
struct SkinnedModelSharedResource
{
    SkinnedModelData modelData;
    Microsoft::WRL::ComPtr<ID3D12Resource> inputVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer; // インデックスバッファも全メッシュ分統合するか、メッシュごとに持つか検討が必要
    // 現状の SkinnedModel はメッシュごとにインデックスバッファを持っているので、ここもそれに合わせる
    struct MeshResource
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
        uint32_t indexCount = 0;
        uint32_t vertexCount = 0;
        uint32_t vertexOffset = 0;
    };
    std::vector<MeshResource> meshes;
    uint32_t totalVertexCount = 0;
};

/**
 * @brief スキニングモデルマネージャークラス
 * @details スキニングモデルの解析データと静的GPUリソースをキャッシュ・管理するシングルトン
 */
class SkinnedModelManager
{
public:
    static SkinnedModelManager* GetInstance();

    /**
     * @brief 初期化
     * @param dxCommon DirectXCommonへのポインタ
     */
    void Initialize(DirectXCommon* dxCommon);

    /**
     * @brief 終了処理
     */
    void Finalize();

    /**
     * @brief モデルのロードとキャッシュ取得
     * @param directoryPath ディレクトリパス
     * @param filename ファイル名
     * @param modelType 拡張子
     * @return 共有リソースへのポインタ
     */
    const SkinnedModelSharedResource* LoadModel(const std::string& directoryPath, const std::string& filename, const std::string& modelType);

private:
    SkinnedModelManager() = default;
    SkinnedModelManager(const SkinnedModelManager&) = delete;
    SkinnedModelManager& operator=(const SkinnedModelManager&) = delete;

private:
    static std::unique_ptr<SkinnedModelManager> instance_;

    DirectXCommon* dxCommon_ = nullptr;

    // キャッシュ (パスをキーにする)
    std::map<std::string, std::unique_ptr<SkinnedModelSharedResource>> modelCache_;
};
