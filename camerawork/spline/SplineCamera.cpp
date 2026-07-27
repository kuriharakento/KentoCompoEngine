#include "SplineCamera.h"

// camerawork
#include "camerawork/spline/Spline.h"
// math
#include "math/MathUtils.h"
// graphics
#include "manager/graphics/LineManager.h"
// editor
#include "manager/editor/JsonEditor.h"

namespace KCE
{
// スプライン補間の微小値（次位置計算用）
constexpr float kSplineSegmentDelta = 0.01f;
// セグメントごとのサンプル数（デバッグ描画用）
constexpr int kSamplesPerSegment = 20;

void SplineCamera::Initialize(Camera* camera)
{
	camera_ = camera;
    splineData_ = std::make_shared<SplineData>();
	JsonEditor::GetInstance()->Register("spline", splineData_);
}

void SplineCamera::Update()
{
    const auto& points = splineData_->GetControlPoints();
    
    // 4つ以上のポイントが必要（Catmull-Romスプラインの要件）
    if (points.size() < 4) return;

    // セグメント数を計算（制御点数 - 3）
    int numSegments = static_cast<int>(points.size()) - 3;

    // time_ に基づいて現在のセグメントと補間の割合を計算
    float segmentTime = time_ * numSegments;
    int segment = static_cast<int>(segmentTime);
    float t = segmentTime - segment;

    // セグメントのインデックスが有効範囲内か確認
    if (segment >= numSegments) {
        if (loop_) {
            // ループする場合は最初に戻る
            time_ = 0.0f;
            segment = 0;
            t = 0.0f;
        }
        else {
            // 最後まで進んだ場合、終了状態に設定
            time_ = 1.0f;
            segment = numSegments - 1;
            t = 1.0f;
			isEnd_ = true;
        }
    }

    // セグメントインデックスを有効範囲に制限
    segment = std::clamp(segment, 0, numSegments - 1);

    // Catmull-Romスプライン補間でカメラ位置を計算
    Vector3 pos = Spline::CatmullRom(
        points[segment + 0],
        points[segment + 1],
        points[segment + 2],
        points[segment + 3],
        t
    );

    camera_->SetTranslate(pos);

	// カメラの向き更新
    if (targetPtr_)
    {
        // ターゲットが指定されている場合、カメラの向きを設定
        Vector3 targetPos = *targetPtr_;
        camera_->SetRotate(MathUtils::CalculateDirectionToTarget(pos, targetPos));
    }
	else if (lookFront)
    {
        // ターゲットがない場合、進行方向から回転を計算
        // t + Δt（小さな値）を使って「次の位置」を取得
        float nextT = t + kSplineSegmentDelta;
        int nextSegment = segment;
        if (nextT >= 1.0f) {
            nextT -= 1.0f;
            nextSegment++;
            if (loop_) {
                nextSegment %= numSegments;
            }
            else {
                nextSegment = (std::min)(nextSegment, numSegments - 1);
            }
        }

        // 範囲外参照を防止
        if (nextSegment + 3 < static_cast<int>(points.size())) {
            Vector3 nextPos = Spline::CatmullRom(
                points[nextSegment + 0],
                points[nextSegment + 1],
                points[nextSegment + 2],
                points[nextSegment + 3],
                nextT
            );

            camera_->SetRotate(MathUtils::CalculateDirectionToTarget(pos, nextPos));
        }
    }

    // 時間の進行
    time_ += speed_;
}

void SplineCamera::Start(float speed, bool loop)
{
    // パラメータを設定
    speed_ = speed;
    loop_ = loop;
    time_ = 0.0f;
}


void SplineCamera::LoadJson(const std::string& filePath)
{
	splineData_->LoadJson(filePath);
}

void SplineCamera::DrawSplineLine()
{
#ifdef _DEBUG
    const auto& points = splineData_->GetControlPoints();
    
    // 4つ以上のポイントが必要
    if (points.size() < 4) return;

    // セグメント数を計算
    const int numSegments = static_cast<int>(points.size()) - 3;

    // 各セグメントを描画
    for (int segment = 0; segment < numSegments; ++segment) {
        Vector3 prevPoint = points[segment + 1];

        // セグメントをサンプル数で分割して線を描画
        for (int i = 1; i <= kSamplesPerSegment; ++i) {
            float t = static_cast<float>(i) / kSamplesPerSegment;
            Vector3 currentPoint = Spline::CatmullRom(
                points[segment + 0],
                points[segment + 1],
                points[segment + 2],
                points[segment + 3],
                t
            );

            // 赤色の線を描画
            LineManager::GetInstance()->DrawLine(prevPoint, currentPoint, { 1.0f, 0.0f, 0.0f, 1.0f });
            prevPoint = currentPoint;
        }
    }
#endif
}
} // namespace KCE
