#pragma once
#include "engine/scene/interface/ISceneState.h"
#include <vector>
#include <memory>
#include <string>
#include <cassert>
#include "graphics/3d/Object3d.h"

#ifdef USE_IMGUI
#include "manager/editor/DebugUIManager.h"
#endif

#include <d3d12.h>

#include "engine/scene/state/SceneStateMachine.h"

class SceneManager;

/**
 * @brief 各シーンの共通処理と状態フックを提供する基底クラス
 */
class BaseScene
{
public:
    BaseScene()
    {
        stateMachine_.SetOwner(this);
    }
    
    virtual ~BaseScene()
    {
#ifdef USE_IMGUI
        if (DebugUIManager::HasInstance())
        {
            DebugUIManager::GetInstance()->UnregisterDebugUI(this);
        }
#endif
    }

    //============================================
    // 純粋仮想関数・仮想関数
    //============================================

    /**
     * @brief 初期化。
     */
    virtual void Initialize() = 0;

    /**
     * @brief 終了処理（NVI）。
     */
    void Finalize()
    {
        OnFinalize();
    }

    /**
     * @brief 3D描画（登録されたオブジェクトを自動描画）。
     */
    virtual void Draw3D()
    {
        for (auto* object : objects_)
        {
            // Forwardパスでは、Forwardタイプのオブジェクトのみを描画
            if (object->GetRenderingType() == RenderingType::Forward)
            {
                object->Draw();
            }
        }
    }

    /**
     * @brief 2D描画。2Dは実装依存（スプライトなど）。
     */
    virtual void Draw2D() = 0;
    
    /**
     * @brief シャドウ描画。
     */
    virtual void DrawShadow()
    {
        for (auto* object : objects_)
        {
            if (object->GetCastShadow())
            {  // 影を落とすオブジェクトのみ
                object->DrawShadowOnly();
            }
        }
    }
    
    /**
     * @brief G-Buffer描画。
     */
    virtual void DrawGBuffer()
    {
        for (auto* object : objects_)
        {
            // G-Bufferパスでは、Deferredタイプのオブジェクトのみを描画
            if (object->GetRenderingType() == RenderingType::Deferred)
            {
                object->DrawGBuffer();
            }
        }
    }

    /**
     * @brief オブジェクトの登録（描画ループで自動的に処理されるようになる）
     * @param object 登録するオブジェクト
     */
    void RegisterObject(Object3d* object)
    {
        objects_.push_back(object);
    }

    /**
     * @brief オブジェクトリストのクリア
     */
    void ClearObjects()
    {
        objects_.clear();
    }

    //==========================================
    // 共通処理
    //==========================================

    /**
     * @brief 更新処理。
     */
    void Update()
    {
        // 共通更新処理
        CommonUpdate();

        // 状態別更新処理
        if (!isPaused_)
        {
            stateMachine_.Update();
        }
    }

    /**
     * @brief ImGuiの描画。
     */
    virtual void DrawImGui() {}

    /**
     * @brief 共通更新処理。
     */
    virtual void CommonUpdate() {}

    /**
     * @brief SceneManager の参照設定（非所有ポインタ）。
     * @param mgr シーンマネージャー
     */
    void SetSceneManager(SceneManager* mgr)
    {
        sceneManager_ = mgr;
    }

	SceneManager* GetSceneManager() const { return sceneManager_; }

    /**
     * @brief ステートマシンの参照取得。
     */
    SceneStateMachine& GetStateMachine() { return stateMachine_; }
    const SceneStateMachine& GetStateMachine() const { return stateMachine_; }

    /**
     * @brief ステートの事前登録。
     */
    void RegisterState(const std::string& name, std::unique_ptr<ISceneState> state)
    {
        stateMachine_.RegisterState(name, std::move(state));
    }

    /**
     * @brief 登録済みのステートに切り替える。
     * @param name 切り替え先のステート名
     */
    void ChangeState(const std::string& name)
    {
        stateMachine_.ChangeState(name);
    }

    /**
     * @brief 一時停止の要求。
     */
    void RequestPause()
    {
        isPaused_ = true;
    }

    /**
     * @brief 一時停止の解除（復帰）。
     */
    void RequestResume()
    {
        isPaused_ = false;
    }

    /**
     * @brief 一時停止中かどうかを取得。
     * @return 一時停止中なら true
     */
    bool IsPaused() const
    {
        return isPaused_;
    }

    /**
     * @brief 現在のステート名を取得（デバッグ用）。
     * @return ステート名の文字列参照
     */
    const std::string& GetCurrentStateName() const
    {
        return stateMachine_.GetCurrentStateName();
    }

protected:
    /**
     * @brief 派生クラスの後片付け用フック。
     */
    virtual void OnFinalize() {}

    // シーンマネージャー（非所有ポインタ）
    SceneManager* sceneManager_ = nullptr;
    // ステートマシン
    SceneStateMachine stateMachine_{this};

private:
    // ポーズ中フラグ
    bool isPaused_ = false;

    // 描画対象オブジェクトリスト
    std::vector<Object3d*> objects_;
};
