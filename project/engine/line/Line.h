#pragma once
#include "LineCommon.h"
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "base/GraphicsTypes.h"

class Camera;

struct LineCube {
    Vector3 center;
	Vector4 color;
	std::vector<Vector3> vertices;
};

struct LineSphere
{
	Vector3 center;
	float radius;
	Vector4 color;
	std::vector<Vector3> vertices;
};

class Line {
public:
    Line() = default;
	~Line();
    void Initialize(LineCommon* lineCommon);
    void AddLine(const Vector3& start, const Vector3& end, const Vector4& color);
	void Update(Camera* camera);
	void Draw();
    void Clear();
private:
	// 頂点データの作成
    void CreateVertexData();
	// 行列データの作成
	void CreateWVPResource();
	// 行列の更新
	void UpdateMatrix(Camera* camera);
	// 頂点データの更新
	void UpdateVertexData();
private:
	// 最大頂点数 ６万頂点（３万ライン）
	const uint32_t kMaxVertexCount = 60000;
    LineCommon* lineCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    std::vector<LineVertex> vertices_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
    LineVertex* vertexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	LineTransformationMatrix* wvpData_ = nullptr;
};
