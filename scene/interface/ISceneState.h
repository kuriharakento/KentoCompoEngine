#pragma once
#include <string>

class BaseScene;

/**
 * @brief シーン内の1つの状態（ステート）を表す基底クラス。
 *
 * 各シーンが必要なステートだけ継承して作る。
 * BaseScene が unique_ptr で所有し、ChangeState で切り替える。
 */
class ISceneState
{
public:
    virtual ~ISceneState() = default;

    /**
     * @brief このステートに入った瞬間に呼ばれる。
     * @param scene 所属シーン
     */
    virtual void OnEnter(BaseScene& scene)
    {
    }

    /**
     * @brief 毎フレーム呼ばれる更新処理。
     * @param scene 所属シーン
     */
    virtual void OnUpdate(BaseScene& scene)
    {
    }

    /**
     * @brief ステート切り替えの条件判定（OnUpdate の直後に自動的に呼ばれる）。
     * @param scene 所属シーン
     */
    virtual void CheckTransition(BaseScene& scene)
    {
    }

    /**
     * @brief このステートから出る瞬間に呼ばれる。
     * @param scene 所属シーン
     */
    virtual void OnExit(BaseScene& scene)
    {
    }

    /**
     * @brief デバッグ表示用のステート名。
     * @return ステート名の文字列参照
     */
    virtual const std::string& GetName() const
    {
        static const std::string name = "Unknown";
        return name;
    }
};
