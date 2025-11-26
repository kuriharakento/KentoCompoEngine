#pragma once
#include "LineCommon.h"
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "base/GraphicsTypes.h"

class Camera;

/**
 * @brief ラインで描画する立方体
 */
struct LineCube {
    Vector3 center;                // 立方体の中心座標
	Vector4 color;                 // 描画色
	std::vector<Vector3> vertices; // 頂点リスト
};

/**
 * @brief ラインで描画する球体
 */
struct LineSphere
{
	Vector3 center;                // 球の中心座標
	float radius;                  // 半径
	Vector4 color;                 // 描画色
	std::vector<Vector3> vertices; // 頂点リスト
};

/**
 * @brief ライン描画クラス
 * 
 * D3D12を使用してラインリスト形式でデバッグ用の線を描画します。
 * 最大3万ライン（6万頂点）まで描画可能。
 * カメラのビュー・プロジェクション行列を使用してワールド空間の線を描画します。
 */
class Line {
public:
    Line() = default;
	~Line();

    /**
     * @brief 初期化
     * @param lineCommon ライン描画共通リソース
     */
    void Initialize(LineCommon* lineCommon);

    /**
     * @brief ラインを追加
     * @param start 始点のワールド座標
     * @param end 終点のワールド座標
     * @param color 線の色（RGBA）
     */
    void AddLine(const Vector3& start, const Vector3& end, const Vector4& color);

    /**
     * @brief 更新処理
     * @param camera カメラ（ビュー・プロジェクション行列取得用）
     */
	void Update(Camera* camera);

    /**
     * @brief 描画処理
     */
	void Draw();

    /**
     * @brief 登録されたラインをすべてクリア
     */
    void Clear();

private:
    void CreateVertexData();      // 頂点バッファの作成
	void CreateWVPResource();     // 定数バッファ（WVP行列）の作成
	void UpdateMatrix(Camera* camera); // 行列の更新
	void UpdateVertexData();      // 頂点データの更新

private:
	static constexpr uint32_t kMaxVertexCount = 60000; // 最大頂点数（3万ライン）
    LineCommon* lineCommon_ = nullptr;                 // 共通リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_; // 頂点バッファ
    std::vector<LineVertex> vertices_;                 // 頂点データ配列
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;        // 頂点バッファビュー
    LineVertex* vertexData_ = nullptr;                 // マップされた頂点データ
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_; // WVP行列用定数バッファ
	LineTransformationMatrix* wvpData_ = nullptr;      // マップされたWVP行列データ
};
