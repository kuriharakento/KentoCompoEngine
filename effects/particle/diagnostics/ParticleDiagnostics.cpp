#include "ParticleDiagnostics.h"
#include <fstream>
#include <sstream>

namespace KCE
{

// ParticleScopeStats
void ParticleScopeStats::Reset()
{
	lastMs = 0.0;
	averageMs = 0.0;
	minMs = 0.0;
	maxMs = 0.0;
	sampleCount = 0;
}

void ParticleScopeStats::Update(double ms)
{
	lastMs = ms;
	if (sampleCount == 0)
	{
		minMs = ms;
		maxMs = ms;
		averageMs = ms;
	}
	else
	{
		minMs = (std::min)(minMs, ms);
		maxMs = (std::max)(maxMs, ms);
		// 指数移動平均 (EMA): alpha = 0.05
		constexpr double alpha = 0.05;
		averageMs = alpha * ms + (1.0 - alpha) * averageMs;
	}
	sampleCount++;
}

// ParticleByteStats
void ParticleByteStats::Reset()
{
	lastBytes = 0;
	totalBytes = 0;
	averageBytes = 0.0;
	minBytes = 0;
	maxBytes = 0;
	sampleCount = 0;
}

void ParticleByteStats::Update(uint64_t bytes)
{
	lastBytes = bytes;
	totalBytes += bytes;
	if (sampleCount == 0)
	{
		minBytes = bytes;
		maxBytes = bytes;
	}
	else
	{
		minBytes = (std::min)(minBytes, bytes);
		maxBytes = (std::max)(maxBytes, bytes);
	}
	sampleCount++;
	averageBytes = static_cast<double>(totalBytes) / sampleCount;
}

// ParticleDiagnostics
ParticleDiagnostics* ParticleDiagnostics::GetInstance()
{
	static ParticleDiagnostics instance;
	return &instance;
}

ParticleDiagnostics::ParticleDiagnostics()
{
#ifdef _DEBUG
	enabled_ = true;
#else
	enabled_ = false; // Releaseでは既定無効
#endif
}

void ParticleDiagnostics::Reset()
{
	for (auto& stat : stats_)
	{
		stat.Reset();
	}
	cpuUploadMemcpyStats_.Reset();
	gpuUploadCopyStats_.Reset();
	gpuReadbackCopyStats_.Reset();
	cpuReadbackMemcpyStats_.Reset();

	currentCpuUploadMemcpyBytes_ = 0;
	currentGpuUploadCopyBytes_ = 0;
	currentGpuReadbackCopyBytes_ = 0;
	currentCpuReadbackMemcpyBytes_ = 0;
	currentRuntimeCounters_ = {};
	lastRuntimeCounters_ = {};

	emitterUpdateCpuAccum_ = 0.0;
	emitterUpdateGpuAccum_ = 0.0;
	rendererUpdateAccum_ = 0.0;
	rendererDrawRecordingAccum_ = 0.0;

	inFrame_ = false;
}

void ParticleDiagnostics::RecordTime(ParticleProfileScope scope, double ms)
{
	if (!enabled_) return;

	// PerCall の計測値は EMA 平均に登録しつつ、テンポラリに累積する
	stats_[static_cast<size_t>(scope)].Update(ms);

	if (scope == ParticleProfileScope::EmitterUpdateCpuPerCall)
	{
		emitterUpdateCpuAccum_ += ms;
	}
	else if (scope == ParticleProfileScope::EmitterUpdateGpuPerCall)
	{
		emitterUpdateGpuAccum_ += ms;
	}
	else if (scope == ParticleProfileScope::RendererUpdatePerCall)
	{
		rendererUpdateAccum_ += ms;
	}
	else if (scope == ParticleProfileScope::RendererDrawRecordingPerCall)
	{
		rendererDrawRecordingAccum_ += ms;
	}
}

const ParticleScopeStats& ParticleDiagnostics::GetStats(ParticleProfileScope scope) const
{
	return stats_[static_cast<size_t>(scope)];
}

void ParticleDiagnostics::BeginFrameCounters()
{
	if (!enabled_) return;

	currentCpuUploadMemcpyBytes_ = 0;
	currentGpuUploadCopyBytes_ = 0;
	currentGpuReadbackCopyBytes_ = 0;
	currentCpuReadbackMemcpyBytes_ = 0;
	currentRuntimeCounters_ = {};

	emitterUpdateCpuAccum_ = 0.0;
	emitterUpdateGpuAccum_ = 0.0;
	rendererUpdateAccum_ = 0.0;
	rendererDrawRecordingAccum_ = 0.0;

	inFrame_ = true;
}

void ParticleDiagnostics::EndFrameCounters()
{
	if (!enabled_ || !inFrame_) return;

	// EmitterUpdateCpuFrameTotal などをコミット
	stats_[static_cast<size_t>(ParticleProfileScope::EmitterUpdateCpuFrameTotal)].Update(emitterUpdateCpuAccum_);
	stats_[static_cast<size_t>(ParticleProfileScope::EmitterUpdateGpuFrameTotal)].Update(emitterUpdateGpuAccum_);
	stats_[static_cast<size_t>(ParticleProfileScope::RendererUpdateFrameTotal)].Update(rendererUpdateAccum_);
	stats_[static_cast<size_t>(ParticleProfileScope::RendererDrawRecordingFrameTotal)].Update(rendererDrawRecordingAccum_);

	// Byte Stats 統計にコミット
	cpuUploadMemcpyStats_.Update(currentCpuUploadMemcpyBytes_);
	gpuUploadCopyStats_.Update(currentGpuUploadCopyBytes_);
	gpuReadbackCopyStats_.Update(currentGpuReadbackCopyBytes_);
	cpuReadbackMemcpyStats_.Update(currentCpuReadbackMemcpyBytes_);
	lastRuntimeCounters_ = currentRuntimeCounters_;

	inFrame_ = false;
}

void ParticleDiagnostics::AddCpuUploadMemcpyBytes(uint64_t bytes)
{
	if (enabled_ && inFrame_) currentCpuUploadMemcpyBytes_ += bytes;
}

void ParticleDiagnostics::AddGpuUploadCopyBytes(uint64_t bytes)
{
	if (enabled_ && inFrame_) currentGpuUploadCopyBytes_ += bytes;
}

void ParticleDiagnostics::AddGpuReadbackCopyBytes(uint64_t bytes)
{
	if (enabled_ && inFrame_) currentGpuReadbackCopyBytes_ += bytes;
}

void ParticleDiagnostics::AddCpuReadbackMemcpyBytes(uint64_t bytes)
{
	if (enabled_ && inFrame_) currentCpuReadbackMemcpyBytes_ += bytes;
}

void ParticleDiagnostics::RecordGpuEmitter(bool pureGpu, uint32_t moduleOperations, uint32_t lutSamples, uint32_t estimatedDescriptors)
{
	if (!enabled_ || !inFrame_) return;
	if (pureGpu) ++currentRuntimeCounters_.pureGpuEmitters; else ++currentRuntimeCounters_.hybridGpuEmitters;
	currentRuntimeCounters_.moduleOperations += moduleOperations;
	currentRuntimeCounters_.lutSamples += lutSamples;
	currentRuntimeCounters_.estimatedDescriptors += estimatedDescriptors;
}

std::string ParticleDiagnostics::GetScopeName(ParticleProfileScope scope) const
{
	switch (scope)
	{
	case ParticleProfileScope::ManagerUpdate: return "ManagerUpdate";
	case ParticleProfileScope::EmitterUpdateCpuPerCall: return "EmitterUpdateCpuPerCall";
	case ParticleProfileScope::EmitterUpdateCpuFrameTotal: return "EmitterUpdateCpuFrameTotal";
	case ParticleProfileScope::EmitterUpdateGpuPerCall: return "EmitterUpdateGpuPerCall";
	case ParticleProfileScope::EmitterUpdateGpuFrameTotal: return "EmitterUpdateGpuFrameTotal";
	case ParticleProfileScope::ReadbackCpuMapMemcpy: return "ReadbackCpuMapMemcpy";
	case ParticleProfileScope::ReadbackCopyCommandRecording: return "ReadbackCopyCommandRecording";
	case ParticleProfileScope::RemoveDeadParticles: return "RemoveDeadParticles";
	case ParticleProfileScope::UpdateGPUSpawns: return "UpdateGPUSpawns";
	case ParticleProfileScope::UploadCpuMapMemcpy: return "UploadCpuMapMemcpy";
	case ParticleProfileScope::UploadCommandRecording: return "UploadCommandRecording";
	case ParticleProfileScope::RendererUpdatePerCall: return "RendererUpdatePerCall";
	case ParticleProfileScope::RendererUpdateFrameTotal: return "RendererUpdateFrameTotal";
	case ParticleProfileScope::RendererDrawRecordingPerCall: return "RendererDrawRecordingPerCall";
	case ParticleProfileScope::RendererDrawRecordingFrameTotal: return "RendererDrawRecordingFrameTotal";
	default: return "Unknown";
	}
}

namespace
{
std::string EscapeCSV(const std::string& src)
{
	bool needsQuotes = false;
	if (src.find(',') != std::string::npos || 
		src.find('"') != std::string::npos || 
		src.find('\n') != std::string::npos || 
		src.find('\r') != std::string::npos)
	{
		needsQuotes = true;
	}

	if (!needsQuotes) return src;

	std::string escaped = "\"";
	for (char c : src)
	{
		if (c == '"')
		{
			escaped += "\"\""; // ダブルクォートをエスケープ
		}
		else
		{
			escaped += c;
		}
	}
	escaped += "\"";
	return escaped;
}
}

bool ParticleDiagnostics::ExportToCSV(const std::string& filepath, int preset, const std::string& modeStr, const std::string& buildStr, uint32_t emitterCount, uint32_t activeCount, const std::string& resultStatus)
{
	bool writeHeader = !std::filesystem::exists(filepath);
	std::ofstream file(filepath, std::ios::app);
	if (!file.is_open()) return false;

	if (writeHeader)
	{
		file << "preset,simulation_mode,build,resolution_width,resolution_height,resolution_source,vsync_state,fps_limiter_state,scope_name,last_ms,average_ms,min_ms,max_ms,sample_count,"
			 << "cpu_upload_memcpy_bytes_last_frame,cpu_upload_memcpy_bytes_average,"
			 << "gpu_upload_copy_bytes_last_frame,gpu_upload_copy_bytes_average,"
			 << "gpu_readback_copy_bytes_last_frame,gpu_readback_copy_bytes_average,"
			 << "cpu_readback_memcpy_bytes_last_frame,cpu_readback_memcpy_bytes_average,"
			 << "emitter_count,active_particle_count,active_particle_count_type,pure_gpu_emitters,hybrid_gpu_emitters,module_operations,lut_samples,estimated_descriptors,sampling_seconds,result_status,notes\n";
	}

	// 数値列は取得不能時に空欄とし、状態は resolution_source に記録する。
	std::string resWidth;
	std::string resHeight;
	std::string resSource = "unknown";
	std::string vsync = "unknown";
	std::string fpsLimiter = "unknown";

	// カウントタイプの選定
	std::string countType = "CPU_VECTOR_EXACT";
	if (modeStr == "GPU")
	{
		countType = lastRuntimeCounters_.pureGpuEmitters > 0 && lastRuntimeCounters_.hybridGpuEmitters == 0
			? "PURE_GPU_ASYNC_COMPLETION_RECORD" : "HYBRID_CPU_VECTOR_1F_DELAYED";
	}

	double samplingSec = 7.0; // 既定
	std::string notes = "";

	if (resultStatus == "SKIPPED_DESCRIPTOR_BUDGET")
	{
		notes = "Skipped execution due to descriptor budget limitations (SrvManager limit is 4096).";
	}

	for (int i = 0; i < static_cast<int>(ParticleProfileScope::Count); ++i)
	{
		auto scope = static_cast<ParticleProfileScope>(i);
		const auto& stat = stats_[i];

		file << preset << ","
			 << EscapeCSV(modeStr) << ","
			 << EscapeCSV(buildStr) << ","
			 << resWidth << ","
			 << resHeight << ","
			 << EscapeCSV(resSource) << ","
			 << EscapeCSV(vsync) << ","
			 << EscapeCSV(fpsLimiter) << ","
			 << EscapeCSV(GetScopeName(scope)) << ","
			 << stat.lastMs << ","
			 << stat.averageMs << ","
			 << stat.minMs << ","
			 << stat.maxMs << ","
			 << stat.sampleCount << ","
			 << cpuUploadMemcpyStats_.lastBytes << ","
			 << cpuUploadMemcpyStats_.averageBytes << ","
			 << gpuUploadCopyStats_.lastBytes << ","
			 << gpuUploadCopyStats_.averageBytes << ","
			 << gpuReadbackCopyStats_.lastBytes << ","
			 << gpuReadbackCopyStats_.averageBytes << ","
			 << cpuReadbackMemcpyStats_.lastBytes << ","
			 << cpuReadbackMemcpyStats_.averageBytes << ","
			 << emitterCount << ","
			 << activeCount << ","
			 << EscapeCSV(countType) << ","
			 << lastRuntimeCounters_.pureGpuEmitters << ","
			 << lastRuntimeCounters_.hybridGpuEmitters << ","
			 << lastRuntimeCounters_.moduleOperations << ","
			 << lastRuntimeCounters_.lutSamples << ","
			 << lastRuntimeCounters_.estimatedDescriptors << ","
			 << samplingSec << ","
			 << EscapeCSV(resultStatus) << ","
			 << EscapeCSV(notes) << "\n";
	}

	return true;
}

// ParticleScopeTimer
ParticleScopeTimer::ParticleScopeTimer(ParticleProfileScope scope)
	: scope_(scope)
{
	if (ParticleDiagnostics::GetInstance()->IsEnabled())
	{
		start_ = std::chrono::steady_clock::now();
	}
}

ParticleScopeTimer::~ParticleScopeTimer()
{
	if (ParticleDiagnostics::GetInstance()->IsEnabled())
	{
		auto end = std::chrono::steady_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start_;
		ParticleDiagnostics::GetInstance()->RecordTime(scope_, elapsed.count());
	}
}

} // namespace KCE
