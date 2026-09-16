#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

namespace KCE
{
class DirectXCommon;

/**
 * @brief 描画パスごとの GPU 時間とドローコール数を測る
 *
 * - パスの前後にタイムスタンプを積み、フレームの最後にまとめて読み出し用のバッファへ移す
 * - PostDraw が毎フレーム GPU の完了を待つので、次のフレームの頭で前のフレームの結果を読める（ここでは待たない）
 * - サブビューのパイプラインは本編の SubViews パスの中で入れ子に回るので、深さを持たせて並べる
 * - 結果は「ビュー名＋パス名」ごとに覚えておき、24fps のモニターのように毎フレーム描かないビューでも表が崩れないようにする
 * - 毎フレームの確保はしない（固定長の配列に詰める）
 */
class RenderProfiler
{
public:
	enum class CpuSection : uint32_t
	{
		Frame,
		Update,
		FrameworkUpdate,
		SceneUpdate,
		RenderCommands,
		ImGui,
		ExecutePresent,
		GpuWait,
		FpsWait,
		Count
	};

	RenderProfiler() = default;
	~RenderProfiler();

	/**
	 * @brief クエリヒープと読み出し用のバッファを作る
	 * @param dxCommon 所有しない。Framework が持ち、このクラスより長生きする
	 */
	void Initialize(DirectXCommon* dxCommon);

	/** @brief 前のフレームの結果を読み、このフレームの計測を始める。パイプラインを回す前に呼ぶ */
	void BeginFrame();

	/** @brief このフレームのタイムスタンプを読み出し用のバッファへ移す。パイプラインを回した後、コマンドリストを閉じる前に呼ぶ */
	void EndFrame();

	/**
	 * @brief パスの計測を始める。EndPass と対で呼ぶ。入れ子にしてよい
	 * @param viewName 描いているビューの名前。中身は写し取るので、呼んだ後に消えてよい
	 * @param passName パスの名前。同上
	 */
	void BeginPass(const char* viewName, const char* passName);

	/** @brief 直近の BeginPass の計測を終える */
	void EndPass();

	/**
	 * @brief 前フレームのCPU計測を確定し、このフレームの計測を始める
	 * @param executePresentMs コマンド実行とPresentの時間
	 * @param gpuWaitMs GPU完了待ちの時間
	 * @param fpsWaitMs FPS固定待ちの時間
	 * @param postDrawTimingValid DirectXCommonの3区間を測れたか
	 */
	void BeginCpuFrame(float executePresentMs, float gpuWaitMs, float fpsWaitMs, bool postDrawTimingValid);

	/** @brief CPU区間の計測を始める */
	void BeginCpuSection(CpuSection section);

	/** @brief CPU区間の計測を終える */
	void EndCpuSection(CpuSection section);

	/** @brief Settingsのチェック状態を返す */
	bool IsEnabled() const { return enabled_; }

#ifdef USE_IMGUI
	/** @brief Settings のページ（レンダリング > 描画の計測）を登録する */
	void RegisterDebugUI();
	/** @brief 計測結果の表を描く */
	void DrawImGui();
#endif

private:
	// 1フレームに測れるパスの数。本編とサブビュー（モニター・反射）を合わせてこれに収まる想定
	static constexpr uint32_t kMaxSamples = 128;
	// パスの入れ子の深さの上限
	static constexpr uint32_t kMaxDepth = 8;
	// ビュー名・パス名を写し取る長さ（終端を含む）
	static constexpr size_t kNameLength = 32;
	// 先頭の2つはフレーム全体の始まりと終わり。パスはその後ろに始まりと終わりの2つずつ
	static constexpr uint32_t kFrameQueryCount = 2;
	static constexpr uint32_t kMaxQueries = kFrameQueryCount + kMaxSamples * 2;

	/** @brief このフレームで測っているパス1つ */
	struct Sample
	{
		char view[kNameLength] = {};
		char pass[kNameLength] = {};
		uint32_t depth = 0;
		// 始めたときのドローコールの数。終わりとの差がこのパスの回数
		uint64_t drawCallsAtBegin = 0;
		uint32_t drawCalls = 0;
		// 終わりのタイムスタンプを積んだか。積んでいないものは読まない
		bool ended = false;
	};

	/** @brief 表に出す結果1行。ビュー名＋パス名で引き当てて更新する */
	struct Result
	{
		char view[kNameLength] = {};
		char pass[kNameLength] = {};
		uint32_t depth = 0;
		float gpuMs = 0.0f;
		uint32_t drawCalls = 0;
		// 最後に測れた読み取り回数。しばらく出てこない行は消す
		uint64_t lastSeen = 0;
	};

	static constexpr uint32_t kCpuHistoryLength = 120;
	static constexpr uint32_t kCpuSectionCount = static_cast<uint32_t>(CpuSection::Count);

	struct CpuHistory
	{
		std::array<float, kCpuHistoryLength> samples{};
		uint32_t count = 0;
		uint32_t next = 0;
	};

	/** @brief 前のフレームのタイムスタンプを読んで、結果の表を更新する */
	void ReadResults();
	/** @brief クエリヒープなどが揃っていて測れるか */
	bool IsReady() const;
	void PushCpuSample(CpuSection section, float milliseconds);
	void GetCpuStats(CpuSection section, float& average, float& maximum) const;

	// 所有しない。Framework が持ち、このクラスより長生きする
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
	Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer_;
	// タイムスタンプの1秒あたりの刻み数
	uint64_t timestampFrequency_ = 0;

	std::array<Sample, kMaxSamples> samples_{};
	uint32_t sampleCount_ = 0;
	// 測っている途中のパスの番号（入れ子の分だけ積む）
	std::array<uint32_t, kMaxDepth> openStack_{};
	uint32_t openDepth_ = 0;
	// 深さの上限を超えて積まなかった分。EndPass でこちらから減らす
	uint32_t skippedDepth_ = 0;
	// このフレームを測っているか
	bool frameOpen_ = false;
	// 前のフレームで読み出し用に移したか。移していなければ読まない
	bool resolved_ = false;
	uint64_t frameDrawCallsAtBegin_ = 0;
	uint32_t frameDrawCalls_ = 0;

	std::array<Result, kMaxSamples> results_{};
	uint32_t resultCount_ = 0;
	// 結果を読んだ回数。行の古さを測るのに使う
	uint64_t readCount_ = 0;
	float frameGpuMs_ = 0.0f;
	uint32_t resultFrameDrawCalls_ = 0;
	std::array<CpuHistory, kCpuSectionCount> cpuHistory_{};
	std::array<float, kCpuSectionCount> cpuFrameMs_{};
	std::array<std::chrono::steady_clock::time_point, kCpuSectionCount> cpuSectionBegin_{};
	std::array<bool, kCpuSectionCount> cpuSectionOpen_{};
	std::chrono::steady_clock::time_point cpuFrameBegin_{};
	bool cpuCollecting_ = false;
	// 計測するか（Settings で切り替える）
	bool enabled_ = true;
};
} // namespace KCE
