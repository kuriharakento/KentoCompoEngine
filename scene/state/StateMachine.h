#pragma once
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "base/Logger.h"
#include "engine/scene/state/SceneState.h"

namespace KCE
{
/**
 * @brief シーンが必要なときだけ持つステートマシン。
 * @tparam TOwner 持ち主のシーンの型
 * @tparam TStateId ステートを指す enum class
 *
 * - 切り替えは Request で予約して、次の Update の頭で反映する。
 *   OnUpdate の途中で切り替わって、新しいステートが同じフレームに動くのを防ぐ
 * - 同じフレームに何回 Request しても、最後の1回だけが効く
 * - シーンの OnFinalize で Stop を呼ぶこと。デストラクタでは OnExit を呼ばない
 *   （シーンのメンバが先に壊れていることがあるため）
 *
 * @code
 * // シーンのメンバ
 * KCE::StateMachine<TitleScene, TitleState> states_{ *this };
 * // Initialize
 * states_.Add<TitleIntroState>(TitleState::Intro);
 * states_.Start(TitleState::Intro);
 * // CommonUpdate
 * states_.Update();
 * // OnFinalize
 * states_.Stop();
 * @endcode
 */
template <typename TOwner, typename TStateId>
class StateMachine
{
	static_assert(std::is_enum_v<TStateId>, "TStateId は enum にする");

public:
	/**
	 * @brief 持ち主を覚える。
	 * @param owner 持ち主のシーン。StateMachine はこのシーンのメンバなので、先に死なない
	 */
	explicit StateMachine(TOwner& owner)
		: owner_(owner)
	{
	}

	StateMachine(const StateMachine&) = delete;
	StateMachine& operator=(const StateMachine&) = delete;

	/**
	 * @brief ステートを作って登録する。同じ ID の二重登録は拒否する。
	 * @tparam TState 登録するステートの型（SceneState<TOwner> の派生）
	 * @param id ステートの ID
	 * @param args TState のコンストラクタ引数
	 * @return 作ったステート（所有しない）。二重登録なら nullptr
	 */
	template <typename TState, typename... Args>
	TState* Add(TStateId id, Args&&... args)
	{
		static_assert(std::is_base_of_v<SceneState<TOwner>, TState>, "TState は SceneState<TOwner> を継承する");

		auto state = std::make_unique<TState>(std::forward<Args>(args)...);
		TState* raw = state.get();
		auto [it, inserted] = states_.emplace(id, std::move(state));
		if (!inserted)
		{
			Logger::Log("[StateMachine Error] Duplicate state id: " + std::to_string(ToInt(id)) + "\n");
			return nullptr;
		}
		return raw;
	}

	/**
	 * @brief 最初のステートにすぐ入る。Initialize で1回呼ぶ。
	 * @param id 入るステート
	 */
	void Start(TStateId id)
	{
		pending_.reset();
		Enter(id);
	}

	/**
	 * @brief ステートの切り替えを予約する。次の Update の頭で切り替わる。
	 * @param id 切り替え先。登録していない ID ならログを出して無視する
	 */
	void Request(TStateId id)
	{
		if (!states_.contains(id))
		{
			Logger::Log("[StateMachine Error] State not found: " + std::to_string(ToInt(id)) + "\n");
			return;
		}
		pending_ = id;
	}

	/**
	 * @brief 予約された切り替えを反映してから、今のステートの OnUpdate を呼ぶ。
	 */
	void Update()
	{
		if (pending_)
		{
			const TStateId next = *pending_;
			pending_.reset();
			Enter(next);
		}
		if (current_)
		{
			current_->OnUpdate(owner_);
		}
	}

	/**
	 * @brief 今のステートの OnExit を呼んで止める。シーンの OnFinalize で呼ぶ。
	 */
	void Stop()
	{
		pending_.reset();
		if (current_)
		{
			current_->OnExit(owner_);
			current_ = nullptr;
			currentId_.reset();
		}
	}

	/**
	 * @brief 今のステートの ID。
	 * @return 動いていなければ std::nullopt
	 */
	std::optional<TStateId> GetCurrentId() const { return currentId_; }

	/**
	 * @brief 今そのステートか。
	 * @param id 調べるステート
	 * @return 今そのステートなら true
	 */
	bool IsIn(TStateId id) const { return currentId_ && *currentId_ == id; }

private:
	static auto ToInt(TStateId id) { return static_cast<std::underlying_type_t<TStateId>>(id); }

	/** @brief 今のステートを抜けて、指定のステートに入る */
	void Enter(TStateId id)
	{
		auto it = states_.find(id);
		if (it == states_.end())
		{
			Logger::Log("[StateMachine Error] State not found: " + std::to_string(ToInt(id)) + "\n");
			return;
		}

		if (current_)
		{
			current_->OnExit(owner_);
		}
		current_ = it->second.get();
		currentId_ = id;
		current_->OnEnter(owner_);
	}

	// 持ち主のシーン。このシーンのメンバなので、寿命はシーンと同じ
	TOwner& owner_;
	// 登録したステート。増えるのは Initialize のときだけ
	std::unordered_map<TStateId, std::unique_ptr<SceneState<TOwner>>> states_;
	// 今のステート。所有は states_
	SceneState<TOwner>* current_ = nullptr;
	std::optional<TStateId> currentId_;
	// 次の Update で入るステート
	std::optional<TStateId> pending_;
};
} // namespace KCE
