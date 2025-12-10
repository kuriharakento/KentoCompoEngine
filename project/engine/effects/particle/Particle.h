#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Quaternion.h"

/**
 * @brief パーティクルデータ構造
 * 
 * 個々のパーティクルの状態を保持する構造体。
 * CPU/GPUシミュレーション両対応。
 */
struct Particle
{
	//===== Transform =====//
	Vector3 position = {};       // 位置
	Vector3 velocity = {};       // 速度
	Vector3 scale = { 1, 1, 1 }; // スケール
	Quaternion rotation = {};    // 回転

	//===== Appearance =====//
	Vector4 color = { 1, 1, 1, 1 }; // カラー (RGBA)

	//===== Lifetime =====//
	float age = 0.0f;            // 経過時間
	float lifetime = 1.0f;       // 寿命

	//===== Ribbon用 =====//
	float ribbonWidth = 1.0f;    // リボン幅
	uint32_t ribbonId = 0;       // リボングループID

	//===== ID =====//
	uint32_t id = 0;             // パーティクルID

	bool IsAlive() const { return age < lifetime; }
	float NormalizedAge() const { return lifetime > 0.0f ? age / lifetime : 1.0f; }
};

/**
 * @brief GPU転送用パーティクルデータ
 */
struct ParticleGPU
{
	Matrix4x4 worldViewProj;
	Matrix4x4 world;
	Vector4 color;
};
