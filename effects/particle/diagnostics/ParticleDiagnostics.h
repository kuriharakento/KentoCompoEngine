#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <filesystem>
#include <optional>

namespace KCE
{

/**
 * @brief パーティクル測定用スコープ定義
 */
enum class ParticleProfileScope
{
	ManagerUpdate,                 ///< ParticleManager::Update全体
	EmitterUpdateCpuPerCall,       ///< ParticleEmitter::UpdateCPU (1 call)
	EmitterUpdateCpuFrameTotal,    ///< 1フレームのEmitter::UpdateCPU合計
	EmitterUpdateGpuPerCall,       ///< ParticleEmitter::UpdateGPU全体 (1 call)
	EmitterUpdateGpuFrameTotal,    ///< 1フレームのEmitter::UpdateGPU合計
	ReadbackCpuMapMemcpy,          ///< ReadbackバッファのMap/memcpy
	ReadbackCopyCommandRecording,  ///< ReadbackコピーのGPUコマンド記録 (Dispatch内)
	RemoveDeadParticles,           ///< ParticleEmitter::RemoveDeadParticles
	UpdateGPUSpawns,               ///< ParticleEmitter::UpdateGPUSpawns
	UploadCpuMapMemcpy,            ///< UploadバッファのMap/memcpy
	UploadCommandRecording,        ///< UploadコピーのGPUコマンド記録
	RendererUpdatePerCall,         ///< renderer Update (1 call)
	RendererUpdateFrameTotal,      ///< 1フレームのRenderer::Update合計
	RendererDrawRecordingPerCall,  ///< renderer DrawのCPU command recording (1 call)
	RendererDrawRecordingFrameTotal,///< 1フレームのRenderer::Drawコマンド記録合計
	Count
};

/**
 * @brief スコープごとの統計情報
 */
struct ParticleScopeStats
{
	double lastMs = 0.0;
	double averageMs = 0.0;
	double minMs = 0.0;
	double maxMs = 0.0;
	uint64_t sampleCount = 0;

	void Reset();
	void Update(double ms);
};

/**
 * @brief バイト転送カウンター統計情報
 */
struct ParticleByteStats
{
	uint64_t lastBytes = 0;
	uint64_t totalBytes = 0;
	double averageBytes = 0.0;
	uint64_t minBytes = 0;
	uint64_t maxBytes = 0;
	uint64_t sampleCount = 0;

	void Reset();
	void Update(uint64_t bytes);
};

struct ParticleRuntimeCounters
{
	uint64_t pureGpuEmitters = 0;
	uint64_t hybridGpuEmitters = 0;
	uint64_t moduleOperations = 0;
	uint64_t lutSamples = 0;
	uint64_t estimatedDescriptors = 0;
};

/**
 * @brief パーティクル性能測定コレクター
 */
class ParticleDiagnostics
{
public:
	static ParticleDiagnostics* GetInstance();

	void SetEnabled(bool enabled) { enabled_ = enabled; }
	bool IsEnabled() const { return enabled_; }

	void Reset();
	void RecordTime(ParticleProfileScope scope, double ms);
	const ParticleScopeStats& GetStats(ParticleProfileScope scope) const;

	// Frame Counters
	void BeginFrameCounters();
	void EndFrameCounters();

	// Counter APIs
	void AddCpuUploadMemcpyBytes(uint64_t bytes);
	void AddGpuUploadCopyBytes(uint64_t bytes);
	void AddGpuReadbackCopyBytes(uint64_t bytes);
	void AddCpuReadbackMemcpyBytes(uint64_t bytes);
	void RecordGpuEmitter(bool pureGpu, uint32_t moduleOperations, uint32_t lutSamples, uint32_t estimatedDescriptors);
	const ParticleRuntimeCounters& GetRuntimeCounters() const { return lastRuntimeCounters_; }

	const ParticleByteStats& GetCpuUploadMemcpyStats() const { return cpuUploadMemcpyStats_; }
	const ParticleByteStats& GetGpuUploadCopyStats() const { return gpuUploadCopyStats_; }
	const ParticleByteStats& GetGpuReadbackCopyStats() const { return gpuReadbackCopyStats_; }
	const ParticleByteStats& GetCpuReadbackMemcpyStats() const { return cpuReadbackMemcpyStats_; }

	std::string GetScopeName(ParticleProfileScope scope) const;

	/**
	 * @brief 測定結果をCSVにエクスポート
	 */
	bool ExportToCSV(const std::string& filepath, int preset, const std::string& modeStr, const std::string& buildStr, uint32_t emitterCount, uint32_t activeCount, const std::string& resultStatus = "PASS");

private:
	ParticleDiagnostics();
	~ParticleDiagnostics() = default;
	ParticleDiagnostics(const ParticleDiagnostics&) = delete;
	ParticleDiagnostics& operator=(const ParticleDiagnostics&) = delete;

	bool enabled_ = true;
	bool inFrame_ = false;

	std::array<ParticleScopeStats, static_cast<size_t>(ParticleProfileScope::Count)> stats_;

	// 累積バイト統計
	ParticleByteStats cpuUploadMemcpyStats_;
	ParticleByteStats gpuUploadCopyStats_;
	ParticleByteStats gpuReadbackCopyStats_;
	ParticleByteStats cpuReadbackMemcpyStats_;

	// テンポラリカウンター（現フレーム用）
	uint64_t currentCpuUploadMemcpyBytes_ = 0;
	uint64_t currentGpuUploadCopyBytes_ = 0;
	uint64_t currentGpuReadbackCopyBytes_ = 0;
	uint64_t currentCpuReadbackMemcpyBytes_ = 0;
	ParticleRuntimeCounters currentRuntimeCounters_;
	ParticleRuntimeCounters lastRuntimeCounters_;

	// 累積プロファイラー（FrameTotal用）
	double emitterUpdateCpuAccum_ = 0.0;
	double emitterUpdateGpuAccum_ = 0.0;
	double rendererUpdateAccum_ = 0.0;
	double rendererDrawRecordingAccum_ = 0.0;
};

/**
 * @brief RAIIベースのプロファイリングタイマー
 */
class ParticleScopeTimer
{
public:
	ParticleScopeTimer(ParticleProfileScope scope);
	~ParticleScopeTimer();

private:
	ParticleProfileScope scope_;
	std::chrono::time_point<std::chrono::steady_clock> start_;
};

} // namespace KCE
