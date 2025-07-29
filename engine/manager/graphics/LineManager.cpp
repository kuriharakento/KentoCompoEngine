#include "LineManager.h"


// math
#include <numbers>
#include "math/VectorColorCodes.h"
#include "math/MathUtils.h"
// system
#include "manager/scene/CameraManager.h"

LineManager* LineManager::instance_ = nullptr; // シングルトンインスタンス

LineManager* LineManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new LineManager();
	}
	return instance_;
}

void LineManager::Initialize(DirectXCommon* dxCommon, CameraManager* cameraManager)
{
	dxCommon_ = dxCommon;
	cameraManager_ = cameraManager;
	lineCommon_ = std::make_unique<LineCommon>();
	lineCommon_->Initialize(dxCommon_);
	line_ = std::make_unique<Line>();
	line_->Initialize(lineCommon_.get());
}

void LineManager::Clear() {
    line_->Clear();
}

void LineManager::Finalize()
{
	line_.reset();
	lineCommon_.reset();
	dxCommon_ = nullptr;
	cameraManager_ = nullptr;
	instance_ = nullptr;
}

void LineManager::RenderLines() {
	//頂点データ、行列データの更新
    line_->Update(cameraManager_->GetActiveCamera());
	//描画
    line_->Draw();
	// 描画後にクリア
	Clear();
}

void LineManager::DrawCube(const Vector3& center, float size, const Vector4& color) {
    float halfSize = size / 2.0f;

    // キューブの8つの頂点を計算
    Vector3 vertices[8] = {
        {center.x - halfSize, center.y - halfSize, center.z - halfSize},
        {center.x + halfSize, center.y - halfSize, center.z - halfSize},
        {center.x + halfSize, center.y + halfSize, center.z - halfSize},
        {center.x - halfSize, center.y + halfSize, center.z - halfSize},
        {center.x - halfSize, center.y - halfSize, center.z + halfSize},
        {center.x + halfSize, center.y - halfSize, center.z + halfSize},
        {center.x + halfSize, center.y + halfSize, center.z + halfSize},
        {center.x - halfSize, center.y + halfSize, center.z + halfSize}
    };

    // キューブの12本の辺を追加
    line_->AddLine(vertices[0], vertices[1], color);
    line_->AddLine(vertices[1], vertices[2], color);
    line_->AddLine(vertices[2], vertices[3], color);
    line_->AddLine(vertices[3], vertices[0], color);

    line_->AddLine(vertices[4], vertices[5], color);
    line_->AddLine(vertices[5], vertices[6], color);
    line_->AddLine(vertices[6], vertices[7], color);
    line_->AddLine(vertices[7], vertices[4], color);

    line_->AddLine(vertices[0], vertices[4], color);
    line_->AddLine(vertices[1], vertices[5], color);
    line_->AddLine(vertices[2], vertices[6], color);
    line_->AddLine(vertices[3], vertices[7], color);
}

void LineManager::DrawSphere(const Vector3& center, float radius, const Vector4& color)
{
    const int segments = 12;
    const int rings = 12;

    for (int i = 0; i <= rings; ++i) {
        float theta1 = i * std::numbers::pi_v<float> / rings;
        float theta2 = (i + 1) * std::numbers::pi_v<float> / rings;

        for (int j = 0; j <= segments; ++j) {
            float phi = j * 2 * std::numbers::pi_v<float> / segments;

            Vector3 p1 = {
                center.x + radius * sinf(theta1) * cosf(phi),
                center.y + radius * cosf(theta1),
                center.z + radius * sinf(theta1) * sinf(phi)
            };

            Vector3 p2 = {
                center.x + radius * sinf(theta2) * cosf(phi),
                center.y + radius * cosf(theta2),
                center.z + radius * sinf(theta2) * sinf(phi)
            };

            line_->AddLine(p1, p2, color);
        }
    }

    for (int i = 0; i <= segments; ++i) {
        float phi1 = i * 2 * std::numbers::pi_v<float> / segments;
        float phi2 = (i + 1) * 2 * std::numbers::pi_v<float> / segments;

        for (int j = 0; j <= rings; ++j) {
            float theta = j * std::numbers::pi_v<float> / rings;

            Vector3 p1 = {
                center.x + radius * sinf(theta) * cosf(phi1),
                center.y + radius * cosf(theta),
                center.z + radius * sinf(theta) * sinf(phi1)
            };

            Vector3 p2 = {
                center.x + radius * sinf(theta) * cosf(phi2),
                center.y + radius * cosf(theta),
                center.z + radius * sinf(theta) * sinf(phi2)
            };

            line_->AddLine(p1, p2, color);
        }
    }
}

