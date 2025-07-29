#pragma once
#include "base/Camera.h"

class CameraWorkBase
{
public:
	virtual void Initialize(Camera* camera) = 0;
	virtual void Update() = 0;
	void Setcamera(Camera* camera) { camera_ = camera; }
protected:
	Camera* camera_ = nullptr;
};
