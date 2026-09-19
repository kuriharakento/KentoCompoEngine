#pragma once
#include "effects/particle/ParticleTypes.h"
#include <cstdint>
#include <d3d12.h>

namespace KCE
{
struct ParticleContext;

/**
 * @brief モジュール実行フェーズ
 */
enum class ModulePhase
{
	Spawn,   // パーティクル生成時に実行
	Update,  // 毎フレーム実行
	Render   // 描画前に実行
};

/**
 * @brief パーティクルモジュールインターフェース
 * 
 * Priority: 小さいほど先に実行される
 * 例: -100 (位置計算) → 0 (標準) → 100 (最終調整)
 */
class IModule
{
public:
	virtual ~IModule() = default;
	
	/**
	 * @brief モジュールを実行
	 * @param context パーティクルコンテキスト
	 */
	virtual void Execute(ParticleContext& context) = 0;
	
	/**
	 * @brief モジュールのフェーズを取得
	 */
	virtual ModulePhase GetPhase() const = 0;
	
	/**
	 * @brief モジュール名を取得
	 */
	virtual const char* GetName() const = 0;
	
	/**
	 * @brief 実行優先度を取得（小さいほど先に実行）
	 */
	virtual int32_t GetPriority() const { return 0; }

	/**
	 * @brief モジュールの状態をリセット
	 * 
	 * エミッターのReset/Play時に呼ばれる。
	 * 内部タイマーやフラグをリセットする。
	 */
	virtual void Reset() {}

	/**
	 * @brief 生成が完了済みかどうかを返す
	 * 
	 * デフォルトは true（自身は完了状態を妨げない）。
	 * 継続的に発射を行うモジュール（SpawnRateなど）や、
	 * ループ中のSpawnBurstなどが未完了の間だけ false を返す。
	 */
	virtual bool IsComplete() const { return true; }

	/**
	 * @brief GPUシミュレーション（CS）をサポートしているか
	 */
	virtual bool IsGPUSupported() const { return false; }

	/**
	 * @brief GPU側でモジュールの計算を実行 (CS Dispatch)
	 * @param simulator GPUSimulatorポインタ
	 * @param cmdList D3D12コマンドリストポインタ
	 * @param deltaTime 経過時間
	 */
	virtual void DispatchGPU(class GPUSimulator* simulator, ID3D12GraphicsCommandList* cmdList, float deltaTime) { (void)simulator; (void)cmdList; (void)deltaTime; }
};
} // namespace KCE
