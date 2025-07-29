#pragma once
#include "base/DirectXCommon.h"

class ModelCommon
{
public:
	//初期化
	void Initialize(DirectXCommon* dxCommon);

public: //ゲッター
	DirectXCommon* GetDXCommon() { return dxCommon_; }

private:
	DirectXCommon* dxCommon_;

};

