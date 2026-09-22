#pragma once
#include <vector>
#include "gameobject/manager/GameObjectManager.h"
#include <memory>
#include <string>
#include <cassert>
#include "graphics/3d/Object3d.h"

#ifdef USE_IMGUI
#include "manager/editor/DebugUIManager.h"
#endif

#include <d3d12.h>

namespace KCE
{
class SceneManager;

/**
 * @brief 各シーンの共通処理を提供する基底クラス
 *
 * - 段階（演出 → プレイなど）を分けたいシーンは、StateMachine をメンバに持つ
 */
class BaseScene
{
public:
    BaseScene() = default;

    virtual ~BaseScene()
    {
#ifdef USE_IMGUI
        if (DebugUIManager::HasInstance())
        {
            DebugUIManager::GetInstance()->Unregister(this);
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
            if (object->GetRenderQueue() == RenderQueue::Opaque && object->GetRenderingType() == RenderingType::Forward)
            {
                object->Draw();
            }
        }
    }

    /**
     * @brief 2D描画。2Dは実装依存（スプライトなど）。
     */
    virtual void Draw2D() = 0;

	/** @brief 半透明描画をシーンから管理オブジェクトへ通す */
	virtual void DrawTransparent(CameraManager* camera)
	{
		GameObjectManager::GetInstance()->DrawTransparent(camera, objects_);
	}
    
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
            if (object->GetRenderQueue() == RenderQueue::Opaque && object->GetRenderingType() == RenderingType::Deferred)
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
        CommonUpdate();
    }

    /**
     * @brief ImGuiの描画。
     */
    virtual void DrawImGui() {}

    /**
     * @brief 保存していない変更があるか。シーンの切り替え・読み直しの前に確認を出すかに使う
     * @return 捨てると困る変更があれば真。既定は偽
     */
    virtual bool HasUnsavedChanges() const { return false; }

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

protected:
    /**
     * @brief 派生クラスの後片付け用フック。
     */
    virtual void OnFinalize() {}

    // シーンマネージャー（非所有ポインタ）
    SceneManager* sceneManager_ = nullptr;

private:
    // 描画対象オブジェクトリスト
    std::vector<Object3d*> objects_;
};
} // namespace KCE
