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

class SceneManager;

/**
 * @brief 各シーンの共通処理と状態フックを提供する基底クラス
 */
class BaseScene
{
public:
    BaseScene() = default;
    
    virtual ~BaseScene()
    {
        // Finalize() で OnExit 済みのはず。未呼び出しを検知する
        assert(!currentState_ && "Finalize() が呼ばれずにデストラクタに到達した");
        
        currentState_.reset();

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
     *
     * non-virtual — BaseScene が実行順序を制御する。
     * 1. ステートの OnExit（派生メンバがまだ生きている状態で実行）
     * 2. 派生クラスの後片付け（OnFinalize）
     */
    void Finalize()
    {
        // 1. ステートの後片付け
        if (currentState_)
        {
            currentState_->OnExit(*this);
            currentState_.reset();
        }

        // 2. 派生クラスの後片付け
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
        if (!isPaused_ && currentState_)
        {
            currentState_->OnUpdate(*this);
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

    /**
     * @brief ステートを切り替える。旧ステートの OnExit → 新ステートの OnEnter を自動で呼ぶ。
     * @param next 次のステート（所有権を移動）
     */
    void ChangeState(std::unique_ptr<ISceneState> next)
    {
        // 再帰呼び出しガード
        assert(!isChangingState_ && "ChangeState の再帰呼び出しは禁止");
        isChangingState_ = true;

        if (currentState_)
        {
            currentState_->OnExit(*this);
        }
        currentState_ = std::move(next);
        if (currentState_)
        {
            currentState_->OnEnter(*this);
        }

        isChangingState_ = false;
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
        static const std::string noneName = "None";
        return currentState_ ? currentState_->GetName() : noneName;
    }

protected:
    /**
     * @brief 派生クラスの後片付け用フック。
     */
    virtual void OnFinalize() {}

    // シーンマネージャー（非所有ポインタ）
    SceneManager* sceneManager_ = nullptr;

private:
    // 現在のステート
    std::unique_ptr<ISceneState> currentState_;
    // ポーズ中フラグ
    bool isPaused_ = false;
    // ChangeState 再帰ガード
    bool isChangingState_ = false;

    // 描画対象オブジェクトリスト
    std::vector<Object3d*> objects_;
};
