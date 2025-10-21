#pragma once

/**
 * @file BaseScene.h
 * @brief シーン共通基底クラス
 */

 /// シーンの状態（ライフサイクル）
enum class SceneState
{
    Enter,      ///< シーン開始（フェードイン等）
    Intro,      ///< イントロ／スタート演出
    Playing,    ///< プレイ中
    Paused,     ///< ポーズ中
    Cutscene,   ///< カットシーン／特別演出
    End,        ///< 終了演出（クリア／失敗）
    Exit,       ///< シーン退場（フェードアウト等）
};

class SceneManager;

/**
 * @brief 各シーンの共通処理と状態フックを提供する基底クラス
 */
class BaseScene
{
public:
    BaseScene() = default;
    virtual ~BaseScene() = default;

	//============================================
	// 純粋仮想関数（必ずオーバーライドする）
	//============================================

    // 初期化 
    virtual void Initialize() = 0;
	// 終了
	virtual void Finalize() = 0;
	// 描画
    virtual void Draw3D() = 0;
    virtual void Draw2D() = 0;

	//==========================================
	// 共通処理
	//==========================================

    /**
	 * @brief 更新処理
     */
    void Update()
    {
		// 共通更新処理
		CommonUpdate();

		// 状態別更新処理
        switch (currentState_)
        {
        case SceneState::Enter:      OnUpdateEnter(); break;
        case SceneState::Intro:      OnUpdateIntro(); break;
        case SceneState::Playing:    OnUpdatePlaying(); break;
        case SceneState::Paused:     OnUpdatePaused(); break;
        case SceneState::Cutscene:   OnUpdateCutscene(); break;
        case SceneState::End:        OnUpdateEnd(); break;
        case SceneState::Exit:       OnUpdateExit(); break;
        default:                     break;
        }
    }

	// ImGuiの描画
    virtual void DrawImGui() {}

    // 共通更新処理
    virtual void CommonUpdate() {}

    // SceneManager の参照設定（非所有ポインタ）
    void SetSceneManager(SceneManager* mgr) { sceneManager_ = mgr; }

    // 現在の状態を取得（デバッグ、条件分岐用）
    SceneState GetCurrentState() const { return currentState_; }

    // --- Pause / Resume の簡易 API ---
    // 外部からシーンを一時停止 / 復帰させたい場合に使う
    void RequestPause()
    {
        if (currentState_ == SceneState::Paused) return;
        previousState_ = currentState_;
        ChangeState(SceneState::Paused);
    }

    void RequestResume()
    {
        if (currentState_ != SceneState::Paused) return;
        // previousState_ が未初期化の場合は Playing に戻す
        ChangeState(previousState_ == SceneState::Paused ? SceneState::Playing : previousState_);
    }

protected:
    /**
	 * \brief 状態の変更
	 * \param next つぎの状態
     */
    void ChangeState(SceneState next)
    {
        if (next == currentState_) return;

        // 現在状態の Exit
        switch (currentState_)
        {
        case SceneState::Enter:      OnExitEnter(); break;
        case SceneState::Intro:      OnExitIntro(); break;
        case SceneState::Playing:    OnExitPlaying(); break;
        case SceneState::Paused:     OnExitPaused(); break;
        case SceneState::Cutscene:   OnExitCutscene(); break;
        case SceneState::End:        OnExitEnd(); break;
        case SceneState::Exit:       OnExitExit(); break;
        default: break;
        }

        SceneState prev = currentState_;
        currentState_ = next;

        // 次状態の Enter
        switch (currentState_)
        {
        case SceneState::Enter:      OnEnterEnter(); break;
        case SceneState::Intro:      OnEnterIntro(); break;
        case SceneState::Playing:    OnEnterPlaying(); break;
        case SceneState::Paused:     OnEnterPaused(); break;
        case SceneState::Cutscene:   OnEnterCutscene(); break;
        case SceneState::End:        OnEnterEnd(); break;
        case SceneState::Exit:       OnEnterExit(); break;
        default: break;
        }
    }

    //==========================================
    // フック（必要なものだけオーバーライドする）
	//==========================================

    // Enter（フェードインなど）
    virtual void OnEnterEnter() {}
    virtual void OnUpdateEnter() {}
    virtual void OnExitEnter() {}

    // Intro（カメライントロ等）
    virtual void OnEnterIntro() {}
    virtual void OnUpdateIntro() {}
    virtual void OnExitIntro() {}

    // Playing（プレイ中）
    virtual void OnEnterPlaying() {}
    virtual void OnUpdatePlaying() {}
    virtual void OnExitPlaying() {}

    // Paused（ポーズ）
    virtual void OnEnterPaused() {}
    virtual void OnUpdatePaused() {}
    virtual void OnExitPaused() {}

    // Cutscene（カットシーン）
    virtual void OnEnterCutscene() {}
    virtual void OnUpdateCutscene() {}
    virtual void OnExitCutscene() {}

    // End（終了演出）
    virtual void OnEnterEnd() {}
    virtual void OnUpdateEnd() {}
    virtual void OnExitEnd() {}

    // Exit（フェードアウト等）
    virtual void OnEnterExit() {}
    virtual void OnUpdateExit() {}
    virtual void OnExitExit() {}

protected:
	// シーンマネージャー（非所有ポインタ）
    SceneManager* sceneManager_ = nullptr;
	// 現在のシーン状態
    SceneState currentState_ = SceneState::Enter; // 初期状態は Enter
	// 前のシーン状態（Pause 復帰用）
    SceneState previousState_ = SceneState::Playing;
};