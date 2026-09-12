#include "base/JobSystem.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <Windows.h>

#include "base/Logger.h"

namespace KCE
{
namespace
{
// メインスレッドの分はワーカーに数えない
constexpr uint32_t kMainThreadCount = 1;
// コア数が取れない・1コアでも最低これだけは立てる
constexpr uint32_t kMinWorkerCount = 1;
// 読み込みはファイル待ちもあるので、コアが多くてもこれ以上は増やさない
constexpr uint32_t kMaxDefaultWorkerCount = 8;
}

JobSystem::~JobSystem()
{
	Finalize();
}

void JobSystem::Initialize(uint32_t workerCount)
{
	if (!workers_.empty())
	{
		return;
	}

	if (workerCount == 0)
	{
		const uint32_t hardwareThreads = std::thread::hardware_concurrency();
		workerCount = hardwareThreads > kMainThreadCount ? hardwareThreads - kMainThreadCount : kMinWorkerCount;
		workerCount = (std::min)(workerCount, kMaxDefaultWorkerCount);
	}

	workers_.reserve(workerCount);
	for (uint32_t i = 0; i < workerCount; ++i)
	{
		workers_.emplace_back([this](std::stop_token stopToken) { WorkerLoop(stopToken); });
	}
}

void JobSystem::Finalize()
{
	for (auto& worker : workers_)
	{
		worker.request_stop();
	}
	// jthread は壊すときに join する。積まれてる仕事は WorkerLoop が片付けてから抜ける
	workers_.clear();
}

void JobSystem::Submit(std::function<void()> job)
{
	// ワーカーが無いならその場で済ませる
	if (workers_.empty())
	{
		RunJob(job);
		return;
	}

	{
		std::lock_guard lock(mutex_);
		jobs_.push_back(std::move(job));
		++unfinishedJobs_;
	}
	jobAvailable_.notify_one();
}

void JobSystem::WaitIdle()
{
	std::unique_lock lock(mutex_);
	idle_.wait(lock, [this]() { return unfinishedJobs_ == 0; });
}

void JobSystem::ParallelFor(size_t count, const std::function<void(size_t)>& body)
{
	if (count == 0)
	{
		return;
	}

	// 添字は取り合いにして、1個ずつの重さがばらついても偏らないようにする
	std::atomic<size_t> nextIndex = 0;
	const auto drain = [&]()
	{
		for (size_t index = nextIndex.fetch_add(1); index < count; index = nextIndex.fetch_add(1))
		{
			body(index);
		}
	};

	// 1個は自分で回すので、手伝いは count - 1 まで
	const size_t helperCount = (std::min)(workers_.size(), count - 1);
	std::mutex helperMutex;
	std::condition_variable helperDone;
	size_t runningHelpers = helperCount;
	for (size_t i = 0; i < helperCount; ++i)
	{
		Submit([&]()
		{
			RunJob(drain);
			std::lock_guard lock(helperMutex);
			--runningHelpers;
			helperDone.notify_one();
		});
	}

	RunJob(drain);

	// 手伝いはこの関数のローカル変数を触るので、全員抜けるまで戻らない
	std::unique_lock lock(helperMutex);
	helperDone.wait(lock, [&]() { return runningHelpers == 0; });
}

void JobSystem::WorkerLoop(std::stop_token stopToken)
{
	// WIC と Media Foundation が COM を使うので、ワーカーごとに MTA で入っておく
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	for (;;)
	{
		std::function<void()> job;
		{
			std::unique_lock lock(mutex_);
			jobAvailable_.wait(lock, stopToken, [this]() { return !jobs_.empty(); });
			if (jobs_.empty())
			{
				// 止めろと言われて、残りの仕事も無い
				break;
			}
			job = std::move(jobs_.front());
			jobs_.pop_front();
		}

		RunJob(job);

		{
			std::lock_guard lock(mutex_);
			--unfinishedJobs_;
			if (unfinishedJobs_ == 0)
			{
				idle_.notify_all();
			}
		}
	}

	if (SUCCEEDED(comResult))
	{
		CoUninitialize();
	}
}

void JobSystem::RunJob(const std::function<void()>& job)
{
	try
	{
		job();
	}
	catch (const std::exception& exception)
	{
		Logger::Log(std::string("ワーカーの仕事で例外が出たので捨てた: ") + exception.what() + "\n", Logger::LogLevel::Error);
	}
	catch (...)
	{
		Logger::Log("ワーカーの仕事で例外が出たので捨てた\n", Logger::LogLevel::Error);
	}
}
} // namespace KCE
