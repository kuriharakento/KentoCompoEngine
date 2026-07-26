#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Quaternion.h"
#include "math/MatrixFunc.h"
#include <cstdint>

namespace KCE
{
/**
 * @brief パーティクルデータ構造（CPU/GPU統一レイアウト）
 * 
 * 個々のパーティクルの状態を保持する構造体。
 * CPU/GPUシミュレーション両対応。16バイトアライメント。
 */
struct alignas(16) Particle
{
	//===== Transform (48 bytes) =====//
	Vector3 position = {};       // 位置 (12 bytes)
	float pad0 = 0.0f;           // アライメント (4 bytes)
	
	Vector3 velocity = {};       // 速度 (12 bytes)
	float pad1 = 0.0f;           // アライメント (4 bytes)
	
	Vector3 scale = { 1, 1, 1 }; // スケール (12 bytes)
	float pad2 = 0.0f;           // アライメント (4 bytes)

	//===== Rotation (16 bytes) =====//
	Vector3 rotation = {};       // オイラー角 (XYZ) (12 bytes)
	float padRot = 0.0f;         // アライメント (4 bytes)

	//===== Appearance (16 bytes) =====//
	Vector4 color = { 1, 1, 1, 1 }; // カラー (RGBA)
	
	//===== Initial Color (16 bytes) =====//
	Vector4 initialColor = { 1, 1, 1, 1 }; // InitialColorModuleで設定された初期カラー

	//===== Lifetime (16 bytes) =====//
	float age = 0.0f;            // 経過時間
	float lifetime = 1.0f;       // 寿命
	float ribbonWidth = 1.0f;    // リボン幅
	uint32_t flags = 0;          // ビットフラグ (bit0: alive, bit1: ribbonHead)

	//===== IDs (16 bytes) =====//
	uint32_t id = 0;             // パーティクルID
	uint32_t ribbonId = 0;       // リボングループID
	uint32_t spriteIndex = 0;    // テクスチャシートのフレーム番号
	uint32_t pad3 = 0;           // アライメント

	//===== Flags =====//
	static constexpr uint32_t FLAG_ALIVE = 1 << 0;
	static constexpr uint32_t FLAG_RIBBON_HEAD = 1 << 1;

	bool IsAlive() const { return (flags & FLAG_ALIVE) != 0; }
	void SetAlive(bool alive) { 
		if (alive) flags |= FLAG_ALIVE; 
		else flags &= ~FLAG_ALIVE; 
	}
	
	bool IsRibbonHead() const { return (flags & FLAG_RIBBON_HEAD) != 0; }
	void SetRibbonHead(bool head) {
		if (head) flags |= FLAG_RIBBON_HEAD;
		else flags &= ~FLAG_RIBBON_HEAD;
	}

	float NormalizedAge() const { return lifetime > 0.0f ? age / lifetime : 1.0f; }
};
// Total: 128 bytes (16バイトの倍数) - initialColorフィールド追加

/**
 * @brief GPU転送用パーティクルインスタンスデータ
 */
struct ParticleGPU
{
	Matrix4x4 worldViewProj;
	Matrix4x4 world;
	Vector4 color;
	Vector4 uvOffsetScale; // テクスチャシート用 (offsetX, offsetY, scaleX, scaleY)
};
} // namespace KCE
