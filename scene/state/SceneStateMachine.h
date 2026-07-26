#pragma once
#include "engine/scene/interface/ISceneState.h"
#include "base/Logger.h"
#include <unordered_map>
#include <memory>
#include <string>

namespace KCE
{
class BaseScene;

/**
 * @brief 事前登録型・デストラクタ自動解放・所有者保持のシーン用ステートマシン
 */
class SceneStateMachine
{
public:
	explicit SceneStateMachine(BaseScene* owner = nullptr)
		: owner_(owner)
	{
	}

	~SceneStateMachine()
	{
		CleanUp();
	}

	// コピー禁止・移動許可
	SceneStateMachine(const SceneStateMachine&) = delete;
	SceneStateMachine& operator=(const SceneStateMachine&) = delete;
	SceneStateMachine(SceneStateMachine&&) noexcept = default;
	SceneStateMachine& operator=(SceneStateMachine&&) noexcept = default;

	/**
	 * @brief 所属シーン（所有者）の参照をセットする
	 */
	void SetOwner(BaseScene* owner)
	{
		owner_ = owner;
	}

	/**
	 * @brief ステートを事前登録する（同じ名前の二重登録は拒否）
	 * @param name ステート識別名
	 * @param state ステートの unique_ptr
	 * @return 登録成功なら true
	 */
	bool RegisterState(const std::string& name, std::unique_ptr<ISceneState> state)
	{
		if (!state)
		{
			Logger::Log("[SceneStateMachine Error] Attempted to register nullptr state: " + name + "\n");
			return false;
		}

		auto [it, inserted] = states_.emplace(name, std::move(state));
		if (!inserted)
		{
			Logger::Log("[SceneStateMachine Error] Duplicate state registration rejected: " + name + "\n");
			return false;
		}

		return true;
	}

	/**
	 * @brief 登録済みのステートに切り替える（引数はステート名だけ）
	 * @param name 切り替え先のステート識別名
	 */
	void ChangeState(const std::string& name)
	{
		auto it = states_.find(name);
		if (it == states_.end())
		{
			Logger::Log("[SceneStateMachine Error] State not found: " + name + "\n");
			return;
		}

		if (currentState_ && owner_)
		{
			currentState_->OnExit(*owner_);
		}

		currentState_ = it->second.get();

		if (currentState_ && owner_)
		{
			currentState_->OnEnter(*owner_);
		}
	}

	/**
	 * @brief 毎フレーム更新（引数なし・内部の owner_ を使用）
	 */
	void Update()
	{
		if (currentState_ && owner_)
		{
			currentState_->OnUpdate(*owner_);
			currentState_->CheckTransition(*owner_);
		}
	}

	/**
	 * @brief 現在のステート名を取得（ImGui用）
	 */
	const std::string& GetCurrentStateName() const
	{
		static const std::string emptyName = "None";
		return currentState_ ? currentState_->GetName() : emptyName;
	}

private:
	/**
	 * @brief デストラクタで自動実行される安全解体処理
	 */
	void CleanUp()
	{
		if (currentState_ && owner_)
		{
			currentState_->OnExit(*owner_);
			currentState_ = nullptr;
		}
		states_.clear();
	}

private:
	BaseScene* owner_ = nullptr; // 所属シーン（非所有ポインタ）
	std::unordered_map<std::string, std::unique_ptr<ISceneState>> states_;
	ISceneState* currentState_ = nullptr; // 現在のステート（非所有ポインタ）
};
} // namespace KCE