void LineManager::DrawGrid(float gridSize, float gridSpacing, const Vector4& color)
{
    // グリッドの範囲を計算
    float halfSize = gridSize / 2.0f;

    // X軸方向の線を描画
    for (float z = -halfSize; z <= halfSize; z += gridSpacing) {
        Vector3 start = { -halfSize, 0.0f, z };
        Vector3 end = { halfSize, 0.0f, z };
        DrawLine(start, end, color);
    }

    // Z軸方向の線を描画
    for (float x = -halfSize; x <= halfSize; x += gridSpacing) {
        Vector3 start = { x, 0.0f, -halfSize };
        Vector3 end = { x, 0.0f, halfSize };
        DrawLine(start, end, color);
    }
}

void LineManager::DrawArrow(const Vector3& start, const Vector3& direction, float length, const Vector4& color)
{
    Vector3 dir = Vector3::Normalize(direction) * length;
    Vector3 end = start + dir;

    // メインの矢印線
    DrawLine(start, end, color);

    // 矢じり（左右に 45 度開いた線を描画）
    Vector3 up = { 0, 1, 0 };
    Vector3 right = Vector3::Cross(up, dir);
    if (Vector3::Length(right) < 0.0001f) {
        right = { 1, 0, 0 }; // 向きが真上 or 真下の時は強制
    }
    right = Vector3::Normalize(right);
    up = Vector3::Normalize(Vector3::Cross(dir, right)); // カメラと垂直な方向

    float headLength = length * 0.1f;
    Vector3 leftWing = Vector3::Rotate(-dir, up, 0.4f) * headLength;
    Vector3 rightWing = Vector3::Rotate(-dir, up, -0.4f) * headLength;

    DrawLine(end, end + leftWing, color);
    DrawLine(end, end + rightWing, color);
}

void LineManager::DrawAxis(const Vector3& position, float scale)
{
    // X軸（赤）
    DrawLine(position, position + Vector3{ scale, 0, 0 }, VectorColorCodes::Red);
    // Y軸（緑）
    DrawLine(position, position + Vector3{ 0, scale, 0 }, VectorColorCodes::Green);
    // Z軸（青）
    DrawLine(position, position + Vector3{ 0, 0, scale }, VectorColorCodes::Blue);
}

void LineManager::DrawAABB(const AABB& aabb, const Vector4& color)
{
    // 8頂点を計算
    Vector3 v[8] = {
        {aabb.min_.x, aabb.min_.y, aabb.min_.z},
        {aabb.max_.x, aabb.min_.y, aabb.min_.z},
        {aabb.max_.x, aabb.max_.y, aabb.min_.z},
        {aabb.min_.x, aabb.max_.y, aabb.min_.z},
        {aabb.min_.x, aabb.min_.y, aabb.max_.z},
        {aabb.max_.x, aabb.min_.y, aabb.max_.z},
        {aabb.max_.x, aabb.max_.y, aabb.max_.z},
        {aabb.min_.x, aabb.max_.y, aabb.max_.z},
    };

    // 12本のエッジを線で描画
    DrawLine(v[0], v[1], color); DrawLine(v[1], v[2], color);
    DrawLine(v[2], v[3], color); DrawLine(v[3], v[0], color);

    DrawLine(v[4], v[5], color); DrawLine(v[5], v[6], color);
    DrawLine(v[6], v[7], color); DrawLine(v[7], v[4], color);

    DrawLine(v[0], v[4], color); DrawLine(v[1], v[5], color);
    DrawLine(v[2], v[6], color); DrawLine(v[3], v[7], color);
}

void LineManager::DrawOBB(const OBB& obb, const Vector4& color)
{
    Vector3 halfSize = obb.size;

    // ローカル座標での8頂点
    Vector3 localPoints[8] = {
        {-halfSize.x, -halfSize.y, -halfSize.z},
        {+halfSize.x, -halfSize.y, -halfSize.z},
        {+halfSize.x, +halfSize.y, -halfSize.z},
        {-halfSize.x, +halfSize.y, -halfSize.z},
        {-halfSize.x, -halfSize.y, +halfSize.z},
        {+halfSize.x, -halfSize.y, +halfSize.z},
        {+halfSize.x, +halfSize.y, +halfSize.z},
        {-halfSize.x, +halfSize.y, +halfSize.z},
    };

    // 回転 + 移動適用
    Vector3 worldPoints[8];
    for (int i = 0; i < 8; ++i) {
        worldPoints[i] = MathUtils::Transform(localPoints[i], obb.rotate) + obb.center;
    }

    // 12本のエッジを描画
    int indices[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7},
    };
    for (int i = 0; i < 12; ++i) {
        DrawLine(worldPoints[indices[i][0]], worldPoints[indices[i][1]], color);
    }
}

void LineManager::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color)
{
	line_->AddLine(start, end, color);
}