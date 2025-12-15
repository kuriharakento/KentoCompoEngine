#pragma once
#include <cstdint>

/**
 * @file ModulePriorities.h
 * @brief パーティクルモジュールの実行優先度定数
 * 
 * 優先度は小さいほど先に実行される。
 * 負の値: 物理計算前の処理
 * 0: 標準
 * 正の値: 物理計算後の処理 / 見た目調整
 */

namespace ParticleModulePriority
{
	//===== Spawn Phase =====//
	/** @brief スポーンレート/バースト (最初に評価) */
	constexpr int32_t kSpawnRate = -100;
	
	/** @brief 初期値設定 */
	constexpr int32_t kInitialValue = -80;
	
	/** @brief スポーン形状 */
	constexpr int32_t kSpawnShape = -90;

	//===== Update Phase - 速度系 =====//
	/** @brief 加速度適用 (重力より先) */
	constexpr int32_t kAcceleration = -55;
	
	/** @brief 重力適用 */
	constexpr int32_t kGravity = -50;
	
	/** @brief カールノイズ/乱流 */
	constexpr int32_t kCurlNoise = -26;
	
	/** @brief アトラクター/フォースフィールド */
	constexpr int32_t kForceField = -20;
	
	/** @brief ターゲット追従 */
	constexpr int32_t kSprintToTarget = -10;
	
	/** @brief ドラッグ（空気抵抗） */
	constexpr int32_t kDrag = 0;
	
	/** @brief 速度制限 */
	constexpr int32_t kVelocityLimit = 10;

	//===== Update Phase - 見た目系 =====//
	/** @brief スケール変化 */
	constexpr int32_t kScaleOverLifetime = 40;
	
	/** @brief 速度によるサイズ変化 */
	constexpr int32_t kSizeBySpeed = 42;
	
	/** @brief カラーフェード */
	constexpr int32_t kColorFade = 50;
	
	/** @brief 速度によるカラー変化 */
	constexpr int32_t kColorBySpeed = 52;
	
	/** @brief 回転 */
	constexpr int32_t kRotationOverLifetime = 60;
	
	/** @brief テクスチャシート */
	constexpr int32_t kTextureSheet = 70;

	//===== Update Phase - キル系 =====//
	/** @brief 衝突判定 */
	constexpr int32_t kCollision = 90;
	
	/** @brief キルゾーン */
	constexpr int32_t kKillZone = 95;

	//===== Other =====//
	/** @brief サブエミッター（最後に評価） */
	constexpr int32_t kSubEmitter = 200;

	//===== Motion Effect Modules =====//
	/** @brief 放射状初期速度 */
	constexpr int32_t kRadialVelocity = -85;
	
	/** @brief 速度オーバーライフタイム */
	constexpr int32_t kVelocityOverLifetime = -45;
	
	/** @brief 速度によるストレッチ */
	constexpr int32_t kStretchByVelocity = 44;
	
	/** @brief 風 */
	constexpr int32_t kWind = -25;
	
	/** @brief フリッカー（点滅） */
	constexpr int32_t kFlicker = 55;
	
	/** @brief アルファフェード */
	constexpr int32_t kAlphaFade = 48;
	
	/** @brief 速度による回転 */
	constexpr int32_t kRotationBySpeed = 62;
	
	/** @brief 正弦波 */
	constexpr int32_t kSineWave = -28;
	
	/** @brief 螺旋 */
	constexpr int32_t kSpiral = -22;
	
	/** @brief ツイスト */
	constexpr int32_t kTwist = -18;
}
