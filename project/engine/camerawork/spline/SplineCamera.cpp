#include "SplineCamera.h"

// camerawork
#include "camerawork/spline/Spline.h"
// math
#include "math/MathUtils.h"
// graphics
#include "manager/graphics/LineManager.h"
// editor
#include "manager/editor/JsonEditorManager.h"

void SplineCamera::Initialize(Camera* camera)
{
	camera_ = camera;
    splineData_ = std::make_shared<SplineData>();
	JsonEditorManager::GetInstance()->Register("spline", splineData_);
}

void SplineCamera::Update()
{
    const auto& points = splineData_->GetControlPoints();
    if (points.size() < 4) return;  // 4つ以上のポイントが必要

    int numSegments = static_cast<int>(points.size()) - 3;  // セグメント数を計算

    // time_ に基づいて現在のセグメントと補間の割合を計算
    float segmentTime = time_ * numSegments;
    int segment = static_cast<int>(segmentTime);  // セグメントのインデックス
    float t = segmentTime - segment;  // 補間の割合

    // セグメントのインデックスが有効範囲内か確認
    if (segment >= numSegments) {
        if (loop_) {
            time_ = 0.0f;  // ループする場合は最初に戻る
            segment = 0;   // 最初のセグメントに戻す
            t = 0.0f;      // 補間の割合も0に戻す
        }
        else {
            time_ = 1.0f;  // 最後まで進んだ場合、時間を1.0に設定
            segment = numSegments - 1;  // 最後のセグメント
            t = 1.0f;  // 補間の割合も1.0に設定
			isEnd_ = true;  // 終了フラグを立てる
        }
    }

    // セグメントが有効な範囲に収まるように修正
    segment = std::clamp(segment, 0, numSegments - 1);  // 範囲内に制限

    // スプラインの補間
    Vector3 pos = Spline::CatmullRom(
        points[segment + 0],
        points[segment + 1],
        points[segment + 2],
        points[segment + 3],
        t
    );

    camera_->SetTranslate(pos);

	// カメラの向き更新
    //ターゲットが指定されている場合、カメラの向きを設定
    if (targetPtr_)
    {
        Vector3 targetPos = *targetPtr_;
        camera_->SetRotate(MathUtils::CalculateDirectionToTarget(pos, targetPos));
    }
	else if (lookFront)
    {
        // ターゲットがない場合、進行方向から回転を計算
        // t + Δt（小さな値）を使って「次の位置」を取得
        float nextT = t + 0.01f;
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
    speed_ = speed; // speed は移動の速さ（例えば 0.01f など）
    loop_ = loop;   // ループの有無
    time_ = 0.0f;   // 初期時間
}


void SplineCamera::LoadJson(const std::string& filePath)
{
	splineData_->LoadJson(filePath);
}

void SplineCamera::DrawSplineLine()
{
    const auto& points = splineData_->GetControlPoints();
    if (points.size() < 4) return; // 4つ以上のポイントが必要

    const int numSegments = static_cast<int>(points.size()) - 3; // セグメント数を計算
    const int samplesPerSegment = 20;         // 各セグメントを分割するサンプル数

    for (int segment = 0; segment < numSegments; ++segment) {
        Vector3 prevPoint = points[segment + 1]; // 初期点（セグメントの始点）

        for (int i = 1; i <= samplesPerSegment; ++i) {
            float t = static_cast<float>(i) / samplesPerSegment; // 補間の割合
            Vector3 currentPoint = Spline::CatmullRom(
                points[segment + 0],
                points[segment + 1],
                points[segment + 2],
                points[segment + 3],
                t
            );

            // 線を描画
            LineManager::GetInstance()->DrawLine(prevPoint, currentPoint, { 1.0f, 0.0f, 0.0f, 1.0f }); // 赤色の線
            prevPoint = currentPoint; // 現在の点を次の始点に設定
        }
    }
}
