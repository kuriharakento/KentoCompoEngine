#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace KCE
{
/**
 * @brief 読み込みみたいな重い仕事をワーカースレッドに回す。
 *
 * - ワーカーは起動時に COM を MTA で初期化する（WIC や Media Foundation を使うため）
 * - 仕事の中では D3D12 のコマンドリストや、スレッドを考えてないマネージャーを触らない
 * - Submit / WaitIdle / ParallelFor を呼ぶのはメインスレッドだけの想定
 */
class JobSystem
{
public:
	JobSystem() = default;
	JobSystem(const JobSystem&) = delete;
	JobSystem& operator=(const JobSystem&) = delete;

	/** @brief 残ってる仕事を片付けてからワーカーを止める */
	~JobSystem();

	/**
	 * @brief ワーカーを立ち上げる。
	 * @param workerCount ワーカー数。0 ならコア数からメインスレッドの分を引いた数にする
	 */
	void Initialize(uint32_t workerCount = 0);

	/** @brief 積まれてる仕事を全部片付けてからワーカーを止める。何度呼んでもいい */
	void Finalize();

	/**
	 * @brief 仕事を1つ投げる。終わりを待つなら WaitIdle を呼ぶ。
	 * @param job 別スレッドで動く処理。例外は投げない前提（投げたらログに出して捨てる）
	 */
	void Submit(std::function<void()> job);

	/** @brief 投げた仕事が全部終わるまで待つ */
	void WaitIdle();

	/**
	 * @brief body(0)〜body(count-1) を手分けして回し、全部終わるまで待つ。
	 * @details 呼んだスレッドも手伝うので、ワーカーが Submit の仕事でふさがってても先に進む。
	 *          ただし戻るのは、手伝いに出した仕事がワーカーで動き終わってから。
	 *          ワーカーが無いときはその場で順番に回す。
	 * @param count 回す回数
	 * @param body 添字を受け取る処理。別スレッドから同時に呼ばれるので、添字ごとに別のデータだけ触ること
	 */
	void ParallelFor(size_t count, const std::function<void(size_t)>& body);

	/** @brief ワーカー数を取得する */
	uint32_t GetWorkerCount() const { return static_cast<uint32_t>(workers_.size()); }

private:
	/** @brief ワーカー1本分のループ。止めろと言われても、積まれてる仕事は片付けてから抜ける */
	void WorkerLoop(std::stop_token stopToken);

	/** @brief 仕事を1つ動かす。例外が漏れて待ちが終わらなくならないよう、ここで受け止める */
	static void RunJob(const std::function<void()>& job);

	std::vector<std::jthread> workers_;
	// 積まれてる仕事（mutex_ で守る）
	std::deque<std::function<void()>> jobs_;
	std::mutex mutex_;
	// 仕事が来たときにワーカーを起こす。stop_token で止めるときも起きる
	std::condition_variable_any jobAvailable_;
	// 実行中も含めて全部終わったときに WaitIdle を起こす
	std::condition_variable idle_;
	// 積まれてる分と実行中の分の合計（mutex_ で守る）
	size_t unfinishedJobs_ = 0;
};
} // namespace KCE
