#pragma once

// camerawork
#include "camerawork/base/CameraWorkBase.h"
#include "camerawork/spline/SplineData.h"

class LineManager;

class SplineCamera : public CameraWorkBase
{
public:
	void Initialize(Camera* camera) override;
	void Update() override;
	void Start(float speed, bool loop);
	void LoadJson(const std::string& filePath);
	void SetLoop(bool loop) { loop_ = loop; }
	void SetTarget(const Vector3* target) { targetPtr_ = target; }
	void SetLookFront(bool lookFront) { lookFront = lookFront; }
	void DrawSplineLine();
	bool IsEnd() const { return isEnd_; }

private:
	Camera* camera_ = nullptr;	//カメラ
	std::shared_ptr<SplineData> splineData_ = nullptr;	//スプラインデータ
	LineManager* lineManager_ = nullptr;	//ラインマネージャー
	const Vector3* targetPtr_ = nullptr;	//ターゲットのポインタ
	float time_ = 0.0f;	//時間経過
	float speed_ = 0.0f;	//スピード
	bool loop_ = false;	//ループフラグ
	bool lookFront = true;	//前を向くか
	bool isEnd_ = false;	//終了フラグ
};

